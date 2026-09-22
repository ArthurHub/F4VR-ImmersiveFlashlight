#include "FlashlightState.h"

#include <algorithm>
#include <cmath>

#include "Config.h"
#include "LiveLight.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "WeaponGripHandler.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

namespace
{
    /**
     * The light form values FlashlightState::setLightValues() writes, compared before and after to tell whether a
     * refresh changed anything the light is built from.
     */
    struct LightFormValues
    {
        float fade;
        std::uint32_t radius;
        float fov;
        float nearDistance;
        REX::EnumSet<RE::TES_LIGHT_FLAGS, std::uint32_t> flags;
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        std::string gobo;

        static LightFormValues of(const RE::TESObjectLIGH& light)
        {
            return {
                .fade = light.fade,
                .radius = light.data.radius,
                .fov = light.data.fov,
                .nearDistance = light.data.nearDistance,
                .flags = light.data.flags,
                .red = light.data.color.red,
                .green = light.data.color.green,
                .blue = light.data.color.blue,
                .gobo = std::string(light.goboTexture.textureName.c_str()),
            };
        }

        bool operator==(const LightFormValues&) const = default;
    };
}

namespace ImFl
{
    /**
     * Check if the active runtime location is any head-mounted flashlight variant.
     */
    bool FlashlightState::isHeadMountedFlashlight()
    {
        return flashlightLocation == FlashlightLocation::OnHead || flashlightLocation == FlashlightLocation::OnPAHead;
    }

    /**
     * Check if the active runtime location is any hand held flashlight variant.
     */
    bool FlashlightState::isHandHeldFlashlight()
    {
        return flashlightLocation == FlashlightLocation::InOffhand || flashlightLocation == FlashlightLocation::InPrimaryHand;
    }

    /**
     * Get the configured flashlight location currently in effect, picking the in-PA or out-of-PA
     * variant based on whether the player is in power armor.
     */
    FlashlightConfigLocation FlashlightState::getActiveFlashlightConfigLocation()
    {
        return f4vr::isInPowerArmor() ? g_config.flashlightConfigLocationInPA : g_config.flashlightConfigLocation;
    }

    /**
     * Switch the flashlight location to the given location and apply its light values (re-creating an on light).
     * Switching to the primary hand while the offhand carries the weapon puts the light in the free hand for the
     * rest of that carry, even when the config location is already the primary hand (which otherwise resolves to
     * the weapon).
     */
    void FlashlightState::switchFlashlightConfigLocation(const FlashlightConfigLocation location)
    {
        const bool inPowerArmor = f4vr::isInPowerArmor();
        const auto currentLocation = getActiveFlashlightConfigLocation();
        const bool heldInFreePrimaryHand = location == FlashlightConfigLocation::InPrimaryHand && WeaponGripHandler::isWeaponCarriedByOffhand();
        if (currentLocation == location && heldInFreePrimaryHand == _heldInFreePrimaryHand) {
            return;
        }
        _heldInFreePrimaryHand = heldInFreePrimaryHand;
        if (currentLocation != location) {
            logger::info("Switch flashlight location {} from {} to {}",
                inPowerArmor ? "(in PA)" : "(out of PA)",
                getFlashlightConfigLocationLabel(currentLocation),
                getFlashlightConfigLocationLabel(location));
            g_config.setFlashlightLocation(location, inPowerArmor);
        }
        refreshFlashlightLocation();
    }

    /**
     * Temporarily override the resolved runtime flashlight location.
     * Used by config mode to preview and edit locations independent of current gameplay state.
     */
    void FlashlightState::setFlashlightRuntimeLocationOverride(const std::optional<FlashlightLocation> locationOverride)
    {
        if (_runtimeLocationOverride == locationOverride) {
            return;
        }
        _runtimeLocationOverride = locationOverride;
        refreshFlashlightLocation();
    }

    /**
     * Refresh the current flashlight location based on config and game state.
     * Update the config references and reload the light values if location changed.
     */
    void FlashlightState::refreshFlashlightLocation()
    {
        if (!WeaponGripHandler::isWeaponCarriedByOffhand()) {
            _heldInFreePrimaryHand = false;
        }

        const auto newFlashlightLocation = getFlashlightLocation();
        if (flashlightLocation == newFlashlightLocation) {
            return;
        }

        flashlightLocation = newFlashlightLocation;
        refreshConfigReferences();

        // push the new location's values onto the light
        refreshLightValues(LightRefreshMode::RecreateWithSound);
    }

    /**
     * Reload the light values from config onto the game light, doing nothing more when none of them changed.
     * The game copies the light form onto the light when it's turned on, so an off light only needs the form
     * written. An on light is re-created by default (LiveLight::recreate(): the engine's own hide and show, without
     * the vanilla toggle's sounds), which INI hot-reload uses. A location change re-creates it with the vanilla
     * light-on sound (RecreateWithSound); the light moving hides the swap.
     * InPlace, for beam tuning, writes the values into the live light instead (LiveLight::refresh()) so they show
     * without it going off. That reaches deeper into the engine, so it's kept to tuning, and it still re-creates the
     * light when the flags changed (shadows on/off make another kind of light) or the light can't take them.
     */
    void FlashlightState::refreshLightValues(const LightRefreshMode mode)
    {
        const auto* light = getLightForm();
        if (!light) {
            logger::warn("Failed to find light object to set flashlight values");
            return;
        }

        const auto before = LightFormValues::of(*light);
        setLightValues();
        const auto after = LightFormValues::of(*light);
        if (after == before || !Utils::isFlashlightOn()) {
            return;
        }
        if (mode == LightRefreshMode::InPlace && after.flags == before.flags && LiveLight::refresh(*light)) {
            return;
        }
        logger::debug("Re-create the light to apply its values...");
        LiveLight::recreate();
        if (mode == LightRefreshMode::RecreateWithSound) {
            RE::UIUtils::PlayMenuSound("UIPipBoyLightOn");
        }
    }

    /**
     * Set the light values to config depending if the flashlight is in hand or on head.
     */
    void FlashlightState::setLightValues()
    {
        auto* light = getLightForm();
        if (!light) {
            logger::warn("Failed to find light object to set flashlight values");
            return;
        }

        if (!flashlightFade) {
            refreshConfigReferences();
        }

        // global to all flashlight locations
        light->data.nearDistance = g_config.flashlightNearDistance;
        light->data.flags = static_cast<RE::TES_LIGHT_FLAGS>(std::stoul(g_config.flashlightFlagsBitmask, nullptr, 2));

        // specific to current flashlight location
        light->fade = *flashlightFade;
        light->data.radius = *flashlightRadius;
        light->data.fov = *flashlightFov;
        light->data.color.red = static_cast<std::uint8_t>(*flashlightColorRed);
        light->data.color.green = static_cast<std::uint8_t>(*flashlightColorGreen);
        light->data.color.blue = static_cast<std::uint8_t>(*flashlightColorBlue);
        light->goboTexture.textureName = *flashlightGoboPath;
        Utils::loadGoboTexture(*flashlightGoboPath);
    }

    /**
     * The light form the flashlight is built from: the standard PA light, which the game uses while a worn item
     * carries the headlamp keyword (added to the Pip-Boy at load).
     */
    RE::TESObjectLIGH* FlashlightState::getLightForm()
    {
        return RE::TESForm::GetFormByID<RE::TESObjectLIGH>(0xB48A0);
    }

    /**
     * Recompute the active grip style for the current frame.
     * Forces Forward when no hand is holding the flashlight (head/PA/weapon), respects the locked modes,
     * and otherwise measures the wrist roll as the angle between the controller's top axis and world up:
     * 0° = top pointing up (Forward grip), 180 = top pointing down (Overhand fist grip). A hysteresis
     * band around the configured threshold prevents the style from flapping at the boundary.
     */
    void FlashlightState::refreshGripStyle()
    {
        if (g_config.flashlightGripMode == FlashlightGripMode::ForwardOnly) {
            flashlightGripStyle = FlashlightGripStyle::Forward;
            return;
        }
        if (g_config.flashlightGripMode == FlashlightGripMode::OverhandOnly) {
            flashlightGripStyle = FlashlightGripStyle::Overhand;
            return;
        }

        if (flashlightLocation != FlashlightLocation::InOffhand && flashlightLocation != FlashlightLocation::InPrimaryHand) {
            flashlightGripStyle = FlashlightGripStyle::Forward;
            return;
        }

        const auto wandNode = flashlightLocation == FlashlightLocation::InOffhand ? f4vr::getOffhandWandNode() : f4vr::getPrimaryHandWandNode();
        if (!wandNode) {
            return;
        }

        // Wand local +Z is the top of the controller. In a fist/overhand grip the wrist flips the
        // controller around its barrel axis so its top swings from world-up to world-down. tiltDeg
        // is the angle between the wand's local +Z and world +Z: 0 = top up, 180° = top down.
        // clamp guards acos against floating-point overshoot of [-1, 1] from the matrix transform.
        const RE::NiPoint3 upWorld = wandNode->world.rotate.Transpose() * RE::NiPoint3(0, 0, 1);
        const float tiltDeg = common::MatrixUtils::radsToDegrees(std::acos(std::clamp(upWorld.z, -1.0f, 1.0f)));

        const float enterOverhandDeg = g_config.flashlightGripOverhandTiltDegrees;
        const float exitOverhandDeg = g_config.flashlightGripOverhandTiltDegrees - g_config.flashlightGripHysteresisDegrees;

        const auto prevStyle = flashlightGripStyle;
        if (flashlightGripStyle == FlashlightGripStyle::Forward) {
            if (tiltDeg > enterOverhandDeg) {
                flashlightGripStyle = FlashlightGripStyle::Overhand;
            }
        } else {
            if (tiltDeg < exitOverhandDeg) {
                flashlightGripStyle = FlashlightGripStyle::Forward;
            }
        }

        if (prevStyle != flashlightGripStyle) {
            logger::info("Grip style switched to {} (tiltDeg={:.1f})", getGripStyleLabel(flashlightGripStyle), tiltDeg);
        }
    }

    /**
     * Whether a config-mode location override is currently active.
     */
    bool FlashlightState::isRuntimeLocationOverrideActive()
    {
        return _runtimeLocationOverride.has_value();
    }

    /**
     * Get a readable label for a flashlight config location.
     */
    const char* FlashlightState::getFlashlightConfigLocationLabel(const FlashlightConfigLocation location)
    {
        switch (location) {
        case FlashlightConfigLocation::OnHead:
            return "OnHead";
        case FlashlightConfigLocation::InOffhand:
            return "InOffhand";
        case FlashlightConfigLocation::InPrimaryHand:
            return "InPrimaryHand";
        default:
            return "Unknown";
        }
    }

    /**
     * Get a readable label for a flashlight grip style.
     */
    const char* FlashlightState::getGripStyleLabel(const FlashlightGripStyle style)
    {
        switch (style) {
        case FlashlightGripStyle::Forward:
            return "Forward";
        case FlashlightGripStyle::Overhand:
            return "Overhand";
        default:
            return "Unknown";
        }
    }

    /**
     * Get the real flashlight location based on config and current game state.
     * A temporary override takes priority, otherwise gameplay state decides between head, PA head, hand, and weapon.
     * For config InOffhand, a two-handed grip occupies the offhand: the light mounts on the weapon, but when the
     * weapon-flashlight requirement is on and the weapon carries no modeled flashlight it falls back to the head
     * (rather than turning off) until the grip is released and it returns to the offhand.
     * A weapon carried one-handed in the offhand leaves the primary hand free, so either hand config holds the
     * light in the primary hand. A weapon carried by the offhand with the firing grip detached (can't be fired)
     * keeps the light on the weapon like a two-handed hold, unless it was put in the free primary hand.
     */
    FlashlightLocation FlashlightState::getFlashlightLocation()
    {
        if (_runtimeLocationOverride.has_value()) {
            return _runtimeLocationOverride.value();
        }

        const auto configLocation = getActiveFlashlightConfigLocation();

        if (configLocation == FlashlightConfigLocation::OnHead) {
            return f4vr::isInPowerArmor() ? FlashlightLocation::OnPAHead : FlashlightLocation::OnHead;
        }

        // The weapon rides one-handed in the offhand and the primary hand is free: hold the light there.
        if (WeaponGripHandler::isWeaponOneHandedInOffhand()) {
            return FlashlightLocation::InPrimaryHand;
        }

        if (configLocation == FlashlightConfigLocation::InOffhand) {
            if (!WeaponGripHandler::isTwoHandedGripActive() && !WeaponGripHandler::isWeaponCarriedByOffhand()) {
                return FlashlightLocation::InOffhand;
            }
            // The offhand grips a two-handed weapon and can't hold the light. Mount it on the weapon, unless the
            // weapon-flashlight requirement is on and the weapon has no modeled flashlight — then fall back to the
            // head instead of losing the light; releasing the grip resolves back to the offhand.
            if (RestrictionHandler::isWeaponFlashlightMeshRequired() && !RestrictionHandler::isWeaponFlashlightAllowed()) {
                return f4vr::isInPowerArmor() ? FlashlightLocation::OnPAHead : FlashlightLocation::OnHead;
            }
            return FlashlightLocation::OnWeapon;
        }

        // config InPrimaryHand: an empty hand (nothing drawn) or bare fists holds the light itself; a drawn
        // weapon occupies the hand, so the light mounts on the weapon instead — unless the offhand carries the
        // weapon and the light was put in the freed hand.
        if (!RestrictionHandler::isWeaponEquipped() || f4vr::isUnarmedWeaponDrawn() || (_heldInFreePrimaryHand && WeaponGripHandler::isWeaponCarriedByOffhand())) {
            return FlashlightLocation::InPrimaryHand;
        }

        return FlashlightLocation::OnWeapon;
    }

    /**
     * Set references to the config by the current flashlight location.
     * So it will be easy to read and modify without needing to check location each time.
     */
    void FlashlightState::refreshConfigReferences()
    {
        switch (flashlightLocation) {
        case FlashlightLocation::OnHead:
            flashlightFade = &g_config.flashlightOnHeadFade;
            flashlightRadius = &g_config.flashlightOnHeadRadius;
            flashlightFov = &g_config.flashlightOnHeadFov;
            flashlightColorRed = &g_config.flashlightOnHeadColorRed;
            flashlightColorGreen = &g_config.flashlightOnHeadColorGreen;
            flashlightColorBlue = &g_config.flashlightOnHeadColorBlue;
            flashlightGoboPath = &g_config.flashlightOnHeadGoboPath;
            break;

        case FlashlightLocation::OnPAHead:
            flashlightFade = &g_config.flashlightOnPAHeadFade;
            flashlightRadius = &g_config.flashlightOnPAHeadRadius;
            flashlightFov = &g_config.flashlightOnPAHeadFov;
            flashlightColorRed = &g_config.flashlightOnPAHeadColorRed;
            flashlightColorGreen = &g_config.flashlightOnPAHeadColorGreen;
            flashlightColorBlue = &g_config.flashlightOnPAHeadColorBlue;
            flashlightGoboPath = &g_config.flashlightOnPAHeadGoboPath;
            break;

        case FlashlightLocation::InOffhand:
        case FlashlightLocation::InPrimaryHand:
            flashlightFade = &g_config.flashlightInHandFade;
            flashlightRadius = &g_config.flashlightInHandRadius;
            flashlightFov = &g_config.flashlightInHandFov;
            flashlightColorRed = &g_config.flashlightInHandColorRed;
            flashlightColorGreen = &g_config.flashlightInHandColorGreen;
            flashlightColorBlue = &g_config.flashlightInHandColorBlue;
            flashlightGoboPath = &g_config.flashlightInHandGoboPath;
            break;

        case FlashlightLocation::OnWeapon:
            flashlightFade = &g_config.flashlightOnWeaponFade;
            flashlightRadius = &g_config.flashlightOnWeaponRadius;
            flashlightFov = &g_config.flashlightOnWeaponFov;
            flashlightColorRed = &g_config.flashlightOnWeaponColorRed;
            flashlightColorGreen = &g_config.flashlightOnWeaponColorGreen;
            flashlightColorBlue = &g_config.flashlightOnWeaponColorBlue;
            flashlightGoboPath = &g_config.flashlightOnWeaponGoboPath;
            break;
        }
    }
}
