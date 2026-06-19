#pragma once

#include "Config.h"

namespace ImFl
{
    /**
     * Manages the stowed flashlight model attached to the chest skeleton bone. The model's
     * "Flashlight_lamp_FX" node is hidden so it casts no glow. The model is authored for a right-handed
     * player and auto-mirrored when left-handed / PA-posed. The grab interaction itself (zone test,
     * suppression, debug visual) lives in the owning Flashlight via a WandActivationSphere; this class only
     * exposes the bone it's attached to and the zone transform anchored to the stowed model.
     */
    class BodyFlashlightMesh
    {
    public:
        explicit BodyFlashlightMesh() = default;

        /**
         * Attaches to (or detaches from) the chest bone to match `enabled`, re-attaching when the bone or
         * power-armor state changes, then applies the (mirrored, PA-aware) model transform.
         */
        void onFrameUpdate(bool enabled);

        /**
         * Shows or hides the stowed model.
         */
        void setVisible(bool visible) const;

        /**
         * The bone the model is currently attached to, or null when detached. This is the parent node the
         * grab zone is measured against.
         */
        RE::NiNode* attachedNode() const
        {
            return _attachedTo;
        }

        /**
         * The grab-zone transform in stow-bone space, with its origin offset to the stowed model (so the
         * zone is measured from the model, not the bone). Pass to the owner's WandActivationSphere.
         */
        RE::NiTransform grabZoneTransform() const;

        /**
         * Forces a detach so the model re-attaches to fresh skeleton nodes next frame. Call on power armor
         * transition and game session load since the skeleton pointers may have changed.
         */
        void invalidate();

    private:
        void attach(RE::NiNode* parentNode, bool inPowerArmor);
        void detach();
        static void applyMirroredTransform(RE::NiNode* node, const RE::NiTransform& transform);
        void hideBeamNode() const;

        RE::NiPointer<RE::NiNode> _meshNode;
        RE::NiNode* _attachedTo = nullptr;
        bool _attachedInPA = false;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlightBody";
        static constexpr const char* NIF_PATH = "flashlight-model.nif";
        static constexpr const char* BEAM_NODE_NAME = "Flashlight_lamp_FX";
    };
}
