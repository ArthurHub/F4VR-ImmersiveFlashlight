#include "FlashlightMesh.h"

#include "Config.h"
#include "FlashlightState.h"
#include "api/FRIKApi.h"
#include "api/FRIKApiV2.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

namespace
{
    constexpr const char* HAND_POSE_TAG = "ImFl_Hold";

    /** Returns true when both FRIK API tables used by the hand pose are initialized (v2 provides the full pose override). */
    bool isFrikHandPoseApiReady()
    {
        return frik::api::FRIKApi::inst && frik::api::FRIKApiV2::inst;
    }

    /** Maps an Immersive Flashlight location to the matching FRIK hand. */
    frik::api::FRIKApi::Hand getFrikHand(const ImFl::FlashlightLocation location)
    {
        return location == ImFl::FlashlightLocation::InOffhand ? frik::api::FRIKApi::Hand::Offhand : frik::api::FRIKApi::Hand::Primary;
    }

    /**
     * Returns true when the flashlight location should have a hand mesh.
     */
    bool isMeshLocation(const ImFl::FlashlightLocation location)
    {
        return location == ImFl::FlashlightLocation::InOffhand || location == ImFl::FlashlightLocation::InPrimaryHand;
    }

    /**
     * Picks the hand pose matching the active grip style and power-armor state.
     */
    const frik::api::FRIKApi::HandPoseData& activeHandPose()
    {
        return ImFl::g_config.getFlashlightHandPose(ImFl::FlashlightState::flashlightGripStyle, f4vr::isInPowerArmor());
    }

    /**
     * Picks the mesh transform matching the active grip style and power-armor state.
     */
    const RE::NiTransform& activeMeshTransform()
    {
        return ImFl::g_config.getFlashlightMeshTransform(ImFl::FlashlightState::flashlightGripStyle, f4vr::isInPowerArmor());
    }

    /**
     * Applies the flashlight hand pose for the active grip style through the FRIK API.
     */
    bool setFlashlightHandPose(const frik::api::FRIKApi::Hand hand)
    {
        if (!isFrikHandPoseApiReady()) {
            return false;
        }
        // v1 and v2 declare Hand and HandPoseData with the same values and layout, so the configured pose carries over as-is.
        return frik::api::FRIKApiV2::inst->setHandPoseCustom(HAND_POSE_TAG,
            static_cast<frik::api::FRIKApiV2::Hand>(hand),
            frik::api::FRIKApiV2::HandPoseData::fromFloats(activeHandPose().asFloatView()),
            frik::api::FRIKApiV2::HAND_POSE_PRIORITY_DEFAULT);
    }

    /** Returns true when FRIK is actively using our hand-pose tag. */
    frik::api::FRIKApi::HandPoseTagState getFlashlightHandPoseState(const frik::api::FRIKApi::Hand hand)
    {
        if (!isFrikHandPoseApiReady()) {
            return frik::api::FRIKApi::HandPoseTagState::None;
        }
        return frik::api::FRIKApi::inst->getHandPoseSetTagState(HAND_POSE_TAG, hand);
    }
}

namespace ImFl
{
    /** Updates mesh attachment, visibility, and FRIK hand-pose state for the current frame. */
    void FlashlightMesh::onFrameUpdate(const bool isFlashlightOn)
    {
        if (!g_config.showFlashlightMesh) {
            hide(true);
            return;
        }

        if (!isFlashlightOn || !isMeshLocation(FlashlightState::flashlightLocation)) {
            hide(true);
            return;
        }

        const auto parent = resolveParentNode();
        if (!parent) {
            hide(true);
            return;
        }

        // re-attach if the skeleton pointer changed (e.g. after PA transition) or location changed
        if (_attachedTo && (parent != _attachedTo || FlashlightState::flashlightLocation != _attachedForLocation)) {
            detach();
        }

        // grip style changes don't require re-attach (same parent), just re-apply transform.
        if (_attachedTo && _attachedForGripStyle != FlashlightState::flashlightGripStyle) {
            setMeshTransform();
            _attachedForGripStyle = FlashlightState::flashlightGripStyle;
        }

        const auto handPoseLocation = FlashlightState::flashlightLocation;

        // A location change moves to a different FRIK hand, so the pose set on the old hand must be cleared.
        // A grip-style change keeps the same hand and is handled by refreshing the pose in place below.
        if (_handPoseSet && _handPoseSetForLocation != handPoseLocation) {
            clearHandPose();
        }

        const auto hand = getFrikHand(handPoseLocation);
        auto handPoseState = _handPoseSet ? getFlashlightHandPoseState(hand) : frik::api::FRIKApi::HandPoseTagState::None;
        if (handPoseState == frik::api::FRIKApi::HandPoseTagState::None) {
            _handPoseSet = false;
        }

        // Apply the pose when first setting it, or refresh its values in place when the grip style changes.
        // Refreshing an existing tag at the default priority keeps its position in FRIK's override stack, so a
        // system currently overriding our tag stays on top and our updated pose only takes effect once it releases.
        if (!_handPoseSet || _handPoseSetForGripStyle != FlashlightState::flashlightGripStyle) {
            if (setFlashlightHandPose(hand)) {
                _handPoseSet = true;
                _handPoseSetForLocation = handPoseLocation;
                _handPoseSetForGripStyle = FlashlightState::flashlightGripStyle;
                handPoseState = getFlashlightHandPoseState(hand);
            }
        }

        if (handPoseState != frik::api::FRIKApi::HandPoseTagState::Active) {
            hide(false);
            return;
        }

        if (!_attachedTo) {
            attach(parent);
        }

        show();

        setMeshTransform();
    }

    /** Forces the cached mesh to detach so it can reattach to fresh skeleton nodes later. */
    void FlashlightMesh::invalidate()
    {
        detach();
    }

    /** Attaches the cached flashlight mesh to the requested parent node, cloning it if needed. */
    void FlashlightMesh::attach(RE::NiNode* parentNode)
    {
        if (!_meshNode) {
            _meshNode.reset(f4vr::getClonedNiNodeForNifFileSetName(NIF_PATH, MESH_NODE_NAME));
            logger::info("FlashlightMesh: cloned NIF for location {}", static_cast<int>(FlashlightState::flashlightLocation));
        } else {
            logger::info("FlashlightMesh: re-attaching cached node for location {}", static_cast<int>(FlashlightState::flashlightLocation));
        }

        parentNode->AttachChild(_meshNode.get(), true);

        _attachedTo = parentNode;
        _attachedForLocation = FlashlightState::flashlightLocation;
        _attachedForGripStyle = FlashlightState::flashlightGripStyle;

        setMeshTransform();
    }

    /**
     * Set the mesh node's local transform based on the provided config transform, mirroring it (Z translate
     * + heading) when it lands on the left-hand bone so one authored value reads for both hands.
     *
     * The transform is authored for the right-hand bone (RArm_Hand); resolveParentNode() attaches to that
     * bone for the primary hand out of left-handed mode and for the offhand in left-handed mode. The other
     * cases land on the mirror-image left-hand bone (LArm_Hand) — the offhand out of left-handed mode, or
     * the primary hand in it — so mirror exactly when location-is-offhand and left-handed-mode disagree.
     */
    void FlashlightMesh::setMeshTransform() const
    {
        if (!_meshNode) {
            return;
        }

        f4vr::updateTransformsDown(_meshNode.get(), true);

        const auto transform = activeMeshTransform();

        const bool leftHandBone = (FlashlightState::flashlightLocation == FlashlightLocation::InOffhand) != f4vr::isLeftHandedMode();
        const float sign = leftHandBone ? -1.0f : 1.0f;
        _meshNode->local.translate = RE::NiPoint3(transform.translate.x, transform.translate.y, sign * transform.translate.z);

        float heading, roll, attitude;
        common::MatrixUtils::getEulerAnglesFromMatrixDegrees(transform.rotate, &heading, &roll, &attitude);
        _meshNode->local.rotate = common::MatrixUtils::getMatrixFromEulerAnglesDegrees(sign * heading, roll, attitude);
        _meshNode->local.scale = transform.scale;
    }

    /** Detaches the mesh from its current parent while keeping the cloned node cached. */
    void FlashlightMesh::detach()
    {
        if (!_attachedTo) {
            return;
        }

        if (_meshNode && _meshNode->parent) {
            logger::debug<>("FlashlightMesh: detached");
            RE::NiPointer<RE::NiAVObject> held;
            _meshNode->parent->DetachChild(_meshNode.get(), held);
            // held goes out of scope; _meshNode keeps the clone alive
        }

        clearHandPose();

        _attachedTo = nullptr;
        _attachedForLocation = FlashlightLocation::OnHead;
    }

    /**
     * Hides the cached mesh and optionally clears this mod's FRIK hand-pose tag.
     */
    void FlashlightMesh::hide(const bool clearPose) const
    {
        if (_meshNode && f4vr::isNodeVisible(_meshNode.get())) {
            f4vr::setNodeVisibility(_meshNode.get(), false);
            logger::info("FlashlightMesh: hidden");
        }
        if (clearPose) {
            clearHandPose();
        }
    }

    /**
     * Shows the cached mesh if it has already been cloned.
     */
    void FlashlightMesh::show() const
    {
        if (_meshNode && !f4vr::isNodeVisible(_meshNode.get())) {
            f4vr::updateTransformsDown(_meshNode.get(), true);
            f4vr::setNodeVisibility(_meshNode.get(), true);
            logger::info("FlashlightMesh: shown");
        }
    }

    /**
     * Clears this mod's FRIK hand-pose tag for the currently attached location.
     */
    void FlashlightMesh::clearHandPose() const
    {
        if (_handPoseSet && frik::api::FRIKApi::inst && isMeshLocation(_handPoseSetForLocation)) {
            frik::api::FRIKApi::inst->clearHandPose(HAND_POSE_TAG, getFrikHand(_handPoseSetForLocation));
            _handPoseSet = false;
            _handPoseSetForLocation = FlashlightLocation::OnHead;
            _handPoseSetForGripStyle = FlashlightGripStyle::Forward;
        }
    }

    /**
     * Resolves the skeleton hand node for the current mesh-capable flashlight location.
     */
    RE::NiNode* FlashlightMesh::resolveParentNode()
    {
        if (FlashlightState::flashlightLocation == FlashlightLocation::InOffhand) {
            return f4vr::isLeftHandedMode() ? f4vr::findNode(f4vr::getCommonNode(), "RArm_Hand") : f4vr::findNode(f4vr::getCommonNode(), "LArm_Hand");
        }
        if (FlashlightState::flashlightLocation == FlashlightLocation::InPrimaryHand) {
            return !f4vr::isLeftHandedMode() ? f4vr::findNode(f4vr::getCommonNode(), "RArm_Hand") : f4vr::findNode(f4vr::getCommonNode(), "LArm_Hand");
        }
        return nullptr;
    }
}
