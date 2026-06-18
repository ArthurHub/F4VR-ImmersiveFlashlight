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
     * each frame so INI live-reload is reflected immediately, and maintains the debug grab sphere.
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

        updateDebugSphere();
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
     * Sphere-vs-point test. The zone uses the grab-sphere transform offset so its origin sits at the
     * stowed model rather than the bone (see grabSphereBoneTransform), carried into world space off the
     * stow bone with the engine's local->world convention (see MatrixUtils::calculateRelocation) and the
     * same Z-mirror applyMirroredTransform uses. The radius is the grab-sphere scale times the bone's
     * world scale times the sphere mesh's base radius, so it matches what the debug sphere renders.
     */
    bool BodyFlashlightMesh::isWithinGrabSphere(const RE::NiPoint3& point) const
    {
        if (!_attachedTo) {
            return false;
        }

        const RE::NiTransform orb = grabSphereBoneTransform();
        const float sign = f4vr::isLeftHandedMode() ? -1.0f : 1.0f;

        const auto& boneWorld = _attachedTo->world;
        const RE::NiPoint3 localOffset(orb.translate.x, orb.translate.y, sign * orb.translate.z);
        const RE::NiPoint3 center = boneWorld.translate + boneWorld.rotate.Transpose() * (localOffset * boneWorld.scale);
        const float radius = orb.scale * boneWorld.scale * SPHERE_NIF_BASE_RADIUS;

        return common::MatrixUtils::vec3Len(point - center) <= radius;
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

        const auto detachNode = [](const RE::NiPointer<RE::NiNode>& node) {
            if (node && node->parent) {
                RE::NiPointer<RE::NiAVObject> held;
                node->parent->DetachChild(node.get(), held);
                // held goes out of scope; the NiPointer member keeps the clone alive
            }
        };
        detachNode(_meshNode);
        detachNode(_sphereNode);

        _attachedTo = nullptr;
        logger::debug<>("BodyFlashlightMesh: detached");
    }

    /**
     * Maintains the debug grab-sphere visual: clones it lazily the first time the debug flag is on,
     * attaches it to the stow bone with the grab-sphere transform so it renders the exact zone, and
     * detaches it when the flag is off (so nothing is loaded/attached during normal play).
     */
    void BodyFlashlightMesh::updateDebugSphere()
    {
        if (g_config.debugShowGrabSphere) {
            if (!_sphereNode) {
                _sphereNode.reset(f4vr::getClonedNiNodeForNifFileSetName(SPHERE_NIF_PATH, SPHERE_NODE_NAME));
                if (!_sphereNode) {
                    logger::warn("BodyFlashlightMesh: failed to clone debug grab-sphere NIF '{}'", SPHERE_NIF_PATH);
                    return;
                }
                _sphereNode->collisionObject.reset();
                logger::info("BodyFlashlightMesh: cloned debug grab-sphere NIF");
            }
            if (_sphereNode->parent != _attachedTo) {
                _attachedTo->AttachChild(_sphereNode.get(), true);
            }
            applyMirroredTransform(_sphereNode.get(), grabSphereBoneTransform());
        } else if (_sphereNode && _sphereNode->parent) {
            RE::NiPointer<RE::NiAVObject> held;
            _sphereNode->parent->DetachChild(_sphereNode.get(), held);
        }
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
     * The grab-sphere transform expressed in stow-bone space with its origin moved to the stowed model.
     */
    RE::NiTransform BodyFlashlightMesh::grabSphereBoneTransform() const
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
