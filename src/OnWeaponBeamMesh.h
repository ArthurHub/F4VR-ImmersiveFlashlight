#pragma once

#include "BeamGlowMesh.h"
#include "Config.h"

namespace ImFl
{
    /**
     * Manages the beam glow (a BeamGlowMesh — just the glow cone, no flashlight body) rooted at the weapon's
     * modeled flashlight mesh. Shown only while the beam is mounted to that mesh (the weapon-flashlight
     * beam-to-mesh path is active), so the gun's own lamp emits a visible beam glow that tracks the weapon;
     * detached and hidden in every other state. Uses the same `tWeaponFlashlightMountTransform` offset that
     * roots the game light at the mesh node, re-applied each frame so INI live-reload tunes both together.
     */
    class OnWeaponBeamMesh
    {
    public:
        explicit OnWeaponBeamMesh() = default;

        /**
         * Called every frame. While the light sits on a weapon with a detected flashlight mesh, attaches the beam
         * glow to it with the configured mount transform and shows it while the light is on; otherwise detaches it.
         */
        void onFrameUpdate();

        /**
         * Force re-attach on the next frame. Call on power armor transition and game session load since the
         * weapon 3D / skeleton pointers may have changed.
         */
        void invalidate();

    private:
        void attach();
        void detach();
        void applyMountTransform() const;

        BeamGlowMesh _beamGlow{ MESH_NODE_NAME };
        RE::NiNode* _attachedTo = nullptr;
        std::optional<RE::NiTransform> _onWeaponTransform;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlightWeaponBeam";
    };
}
