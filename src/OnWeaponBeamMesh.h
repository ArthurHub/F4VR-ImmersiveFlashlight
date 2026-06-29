#pragma once

#include "Config.h"

namespace ImFl
{
    /**
     * Manages a cloned beam-only model (just the lamp glow FX, no flashlight body) rooted at the weapon's
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
         * Called every frame. When `meshNode` is a node, attaches the beam-only model to it (cloning on
         * first use) with the configured mount transform; a null `meshNode` detaches and hides it.
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

        RE::NiPointer<RE::NiNode> _meshNode;
        RE::NiNode* _attachedTo = nullptr;
        std::optional<RE::NiTransform> _onWeaponTransform;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlightWeaponBeam";
        static constexpr const char* NIF_PATH = "flashlight-beam-only.nif";
    };
}
