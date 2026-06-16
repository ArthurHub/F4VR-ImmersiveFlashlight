#pragma once

#include "Config.h"

namespace ImFl
{
    /**
     * Manages the lifecycle of a cloned 3D flashlight mesh node attached to the player's hand.
     * When the flashlight is on and in a hand mode (InOffhand, InPrimaryHand), the mesh is
     * attached as a child of the VR controller wand node so it auto-tracks the hand each frame.
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
        void setMeshTransform(const RE::NiTransform& transform) const;
        void detach();
        void hide(bool clearPose) const;
        void show() const;
        void clearHandPose() const;

        static RE::NiNode* resolveParentNode();

        RE::NiPointer<RE::NiNode> _meshNode;
        RE::NiNode* _attachedTo = nullptr;
        FlashlightLocation _attachedForLocation = FlashlightLocation::OnHead;
        FlashlightGripStyle _attachedForGripStyle = FlashlightGripStyle::Forward;
        mutable bool _handPoseSet = false;
        mutable FlashlightLocation _handPoseSetForLocation = FlashlightLocation::OnHead;
        mutable FlashlightGripStyle _handPoseSetForGripStyle = FlashlightGripStyle::Forward;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlight";
        // f4vr::getClonedNiNodeForNifFileSetName prepends "Data/Meshes/" to this path
        static constexpr const char* NIF_PATH = "flashlight_model.nif";
    };
}
