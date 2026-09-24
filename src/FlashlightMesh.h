#pragma once

#include "BeamGlowMesh.h"
#include "Config.h"

namespace ImFl
{
    /**
     * Manages the lifecycle of a cloned 3D flashlight mesh node attached to the player's hand.
     * When the flashlight is on and in a hand mode (InOffhand, InPrimaryHand), the mesh is
     * attached as a child of the VR controller wand node so it auto-tracks the hand each frame.
     * The model carries the beam glow (a BeamGlowMesh) under its "Flashlight_beam_attach" node, and its lens disc is
     * tinted with the beam color.
     */
    class FlashlightMesh
    {
    public:
        explicit FlashlightMesh() = default;

        /**
         * Called every frame (even when flashlight is off, to handle detach).
         */
        void onFrameUpdate(bool isFlashlightOn);

        /**
         * Force re-attach on the next frame. Call on power armor transition and game session load
         * since the skeleton pointers may have changed.
         */
        void invalidate();

    private:
        void attach(RE::NiNode* parentNode);
        void setMeshTransform() const;
        void detach();
        void hide(bool clearPose, const char* reason) const;
        void show() const;
        void clearHandPose() const;
        void tintLens();

        static RE::NiNode* resolveParentNode();

        RE::NiPointer<RE::NiNode> _meshNode;
        BeamGlowMesh _beamGlow{ BEAM_GLOW_NODE_NAME };
        RE::NiAVObject* _lensNode = nullptr;
        std::optional<std::array<float, 3>> _lensColor;
        RE::NiNode* _attachedTo = nullptr;
        FlashlightLocation _attachedForLocation = FlashlightLocation::OnHead;
        FlashlightGripStyle _attachedForGripStyle = FlashlightGripStyle::Forward;
        mutable bool _handPoseSet = false;
        mutable FlashlightLocation _handPoseSetForLocation = FlashlightLocation::OnHead;
        mutable FlashlightGripStyle _handPoseSetForGripStyle = FlashlightGripStyle::Forward;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlight";
        static constexpr const char* NIF_PATH = "flashlight-model.nif";
        static constexpr const char* BEAM_GLOW_NODE_NAME = "ImmersiveFlashlightHandBeam";
        // Empty node at the lens whose +X points out of the lamp, where the beam glow hangs.
        static constexpr const char* BEAM_ATTACH_NODE_NAME = "Flashlight_beam_attach";
        static constexpr const char* LENS_NODE_NAME = "Flashlight_lens_FX";
    };
}
