#include "BeamGlowMesh.h"

#include "Config.h"
#include "FlashlightState.h"
#include "f4vr/EffectShaderMaterials.h"
#include "f4vr/F4VRUtils.h"

namespace ImFl
{
    /**
     * Rounds the glow's angle down to a mesh variant and clamps it to the variants that exist, so the glow never reads
     * wider than the lit cone. The small epsilon keeps a value that lands exactly on a step from flooring a step low.
     */
    std::optional<BeamGlowMesh::Look> BeamGlowMesh::getActiveBeamLook()
    {
        if (!FlashlightState::flashlightFov || !FlashlightState::flashlightFade || !FlashlightState::flashlightColorRed || !FlashlightState::flashlightColorGreen ||
            !FlashlightState::flashlightColorBlue) {
            return std::nullopt;
        }

        const float glowFov = *FlashlightState::flashlightFov - g_config.beamGlowFovOffset;
        const int steps = static_cast<int>(std::floor(glowFov / VARIANT_STEP_DEGREES + 0.001f));
        const auto channel = [](const int value) { return std::clamp(static_cast<float>(value) / 255.0f, 0.0f, 1.0f); };
        return Look{
            .variantDegrees = std::clamp(steps * VARIANT_STEP_DEGREES, MIN_VARIANT_DEGREES, MAX_VARIANT_DEGREES),
            .color = { channel(*FlashlightState::flashlightColorRed), channel(*FlashlightState::flashlightColorGreen), channel(*FlashlightState::flashlightColorBlue) },
            .colorScale = g_config.beamGlowBaseIntensity + *FlashlightState::flashlightFade * g_config.beamGlowIntensity,
        };
    }

    /**
     * Hangs the glow under `parent`, loading the variant for the active beam on first use. No-op when it already hangs
     * there.
     */
    void BeamGlowMesh::attach(RE::NiNode* parent)
    {
        if (!parent || parent == _parent) {
            return;
        }

        detach();
        _parent = parent;
        if (_node) {
            _parent->AttachChild(_node.get(), true);
            f4vr::updateTransformsDown(_node.get(), true);
        }
        onFrameUpdate();
    }

    /**
     * Detaches the glow from its parent while keeping the loaded variants cached.
     */
    void BeamGlowMesh::detach()
    {
        if (_node && _node->parent) {
            RE::NiPointer<RE::NiAVObject> held;
            _node->parent->DetachChild(_node.get(), held);
            // held goes out of scope; _node keeps the clone alive
        }
        _parent = nullptr;
    }

    void BeamGlowMesh::setVisible(const bool visible)
    {
        if (visible != _visible) {
            _visible = visible;
            applyVisibility();
        }
    }

    /**
     * Keeps the attached glow in step with the active beam: swaps in the variant for its angle, and re-tints on any
     * change. One comparison when nothing changed, so it is fine to call every frame the glow may be seen.
     */
    void BeamGlowMesh::onFrameUpdate()
    {
        if (!_parent) {
            return;
        }
        const auto look = getActiveBeamLook();
        if (!look) {
            return;
        }

        if (look->variantDegrees != _nodeDegrees) {
            showVariant(look->variantDegrees);
        }
        if (_node && look != _tintedLook) {
            tint(*look);
            _tintedLook = look;
            applyVisibility();
            logger::debug("BeamGlowMesh: '{}' {}deg, color ({:.2f}, {:.2f}, {:.2f}) x {:.2f}",
                _nodeName,
                _nodeDegrees,
                look->color[0],
                look->color[1],
                look->color[2],
                look->colorScale);
        }
    }

    /**
     * The cloned mesh for a variant, loaded on first use: collision dropped, and its material made its own before it is
     * first attached, so it can be tinted while shown. A failed load is remembered as null.
     */
    RE::NiPointer<RE::NiNode> BeamGlowMesh::getVariant(const int degrees)
    {
        if (const auto it = _variants.find(degrees); it != _variants.end()) {
            return it->second;
        }

        const auto path = std::format("flashlight-beam-{}deg.nif", degrees);
        RE::NiPointer<RE::NiNode> variant;
        try {
            variant.reset(f4vr::getClonedNiNodeForNifFileSetName(path, _nodeName));
        } catch (const std::exception& ex) {
            logger::error("BeamGlowMesh: failed to load '{}': {}", path, ex.what());
        }
        if (variant) {
            // Passive glow prop: no collision, so it can't disturb physics on the moving hand or weapon.
            variant->collisionObject.reset();
            f4vr::makeEffectShaderMaterialsPrivate(variant.get());
            logger::info("BeamGlowMesh: loaded '{}' for '{}'", path, _nodeName);
        }
        _variants.emplace(degrees, variant);
        return variant;
    }

    /**
     * Puts the variant for `degrees` in place of the attached mesh, keeping its local transform. It stays hidden until
     * the caller tints it, so an untinted glow is never drawn. A variant that fails to load leaves the current mesh.
     */
    void BeamGlowMesh::showVariant(const int degrees)
    {
        const auto variant = getVariant(degrees);
        if (!variant) {
            return;
        }

        if (_node) {
            variant->local = _node->local;
            if (_node->parent) {
                RE::NiPointer<RE::NiAVObject> held;
                _node->parent->DetachChild(_node.get(), held);
            }
        }
        _parent->AttachChild(variant.get(), true);
        _node = variant;
        _nodeDegrees = degrees;
        _tintedLook.reset();
        applyVisibility();
        f4vr::updateTransformsDown(_node.get(), true);
    }

    /**
     * Writes the beam color and brightness on the attached variant's material, keeping its authored alpha.
     */
    void BeamGlowMesh::tint(const Look& look) const
    {
        f4vr::forEachEffectShaderMaterial(_node.get(), [&](RE::BSEffectShaderMaterial& material, RE::BSGeometry&) {
            material.baseColor.r = look.color[0];
            material.baseColor.g = look.color[1];
            material.baseColor.b = look.color[2];
            material.baseColorScale = look.colorScale;
        });
    }

    /**
     * Shown when the owner wants it, it has been tinted, and its brightness is above zero.
     */
    void BeamGlowMesh::applyVisibility() const
    {
        if (_node) {
            f4vr::setNodeVisibility(_node.get(), _visible && _tintedLook && _tintedLook->colorScale > 0.0f);
        }
    }
}
