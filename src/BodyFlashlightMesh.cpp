#include "BodyFlashlightMesh.h"

#include "Config.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRSkelly.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    // The body bone the stowed model is placed at.
    constexpr const char* STOW_BONE_NAME = f4vr::SkellyBones::Chest.data();

    /**
     * Attaches/detaches the stowed model to keep it in sync with the feature being enabled, then places it at
     * the chest bone. The model hangs under the primary-hand UI attach node, so its local transform is
     * re-derived every frame from the chest bone and the (mirrored / PA) model transform — which keeps it on
     * the body as the hand moves and reflects INI live-reload immediately. The bone is looked up every frame
     * because the skeleton is rebuilt (character creation, power armor).
     */
    void BodyFlashlightMesh::onFrameUpdate(const bool enabled)
    {
        if (!enabled) {
            detach();
            return;
        }

        const auto root = f4vr::getRootNode();
        _stowBone = root ? f4vr::findNode(root, STOW_BONE_NAME) : nullptr;
        const auto parent = f4vr::getVRPlayerNodes()->primaryUIAttachNode;
        if (!_stowBone || !parent) {
            detach();
            return;
        }

        if (_attachedTo && parent != _attachedTo) {
            detach();
        }

        if (!_attachedTo) {
            attach(parent);
            if (!_attachedTo) {
                return;
            }
        }

        _inPA = f4vr::isInPowerArmor();
        const auto stowTransform = mirrorTransform(g_config.getFlashlightBodyTransform(_inPA));
        _meshNode->local = common::MatrixUtils::reparentTransform(_stowBone->world, stowTransform, _attachedTo->world);
        f4vr::updateTransformsDown(_meshNode.get(), true);
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

    /**
     * Forces the cached model to detach so it re-attaches to fresh nodes later.
     */
    void BodyFlashlightMesh::invalidate()
    {
        detach();
    }

    /**
     * Clones the model on first use (hiding its beam + collision) and attaches it to the parent node.
     */
    void BodyFlashlightMesh::attach(RE::NiNode* parentNode)
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
        logger::info("BodyFlashlightMesh: attached under '{}', placed at '{}'", parentNode->name.c_str(), STOW_BONE_NAME);
    }

    /**
     * Detaches the model from its parent while keeping the clone cached.
     */
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
     * Returns a config transform authored for a right-handed player, mirroring the Z translate and
     * heading/roll when the player is left-handed so one configured value reads correctly for both
     * handedness modes.
     */
    RE::NiTransform BodyFlashlightMesh::mirrorTransform(const RE::NiTransform& transform)
    {
        const float sign = f4vr::isLeftHandedMode() ? -1.0f : 1.0f;

        RE::NiTransform mirrored;
        mirrored.translate = RE::NiPoint3(transform.translate.x, transform.translate.y, sign * transform.translate.z);

        float heading, roll, attitude;
        common::MatrixUtils::getEulerAnglesFromMatrixDegrees(transform.rotate, &heading, &roll, &attitude);
        mirrored.rotate = common::MatrixUtils::getMatrixFromEulerAnglesDegrees(sign * heading, sign * roll, attitude);
        mirrored.scale = transform.scale;
        return mirrored;
    }

    /**
     * The grab-zone transform expressed in stow-bone space with its origin moved to the stowed model.
     */
    RE::NiTransform BodyFlashlightMesh::grabZoneTransform() const
    {
        RE::NiTransform orb = g_config.bodyActivation.zoneFor(_inPA);
        orb.translate += g_config.getFlashlightBodyTransform(_inPA).translate;
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
