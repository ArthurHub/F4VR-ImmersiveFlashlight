#pragma once

#include <array>
#include <map>
#include <optional>

namespace ImFl
{
    /**
     * The visible glow of the flashlight beam: a soft cone mesh matching the active beam, hung under a node its owner
     * picks (the hand-held model's lens, a weapon's lamp).
     *
     * A node transform scales all three axes alike, so it can't change a cone's angle. The angle comes from one of the
     * mesh variants instead (flashlight-beam-<N>deg.nif, N = 20..90 in steps of 5, the beam screen's FOV step), picked
     * from the active beam's FOV less fBeamGlowFovOffset and rounded down. Each variant crosses its lens plane (x = 0)
     * inside the lens and starts behind it, inside the flashlight, so its start edge is hidden at any angle. The color and
     * brightness are written on the variant's own material whenever the active beam values change (the beam screen's
     * tuning changes them live): the beam color, and fBeamGlowBaseIntensity + fade x fBeamGlowIntensity as the emissive
     * scale. The mesh's authored alpha is left alone.
     *
     * The owner decides where the glow hangs (attach) and whether it shows (setVisible), and calls onFrameUpdate() while
     * it may be seen.
     */
    class BeamGlowMesh
    {
    public:
        /**
         * What the active beam's glow looks like.
         */
        struct Look
        {
            // Full cone angle of the mesh variant, in degrees.
            int variantDegrees = 0;
            // The beam color, 0..1 per channel.
            std::array<float, 3> color{};
            // Written as the material's base color scale; <= 0 hides the glow.
            float colorScale = 0.0f;

            bool operator==(const Look&) const = default;
        };

        explicit BeamGlowMesh(std::string nodeName)
            : _nodeName(std::move(nodeName))
        {}

        /**
         * The glow look for the active beam values, or nullopt before they are resolved.
         */
        static std::optional<Look> getActiveBeamLook();

        void attach(RE::NiNode* parent);
        void detach();
        void setVisible(bool visible);
        void onFrameUpdate();

        /**
         * The attached variant's node, whose local transform is the owner's to set; null until a variant has loaded.
         */
        RE::NiNode* node() const
        {
            return _node.get();
        }

    private:
        RE::NiPointer<RE::NiNode> getVariant(int degrees);
        void showVariant(int degrees);
        void tint(const Look& look) const;
        void applyVisibility() const;

        std::string _nodeName;
        // Cloned variants by angle. A null entry is one that failed to load, so it isn't retried every frame.
        std::map<int, RE::NiPointer<RE::NiNode>> _variants;
        RE::NiPointer<RE::NiNode> _node;
        int _nodeDegrees = 0;
        RE::NiNode* _parent = nullptr;
        // The look last written on _node's material; reset when another variant takes its place.
        std::optional<Look> _tintedLook;
        bool _visible = true;

        static constexpr int MIN_VARIANT_DEGREES = 20;
        static constexpr int MAX_VARIANT_DEGREES = 90;
        static constexpr int VARIANT_STEP_DEGREES = 5;
    };
}
