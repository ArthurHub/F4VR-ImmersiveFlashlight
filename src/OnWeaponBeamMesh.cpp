#include "OnWeaponBeamMesh.h"

#include "Config.h"
#include "FlashlightState.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "f4vr/F4VRUtils.h"

namespace ImFl
{
    /**
     * Attaches the beam-only model to the supplied weapon flashlight mesh node (cloning on first use),
     * re-attaching when the node changes, then re-applies the mount transform each frame so INI live-reload
     * is reflected immediately. A null node detaches and hides the model.
     */
    void OnWeaponBeamMesh::onFrameUpdate()
    {
        if (FlashlightState::flashlightLocation != FlashlightLocation::OnWeapon || !RestrictionHandler::getOnWeaponFlashlightMeshNode().first) {
            detach();
            return;
        }

        if (!_attachedTo) {
            attach();
        }

        if (_attachedTo && _meshNode) {
            const bool beamVisible = FlashlightState::flashlightLocation == FlashlightLocation::OnWeapon && Utils::isFlashlightOn();
            f4vr::setNodeVisibility(_meshNode.get(), beamVisible);
            if (beamVisible) {
                applyMountTransform();
            }
        }
    }

    /** Forces the cached model to detach so it re-attaches to fresh weapon 3D later. */
    void OnWeaponBeamMesh::invalidate()
    {
        detach();
    }

    /** Clones the beam-only model on first use (dropping collision) and attaches it to the mesh node. */
    void OnWeaponBeamMesh::attach()
    {
        if (!_meshNode) {
            _meshNode.reset(f4vr::getClonedNiNodeForNifFileSetName(NIF_PATH, MESH_NODE_NAME));
            if (!_meshNode) {
                logger::warn("OnWeaponBeamMesh: failed to clone NIF '{}'", NIF_PATH);
                return;
            }
            // Passive glow prop: no collision so it can't disturb physics on the moving weapon.
            _meshNode->collisionObject.reset();
            logger::info("OnWeaponBeamMesh: cloned beam-only NIF");
        }

        const auto [onWeaponNode, onWeaponTransform] = RestrictionHandler::getOnWeaponFlashlightMeshNode();

        _onWeaponTransform = onWeaponTransform;
        _attachedTo = onWeaponNode->parent;

        _attachedTo->AttachChild(_meshNode.get(), true);

        applyMountTransform();
        f4vr::updateTransformsDown(_meshNode.get(), true);
        logger::info("OnWeaponBeamMesh: attached to weapon flashlight mesh");
    }

    /** Detaches the cached model from its parent while keeping the clone cached. */
    void OnWeaponBeamMesh::detach()
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
        _onWeaponTransform.reset();
        logger::debug<>("OnWeaponBeamMesh: detached");
    }

    /**
     * Sets the model's local transform from the configured mount offset (the same value that roots the game
     * light at the mesh node). No handedness mirroring — the weapon mesh is the same in both modes.
     */
    void OnWeaponBeamMesh::applyMountTransform() const
    {
        _meshNode->local = *_onWeaponTransform;
        const auto& transform = g_config.weaponFlashlightMountTransform;
        _meshNode->local.translate += transform.translate;
        _meshNode->local.rotate = _meshNode->local.rotate * transform.rotate;
        _meshNode->local.scale *= transform.scale;
    }
}
