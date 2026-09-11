#pragma once

#include "Config.h"

namespace ImFl
{
    /**
     * Manages the stowed flashlight model shown at the chest skeleton bone. The model's "Flashlight_lamp_FX"
     * node is hidden so it casts no glow. The model is authored for a right-handed player and auto-mirrored
     * when left-handed / PA-posed.
     *
     * The model is not a child of the chest bone: character creation rebuilds the player's 3D (preset applied,
     * sex switched), and a node of ours inside that tree crashes the game shortly after. Instead it hangs under
     * the VR primary-hand UI attach node, which lives outside the player's 3D and still renders the model's lit
     * shader (the room, HMD and world scene nodes don't), and is placed at the chest bone every frame.
     *
     * The grab interaction itself (zone test, suppression, debug visual) lives in the owning Flashlight via a
     * WandActivationSphere; this class only exposes the chest bone and the zone transform anchored to the
     * stowed model.
     */
    class BodyFlashlightMesh
    {
    public:
        explicit BodyFlashlightMesh() = default;

        /**
         * Attaches (or detaches) the model to match `enabled`, then places it at the chest bone through the
         * (mirrored, PA-aware) model transform.
         */
        void onFrameUpdate(bool enabled);

        /**
         * Shows or hides the stowed model.
         */
        void setVisible(bool visible) const;

        /**
         * The chest bone the model is placed at, or null when detached. This is the node the grab zone is
         * measured against.
         */
        RE::NiNode* stowBoneNode() const
        {
            return _attachedTo ? _stowBone : nullptr;
        }

        /**
         * The grab-zone transform in stow-bone space, with its origin offset to the stowed model (so the
         * zone is measured from the model, not the bone). Pass to the owner's WandActivationSphere.
         */
        RE::NiTransform grabZoneTransform() const;

        /**
         * Forces a detach so the model re-attaches and re-resolves its nodes next frame. Call on power armor
         * transition and game session load since the skeleton pointers may have changed.
         */
        void invalidate();

    private:
        void attach(RE::NiNode* parentNode);
        void detach();
        static RE::NiTransform mirrorTransform(const RE::NiTransform& transform);
        void hideBeamNode() const;

        RE::NiPointer<RE::NiNode> _meshNode;
        // the node the model is a child of (the VR primary-hand UI attach node)
        RE::NiNode* _attachedTo = nullptr;
        // the chest bone the model is placed at each frame
        RE::NiNode* _stowBone = nullptr;
        bool _inPA = false;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlightBody";
        static constexpr const char* NIF_PATH = "flashlight-model.nif";
        static constexpr const char* BEAM_NODE_NAME = "Flashlight_lamp_FX";
    };
}
