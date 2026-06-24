#include "FlashlightState.h"

#include <algorithm>
#include <cmath>

#include "Config.h"
#include "Utils.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

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
     * Switch the flashlight location to the given location, update the light values, and toggle the light to apply the changes.
     */
    void FlashlightState::switchFlashlightConfigLocation(const FlashlightConfigLocation location)
    {
        const bool inPowerArmor = f4vr::isInPowerArmor();
        const auto currentLocation = getActiveFlashlightConfigLocation();
        if (currentLocation == location) {
            return;
        }
        logger::info("Switch flashlight location {} from {} to {}",
            inPowerArmor ? "(in PA)" : "(out of PA)",
            getFlashlightConfigLocationLabel(currentLocation),
            getFlashlightConfigLocationLabel(location));
        g_config.setFlashlightLocation(location, inPowerArmor);
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
        const auto newFlashlightLocation = getFlashlightLocation();
        if (flashlightLocation == newFlashlightLocation) {
            if (_lightValuesRefreshRequired && Utils::isFlashlightOn()) {
                toggleLightRefreshValues();
            }
            return;
        }

        flashlightLocation = newFlashlightLocation;
        refreshConfigReferences();

        // toggle the flashlight to reload the light values
        toggleLightRefreshValues();
    }

    /**
     * Toggle flashlight off/on and reload the light values from config.
     */
    void FlashlightState::toggleLightRefreshValues()
    {
        const auto player = f4vr::getPlayer();
        if (!f4vr::isPipboyLightOn(player)) {
            _lightValuesRefreshRequired = true;
            return;
        }
        logger::debug("Toggle light refresh values...");
        _lightValuesRefreshRequired = false;
        f4vr::togglePipboyLight(player);
        setLightValues();
        f4vr::togglePipboyLight(player);
    }

    /**
     * Set the light values to config depending if the flashlight is in hand or on head.
     * The light object is the standard PA light.
     */
    void FlashlightState::setLightValues()
    {
        auto* light = RE::TESForm::GetFormByID<RE::TESObjectLIGH>(0xB48A0);
        if (!light) {
            logger::warn("Failed to find light object to set flashlight values");
            return;
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

        if (configLocation == FlashlightConfigLocation::InOffhand) {
            return frik::api::FRIKApi::inst && frik::api::FRIKApi::inst->isOffHandGrippingWeapon() ? FlashlightLocation::OnWeapon : FlashlightLocation::InOffhand;
        }

        if (!f4vr::IsWeaponDrawn()) {
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
