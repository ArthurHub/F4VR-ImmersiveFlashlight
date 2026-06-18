#pragma once

#include "Config.h"

namespace ImFl
{
    /**
     * Manages the stowed flashlight model attached to the chest skeleton bone, and — only while
     * debugging — a visual sphere mesh marking the grab zone. The model's "Flashlight_lamp_FX" node is
     * hidden so it casts no glow. The grab zone itself is purely a center + radius computed from the
     * configured transform (no mesh needed); the debug sphere is loaded/attached only when the debug
     * flag is on, to show that exact zone for tuning.
     */
    class BodyFlashlightMesh
    {
    public:
        explicit BodyFlashlightMesh() = default;

        /**
         * Attaches to (or detaches from) the chest bone to match `enabled`, re-attaching when the bone or
         * power-armor state changes, then applies the (mirrored, PA-aware) model transform and maintains
         * the debug grab sphere.
         */
        void onFrameUpdate(bool enabled);

        /**
         * Shows or hides the stowed model.
         */
        void setVisible(bool visible) const;

        /**
         * True once the model is attached to a body bone.
         */
        bool isAttached() const
        {
            return _attachedTo != nullptr;
        }

        /**
         * True when the point lies inside the configured grab zone. The center and radius are derived from
         * the grab-sphere transform, offset so its origin sits at the stowed model rather than the bone — no
         * mesh is involved, so this works whether or not the debug sphere is shown.
         */
        bool isWithinGrabSphere(const RE::NiPoint3& point) const;

        /**
         * Forces a detach so the nodes re-attach to fresh skeleton nodes next frame. Call on power armor
         * transition and game session load since the skeleton pointers may have changed.
         */
        void invalidate();

    private:
        void attach(RE::NiNode* parentNode, bool inPowerArmor);
        void detach();
        void updateDebugSphere();
        static void applyMirroredTransform(RE::NiNode* node, const RE::NiTransform& transform);
        RE::NiTransform grabSphereBoneTransform() const;
        void hideBeamNode() const;

        RE::NiPointer<RE::NiNode> _meshNode;
        RE::NiPointer<RE::NiNode> _sphereNode; // debug visual only; null until the debug flag is first enabled
        RE::NiNode* _attachedTo = nullptr;
        bool _attachedInPA = false;

        static constexpr const char* MESH_NODE_NAME = "ImmersiveFlashlightBody";
        static constexpr const char* NIF_PATH = "flashlight-model.nif";
        static constexpr const char* BEAM_NODE_NAME = "Flashlight_lamp_FX";
        static constexpr const char* SPHERE_NODE_NAME = "ImmersiveFlashlightGrabSphere";
        static constexpr const char* SPHERE_NIF_PATH = "debug-sphere.nif";
        static constexpr float SPHERE_NIF_BASE_RADIUS = 0.5f;
    };
}
