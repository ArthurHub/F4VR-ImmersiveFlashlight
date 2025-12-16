#include "Utils.h"

#include "Config.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    /**
     * Toggle flashlight off/on and reload the light values from config.
     */
    void Utils::toggleLightsRefreshValues()
    {
        const auto player = f4vr::getPlayer();
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

        switch (g_config.flashlightLocation) {
        case FlashlightLocation::OnHead:
            light->fade = g_config.flashlightOnHeadFade;
            light->data.radius = g_config.flashlightOnHeadRadius;
            light->data.fov = g_config.flashlightOnHeadFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightOnHeadColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightOnHeadColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightOnHeadColorBlue);
            break;
        case FlashlightLocation::InOffhand:
            light->fade = g_config.flashlightInHandFade;
            light->data.radius = g_config.flashlightInHandRadius;
            light->data.fov = g_config.flashlightInHandFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightInHandColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightInHandColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightInHandColorBlue);
            break;
        case FlashlightLocation::InPrimaryHand:
            light->fade = g_config.flashlightOnWeaponFade;
            light->data.radius = g_config.flashlightOnWeaponRadius;
            light->data.fov = g_config.flashlightOnWeaponFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightOnWeaponColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightOnWeaponColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightOnWeaponColorBlue);
            break;
        }
    }
}
