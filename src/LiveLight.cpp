#include "LiveLight.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "Utils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

// Where ShowPipboyLight keeps the live light: CommonLibF4VR's VR layout puts these 0x470 past the flat 0xC38 / 0xC40,
// matching the field PlayerCharacter::IsPipboyLightOn (VR 0x140f27790) reads.
static_assert(offsetof(RE::PlayerCharacter, pipboyLight) == 0x10A8);
static_assert(offsetof(RE::PlayerCharacter, niPipboyLight) == 0x10B0);

namespace
{
    // NiLight members, as TESObjectLIGH::GenDynamic (VR 0x140306a50) sets them on a new light. The renderer and
    // BSLight::GetLuminanceAtPoint (VR 0x14286ea60) read them every frame.
    constexpr std::size_t NILIGHT_DIFFUSE = 0x16C; // NiColor
    constexpr std::size_t NILIGHT_RADIUS = 0x178; // NiPoint3, the radius in all three
    constexpr std::size_t NILIGHT_FADE = 0x184; // float

    // BSLight members, as the ShadowSceneNode light factory (VR 0x1427e9cd0) sets them on a new light.
    constexpr std::size_t BSLIGHT_SPOT_COS = 0xA4; // float: cos(FOV / 2), where the spot cone ends
    constexpr std::size_t BSLIGHT_VOLUME = 0x148; // NiPointer<BSGeometry>: the light-volume cone (BSLight::SetShape)
    constexpr std::size_t BSLIGHT_PROJECTION_CAMERA = 0x150; // NiPointer<NiCamera>: gobo projection camera, lights without shadows only
    constexpr std::size_t BSLIGHT_GOBO = 0x158; // NiPointer<NiTexture>
    constexpr std::size_t BSLIGHT_TYPE = 0x180; // std::int32_t
    constexpr std::int32_t BSLIGHT_TYPE_SPOT = 6;

    // BSShadowFrustumLight members: the near / far its shadow camera's frustum is rebuilt with every frame (vtable
    // slot 15, VR 0x142912d00 — far falls back to the NiLight radius when 0), and the render data holding that camera.
    constexpr std::size_t SHADOW_NEAR = 0x1F0;
    constexpr std::size_t SHADOW_FAR = 0x1F4;
    constexpr std::size_t SHADOW_RENDER_DATA = 0x198;
    constexpr std::size_t SHADOW_RENDER_DATA_CAMERA = 0x40;

    // GenDynamic's light form -> light conversions.
    constexpr float COLOR_SCALE = 1.0f / 255.0f;
    constexpr float DEGREES_TO_RADIANS = std::numbers::pi_v<float> / 180.0f;
    constexpr float MIN_RADIUS = 0.1f;

    // The light factory's cone: the half FOV is clamped to [1°, 160°] and the cone widened by this factor.
    constexpr float MIN_CONE_HALF_FOV = DEGREES_TO_RADIANS;
    constexpr float MAX_CONE_HALF_FOV = 160.0f * DEGREES_TO_RADIANS;
    constexpr float CONE_WIDTH_FACTOR = 1.22077f;

    // The near plane the gobo projection camera is made with (VR 0x14286fc30).
    constexpr float PROJECTION_CAMERA_NEAR = 1.0f;

    // GenDynamic clears this NiTexture flag on the gobo it hands a new light.
    constexpr std::uint32_t GOBO_CLEARED_TEXTURE_FLAG = 0x40;

    template <class T>
    T& member(void* object, const std::size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
    }
}

namespace ImFl
{
    /**
     * Apply the form's beam values to the live light the way TESObjectLIGH::GenDynamic and the ShadowSceneNode light
     * factory apply them to a new one: color, radius and fade on the NiLight; the spot cone, its light-volume mesh and
     * the gobo on the BSLight; and the frustum of the camera a shadow or gobo is projected from.
     * Only a spot light that is still the class the form's flags ask for (BSLight without shadows, BSShadowFrustumLight
     * with them) can take them in place. Returns false when it isn't, or the light is off, so the caller re-creates it.
     */
    bool LiveLight::refresh(const RE::TESObjectLIGH& form)
    {
        const auto player = f4vr::getPlayer();
        auto* niLight = player ? player->niPipboyLight.get() : nullptr;
        auto* bsLight = player ? player->pipboyLight.get() : nullptr;
        if (!niLight || !bsLight) {
            return false;
        }

        using enum RE::TES_LIGHT_FLAGS;
        const auto& flags = form.data.flags;
        const bool shadows = flags.any(kSpotShadow);
        const auto expectedVtbl = shadows ? f4vr::BSShadowFrustumLight_vtbl.address() : f4vr::BSLight_vtbl.address();
        if (!(shadows || flags.any(kNonShadowSpot)) || flags.any(kHemiShadow, kOmniShadow) || member<std::uintptr_t>(bsLight, 0) != expectedVtbl ||
            member<std::int32_t>(bsLight, BSLIGHT_TYPE) != BSLIGHT_TYPE_SPOT) {
            return false;
        }

        const float colorScale = flags.any(kNegative) ? -COLOR_SCALE : COLOR_SCALE;
        member<RE::NiColor>(niLight, NILIGHT_DIFFUSE) = RE::NiColor{ form.data.color.red * colorScale, form.data.color.green * colorScale, form.data.color.blue * colorScale };
        const float radius = form.data.radius > 0 ? static_cast<float>(form.data.radius) : MIN_RADIUS;
        member<RE::NiPoint3>(niLight, NILIGHT_RADIUS) = RE::NiPoint3(radius, radius, radius);
        member<float>(niLight, NILIGHT_FADE) = form.fade;

        const float fov = form.data.fov * DEGREES_TO_RADIANS;
        member<float>(bsLight, BSLIGHT_SPOT_COS) = std::cos(fov * 0.5f);
        rebuildLightVolume(bsLight, form, fov, radius);
        setGobo(bsLight, form);

        if (shadows) {
            // Near and far are re-applied to the shadow camera every frame, its FOV only here.
            member<float>(bsLight, SHADOW_NEAR) = form.data.nearDistance;
            member<float>(bsLight, SHADOW_FAR) = radius;
            if (auto* renderData = member<std::byte*>(bsLight, SHADOW_RENDER_DATA)) {
                if (auto* shadowCamera = member<RE::NiCamera*>(renderData, SHADOW_RENDER_DATA_CAMERA)) {
                    f4vr::BSLight_SetCameraFrustum(bsLight, fov, fov, shadowCamera, form.data.nearDistance, radius);
                }
            }
        } else if (auto* projectionCamera = member<RE::NiCamera*>(bsLight, BSLIGHT_PROJECTION_CAMERA)) {
            // Made on the light's first frame with a gobo, and its frustum set only then.
            f4vr::BSLight_SetCameraFrustum(bsLight, fov, fov, projectionCamera, PROJECTION_CAMERA_NEAR, radius);
        }
        return true;
    }

    /**
     * Re-create the live light from the form: hide it and show it again, like two togglePipboyLight calls without the
     * vanilla toggle's menu sound, controller light bar and glow-mesh effects, since the light is back on the same frame.
     * A refresh still waiting to rebuild its cone is dropped: the new light is built from the form as it is now.
     */
    void LiveLight::recreate()
    {
        _pendingRefreshForm = nullptr;
        _lightVolumeRebuildAttempts = 0;

        const auto player = f4vr::getPlayer();
        f4vr::PlayerCharacter_ShowPipboyLight(player, false, true);
        f4vr::PlayerCharacter_ShowPipboyLight(player, true, true);
    }

    /**
     * Per frame: redo a refresh whose cone rebuild the busy renderer refused, and drop the references to objects that
     * were retired from the light long enough ago.
     */
    void LiveLight::onFrameUpdate()
    {
        if (const auto* form = std::exchange(_pendingRefreshForm, nullptr)) {
            refresh(*form);
        }

        for (auto& retired : _retiredObjects) {
            if (--retired.framesLeft <= 0) {
                retired.object->DecRefCount();
                retired.object = nullptr;
            }
        }
        std::erase_if(_retiredObjects, [](const RetiredObject& retired) { return !retired.object; });
    }

    /**
     * Rebuild the light-volume cone the deferred renderer draws the spot with, for the new FOV and radius, as the light
     * factory builds it (BSLight::SetShape): a cone narrower or shorter than the beam would cut the light off at its
     * edge. The old cone is retired, not dropped. SetShape keeps the old cone while the renderer is busy, and then
     * the refresh is redone on the next frame, a limited number of times.
     */
    void LiveLight::rebuildLightVolume(RE::BSLight* bsLight, const RE::TESObjectLIGH& form, const float fov, const float radius)
    {
        const float tanHalfFov = std::tan(std::clamp(fov * 0.5f, MIN_CONE_HALF_FOV, MAX_CONE_HALF_FOV));
        const float width = std::sqrt(tanHalfFov * tanHalfFov * 2.0f) * CONE_WIDTH_FACTOR;

        auto* oldVolume = member<RE::NiRefObject*>(bsLight, BSLIGHT_VOLUME);
        if (oldVolume) {
            oldVolume->IncRefCount();
            retire(oldVolume);
        }

        // SetShape does nothing for the type the light already has.
        member<std::int32_t>(bsLight, BSLIGHT_TYPE) = 0;
        f4vr::BSLight_SetShape(bsLight, BSLIGHT_TYPE_SPOT, width, width * radius, radius, 1.0f, 1.0f, 1.0f);

        const auto* newVolume = member<RE::NiRefObject*>(bsLight, BSLIGHT_VOLUME);
        if (newVolume && newVolume != oldVolume) {
            _lightVolumeRebuildAttempts = 0;
            return;
        }
        if (++_lightVolumeRebuildAttempts < MAX_LIGHT_VOLUME_REBUILD_ATTEMPTS) {
            _pendingRefreshForm = &form;
        } else {
            logger::warn("Renderer kept refusing to rebuild the flashlight light volume, keeping the old one");
            _lightVolumeRebuildAttempts = 0;
        }
    }

    /**
     * Point the light at the form's gobo texture, or none for an empty path: the texture GenDynamic loads for a new
     * light. The light's reference to the old texture is retired, not dropped.
     */
    void LiveLight::setGobo(RE::BSLight* bsLight, const RE::TESObjectLIGH& form)
    {
        const auto& goboPath = form.goboTexture.textureName;
        auto* gobo = goboPath.empty() ? nullptr : Utils::loadGoboTexture(goboPath.c_str());
        auto*& goboSlot = member<RE::NiTexture*>(bsLight, BSLIGHT_GOBO);
        if (goboSlot == gobo) {
            return;
        }
        if (gobo) {
            gobo->flags &= ~GOBO_CLEARED_TEXTURE_FLAG;
            gobo->IncRefCount();
        }
        if (goboSlot) {
            retire(goboSlot);
        }
        goboSlot = gobo;
    }

    /**
     * Take over a reference to an object just replaced on the light and drop it RETIRE_FRAMES frames later
     * (onFrameUpdate()), in case the renderer still uses the pointer it had.
     */
    void LiveLight::retire(RE::NiRefObject* object)
    {
        _retiredObjects.push_back({ .object = object, .framesLeft = RETIRE_FRAMES });
    }
}
