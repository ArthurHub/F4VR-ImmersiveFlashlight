#include "OnWeaponBeamMesh.h"

#include "Config.h"
#include "FlashlightState.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "f4vr/F4VRUtils.h"

namespace ImFl
{
    /**
     * Attaches the beam glow to the weapon flashlight mesh's parent node, re-attaching when the node changes, then
     * keeps it in step with the beam values and re-applies the mount transform each frame so INI live-reload is
     * reflected immediately. Without a mounted lamp it detaches.
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

        if (_attachedTo) {
            const bool beamVisible = Utils::isFlashlightOn();
            _beamGlow.setVisible(beamVisible);
            if (beamVisible) {
                _beamGlow.onFrameUpdate();
                applyMountTransform();
            }
        }
    }

    /**
     * Forces the glow to detach so it re-attaches to fresh weapon 3D later.
     */
    void OnWeaponBeamMesh::invalidate()
    {
        detach();
    }

    /**
     * Hangs the beam glow under the weapon flashlight mesh's parent, a sibling of the lamp.
     */
    void OnWeaponBeamMesh::attach()
    {
        const auto [onWeaponNode, onWeaponTransform] = RestrictionHandler::getOnWeaponFlashlightMeshNode();
        if (!onWeaponNode->parent) {
            return;
        }

        _onWeaponTransform = onWeaponTransform;
        _attachedTo = onWeaponNode->parent;

        _beamGlow.attach(_attachedTo);

        applyMountTransform();
        if (const auto node = _beamGlow.node()) {
            f4vr::updateTransformsDown(node, true);
        }
        logger::info("OnWeaponBeamMesh: attached to weapon flashlight mesh");
    }

    /**
     * Detaches the beam glow from the weapon while keeping its loaded meshes cached.
     */
    void OnWeaponBeamMesh::detach()
    {
        if (!_attachedTo) {
            return;
        }

        _beamGlow.detach();

        _attachedTo = nullptr;
        _onWeaponTransform.reset();
        logger::debug<>("OnWeaponBeamMesh: detached");
    }

    /**
     * Sets the glow's local transform from the configured mount offset (the same value that roots the game
     * light at the mesh node). No handedness mirroring — the weapon mesh is the same in both modes.
     */
    void OnWeaponBeamMesh::applyMountTransform() const
    {
        const auto node = _beamGlow.node();
        if (!node) {
            return;
        }

        node->local = *_onWeaponTransform;
        const auto& transform = g_config.weaponFlashlightMountTransform;
        node->local.translate += transform.translate;
        node->local.rotate = node->local.rotate * transform.rotate;
        node->local.scale *= transform.scale;
    }
}
