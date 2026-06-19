#include "BodyFlashlightMesh.h"

#include "Config.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRSkelly.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    // The body bone the stowed model attaches to.
    constexpr const char* STOW_BONE_NAME = f4vr::SkellyBones::Chest.data();

    /**
     * Attaches/detaches the stowed model to keep it in sync with the feature being enabled. Re-attaches
     * when the stow bone or power-armor state changes, then re-applies the (mirrored / PA) model transform
     * each frame so INI live-reload is reflected immediately.
     */
    void BodyFlashlightMesh::onFrameUpdate(const bool enabled)
    {
        if (!enabled) {
            detach();
            return;
        }

        const auto root = f4vr::getRootNode();
        const auto parent = root ? f4vr::findNode(root, STOW_BONE_NAME) : nullptr;
        if (!parent) {
            detach();
            return;
        }

        const bool inPA = f4vr::isInPowerArmor();
        if (_attachedTo && (parent != _attachedTo || _attachedInPA != inPA)) {
            detach();
        }

        if (!_attachedTo) {
            attach(parent, inPA);
        }
    }

    /**
     * Shows or hides the stowed model, skipping the call when it is already in the requested state.
     */
    void BodyFlashlightMesh::setVisible(const bool visible) const
    {
        if (_meshNode && f4vr::isNodeVisible(_meshNode.get()) != visible) {
            f4vr::setNodeVisibility(_meshNode.get(), visible);
        }
    }

    /** Forces the cached nodes to detach so they reattach to fresh skeleton nodes later. */
    void BodyFlashlightMesh::invalidate()
    {
        detach();
    }

    /** Clones the model on first use (hiding its beam + collision) and attaches it to the body bone. */
    void BodyFlashlightMesh::attach(RE::NiNode* parentNode, const bool inPowerArmor)
    {
        if (!_meshNode) {
            _meshNode.reset(f4vr::getClonedNiNodeForNifFileSetName(NIF_PATH, MESH_NODE_NAME));

            if (!_meshNode) {
                logger::warn("BodyFlashlightMesh: failed to clone NIF '{}'", NIF_PATH);
                return;
            }
            // Passive prop: no beam glow, and no collision so it can't disturb furniture physics
            // (the VirtualHolsters pattern for body-attached meshes).
            _meshNode->collisionObject.reset();
            hideBeamNode();
            logger::info("BodyFlashlightMesh: cloned model NIF");
        }

        parentNode->AttachChild(_meshNode.get(), true);
        _attachedTo = parentNode;
        _attachedInPA = inPowerArmor;

        applyMirroredTransform(_meshNode.get(), g_config.getFlashlightBodyTransform(inPowerArmor));
        logger::info("BodyFlashlightMesh: attached to '{}'{}", STOW_BONE_NAME, inPowerArmor ? " (PA)" : "");
    }

    /** Detaches the cached nodes from their bone while keeping the clones cached. */
    void BodyFlashlightMesh::detach()
    {
        if (!_attachedTo) {
            return;
        }

        if (_meshNode && _meshNode->parent) {
            RE::NiPointer<RE::NiAVObject> held;
            _meshNode->parent->DetachChild(_meshNode.get(), held);
            // held goes out of scope; the NiPointer member keeps the clone alive
        }

        _attachedTo = nullptr;
        logger::debug<>("BodyFlashlightMesh: detached");
    }

    /**
     * Sets node->local from a config transform authored for a right-handed player, mirroring the Z
     * translate and heading/roll when the player is left-handed so one configured value reads correctly
     * for both handedness modes.
     */
    void BodyFlashlightMesh::applyMirroredTransform(RE::NiNode* node, const RE::NiTransform& transform)
    {
        const float sign = f4vr::isLeftHandedMode() ? -1.0f : 1.0f;

        node->local.translate = RE::NiPoint3(transform.translate.x, transform.translate.y, sign * transform.translate.z);

        float heading, roll, attitude;
        common::MatrixUtils::getEulerAnglesFromMatrixDegrees(transform.rotate, &heading, &roll, &attitude);
        node->local.rotate = common::MatrixUtils::getMatrixFromEulerAnglesDegrees(sign * heading, sign * roll, attitude);
        node->local.scale = transform.scale;
    }

    /**
     * The grab-zone transform expressed in stow-bone space with its origin moved to the stowed model.
     */
    RE::NiTransform BodyFlashlightMesh::grabZoneTransform() const
    {
        RE::NiTransform orb = g_config.getFlashlightGrabSphereTransform(_attachedInPA);
        orb.translate += g_config.getFlashlightBodyTransform(_attachedInPA).translate;
        return orb;
    }

    /**
     * Hides the lamp-glow FX node inside the cloned model so the stowed flashlight emits no glow.
     */
    void BodyFlashlightMesh::hideBeamNode() const
    {
        if (const auto beam = f4vr::findNode(_meshNode.get(), BEAM_NODE_NAME)) {
            f4vr::setNodeVisibility(beam, false);
            logger::info("BodyFlashlightMesh: hid beam node '{}'", BEAM_NODE_NAME);
        } else {
            logger::warn("BodyFlashlightMesh: beam node '{}' not found in model", BEAM_NODE_NAME);
        }
    }
}
