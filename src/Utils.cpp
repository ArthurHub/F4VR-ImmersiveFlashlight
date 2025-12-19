#include "Utils.h"

#include "Config.h"
#include "api/FRIKApi.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    /**
     * Switch the flashlight location to the given location, update the light values, and toggle the light to apply the changes.
     */
    void Utils::switchFlashlightConfigLocation(const FlashlightConfigLocation location)
    {
        if (g_config.flashlightConfigLocation == location) {
            return;
        }
        g_config.setFlashlightLocation(location);
        refreshFlashlightLocation();
    }

    /**
     * Refresh the current flashlight location based on config and game state.
     * Update the config references and reload the light values if location changed.
     */
    void Utils::refreshFlashlightLocation()
    {
        const auto newFlashlightLocation = getFlashlightLocation();
        if (flashlightLocation == newFlashlightLocation) {
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
    void Utils::toggleLightRefreshValues()
    {
        const auto player = f4vr::getPlayer();
        if (!f4vr::isPipboyLightOn(player)) {
            return;
        }
        f4vr::togglePipboyLight(player);
        setLightValues();
        f4vr::togglePipboyLight(player);
    }

    /**
     * Set the light values to config depending if the flashlight is in hand or on head.
     * The light object is the standard PA light.
     */
    void Utils::setLightValues()
    {
        auto* light = RE::TESForm::GetFormByID<RE::TESObjectLIGH>(0xB48A0);
        if (!light) {
            logger::warn("Failed to find light object to set flashlight values");
            return;
        }

        light->fade = *flashlightFade;
        light->data.radius = *flashlightRadius;
        light->data.fov = *flashlightFov;
        light->data.color.red = static_cast<std::uint8_t>(*flashlightColorRed);
        light->data.color.green = static_cast<std::uint8_t>(*flashlightColorGreen);
        light->data.color.blue = static_cast<std::uint8_t>(*flashlightColorBlue);
        light->goboTexture.textureName = *flashlightGoboPath;
        loadGoboTexture(*flashlightGoboPath);
    }

    /**
     * Turns flashlight on if it's off.
     */
    void Utils::turnFlashlightOn()
    {
        const auto player = f4vr::getPlayer();
        if (!f4vr::isPipboyLightOn(player)) {
            f4vr::togglePipboyLight(player);
        }
    }

    /**
     * Load the gobo texture into the game so it will be available to the flashlight light.
     * The game caches the texture so when the path is set on "textureName" it can find it.
     * Only load each texture once.
     */
    void Utils::loadGoboTexture(const std::string& goboFilePath)
    {
        if (_goboTextures.contains(goboFilePath)) {
            return;
        }

        logger::info("Loading gobo texture: {}", goboFilePath);
        RE::NiTexture* newGoboTexture = nullptr;
        f4vr::LoadTextureByPath(goboFilePath.c_str(), 1, newGoboTexture, 0, 0, 0);
        _goboTextures[goboFilePath] = newGoboTexture;
    }

    /**
     * Get the real flashlight location based on config and current game state.
     * A flashlight cannot be in primary hand if weapon is drawn, it switches to on-weapon.
     * But only for non-melee weapons, melee weapons force flashlight to offhand.
     */
    FlashlightLocation Utils::getFlashlightLocation()
    {
        if (g_config.flashlightConfigLocation == FlashlightConfigLocation::OnHead) {
            return FlashlightLocation::OnHead;
        }

        if (g_config.flashlightConfigLocation == FlashlightConfigLocation::InOffhand) {
            return frik::api::FRIKApi::inst && frik::api::FRIKApi::inst->isOffHandGrippingWeapon() ? FlashlightLocation::OnWeapon : FlashlightLocation::InOffhand;
        }

        const auto weaponNode = f4vr::getWeaponNode();
        if (!f4vr::isNodeVisible(weaponNode)) {
            return FlashlightLocation::InPrimaryHand;
        }

        if (f4vr::isMeleeWeaponEquipped()) {
            return FlashlightLocation::InOffhand;
        }

        return FlashlightLocation::OnWeapon;
    }

    /**
     * Set references to the config by the current flashlight location.
     * So it will be easy to read and modify without needing to check location each time.
     */
    void Utils::refreshConfigReferences()
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
