#include "FlashlightMesh.h"

#include "Config.h"
#include "Utils.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

namespace
{
    constexpr const char* HAND_POSE_TAG = "ImFl_Hold";

    /** Returns true when the initialized FRIK API supports v4 hand-pose features. */
    bool isFrikApiV4()
    {
        return frik::api::FRIKApi::inst && frik::api::FRIKApi::inst->getVersion() >= 4;
    }

    /** Maps an Immersive Flashlight location to the matching FRIK hand. */
    frik::api::FRIKApi::Hand getFrikHand(const ImFl::FlashlightLocation location)
    {
        return location == ImFl::FlashlightLocation::InOffhand
            ? frik::api::FRIKApi::Hand::Offhand
            : frik::api::FRIKApi::Hand::Primary;
    }

    /**
     * Returns true when the flashlight location should have a hand mesh.
     */
    bool isMeshLocation(const ImFl::FlashlightLocation location)
    {
        return location == ImFl::FlashlightLocation::InOffhand
            || location == ImFl::FlashlightLocation::InPrimaryHand;
    }

    /**
     * Applies the configured flashlight holding pose through the FRIK API.
     */
    bool setFlashlightHandPose(const frik::api::FRIKApi::Hand hand)
    {
        const frik::api::FRIKApi::HandPoseData pose = {
            .thumb = { .prox = ImFl::g_config.flashlightHandPoseThumb, .mid = ImFl::g_config.flashlightHandPoseThumb, .dist = ImFl::g_config.flashlightHandPoseThumb },
            .index = { .prox = ImFl::g_config.flashlightHandPoseIndex, .mid = ImFl::g_config.flashlightHandPoseIndex, .dist = ImFl::g_config.flashlightHandPoseIndex },
            .middle = { .prox = ImFl::g_config.flashlightHandPoseMiddle, .mid = ImFl::g_config.flashlightHandPoseMiddle, .dist = ImFl::g_config.flashlightHandPoseMiddle },
            .ring = { .prox = ImFl::g_config.flashlightHandPoseRing, .mid = ImFl::g_config.flashlightHandPoseRing, .dist = ImFl::g_config.flashlightHandPoseRing },
            .pinky = { .prox = ImFl::g_config.flashlightHandPosePinky, .mid = ImFl::g_config.flashlightHandPosePinky, .dist = ImFl::g_config.flashlightHandPosePinky },
        };
        return isFrikApiV4() && frik::api::FRIKApi::inst->setHandPoseCustom(HAND_POSE_TAG, hand, pose, false);
    }

    /** Returns true when FRIK is actively using our hand-pose tag. */
    frik::api::FRIKApi::HandPoseTagState getFlashlightHandPoseState(const frik::api::FRIKApi::Hand hand)
    {
        if (!isFrikApiV4()) {
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

        if (f4vr::isInPowerArmor()) {
            hide(true);
            return;
        }

        if (!isFlashlightOn || !isMeshLocation(Utils::flashlightLocation)) {
            hide(true);
            return;
        }

        const auto parent = resolveParentNode();
        if (!parent) {
            hide(true);
            return;
        }

        // re-attach if the skeleton pointer changed (e.g. after PA transition) or location changed
        if (_attachedTo && (parent != _attachedTo || Utils::flashlightLocation != _attachedForLocation)) {
            detach();
        }

        const auto handPoseLocation = Utils::flashlightLocation;
        if (_handPoseSet && _handPoseSetForLocation != handPoseLocation) {
            clearHandPose();
        }

        const auto hand = getFrikHand(handPoseLocation);
        auto handPoseState = _handPoseSet ? getFlashlightHandPoseState(hand) : frik::api::FRIKApi::HandPoseTagState::None;
        if (handPoseState == frik::api::FRIKApi::HandPoseTagState::None) {
            _handPoseSet = false;
        }

        if (!_handPoseSet && setFlashlightHandPose(hand)) {
            _handPoseSet = true;
            _handPoseSetForLocation = handPoseLocation;
            handPoseState = getFlashlightHandPoseState(hand);
        }

        if (handPoseState != frik::api::FRIKApi::HandPoseTagState::Active) {
            hide(false);
            return;
        }

        if (!_attachedTo) {
            attach(parent);
        }

        show();
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
            logger::info("FlashlightMesh: cloned NIF for location {}", static_cast<int>(Utils::flashlightLocation));
        } else {
            logger::info("FlashlightMesh: re-attaching cached node for location {}", static_cast<int>(Utils::flashlightLocation));
        }

        parentNode->AttachChild(_meshNode.get(), true);

        _attachedTo = parentNode;
        _attachedForLocation = Utils::flashlightLocation;

        setMeshTransform(g_config.flashlightMeshTransform);
    }

    /**
     * Set the mesh node's local transform based on the provided config transform, applying location-specific adjustments.
     */
    void FlashlightMesh::setMeshTransform(const RE::NiTransform& transform) const
    {
        const float sign = Utils::flashlightLocation == FlashlightLocation::InOffhand ? -1.0f : 1.0f;
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
        }
    }

    /**
     * Resolves the skeleton hand node for the current mesh-capable flashlight location.
     */
    RE::NiNode* FlashlightMesh::resolveParentNode()
    {
        if (Utils::flashlightLocation == FlashlightLocation::InOffhand) {
            return f4vr::isLeftHandedMode()
                ? f4vr::findNode(f4vr::getCommonNode(), "RArm_Hand")
                : f4vr::findNode(f4vr::getCommonNode(), "LArm_Hand");
        }
        if (Utils::flashlightLocation == FlashlightLocation::InPrimaryHand) {
            return !f4vr::isLeftHandedMode()
                ? f4vr::findNode(f4vr::getCommonNode(), "RArm_Hand")
                : f4vr::findNode(f4vr::getCommonNode(), "LArm_Hand");
        }
        return nullptr;
    }
}
