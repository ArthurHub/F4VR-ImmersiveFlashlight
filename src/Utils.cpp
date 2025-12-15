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
            light->fade = g_config.flashlightInOffhandFade;
            light->data.radius = g_config.flashlightInOffhandRadius;
            light->data.fov = g_config.flashlightInOffhandFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightInOffhandColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightInOffhandColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightInOffhandColorBlue);
            break;
        case FlashlightLocation::InPrimaryHand:
            light->fade = g_config.flashlightInPrimaryHandFade;
            light->data.radius = g_config.flashlightInPrimaryHandRadius;
            light->data.fov = g_config.flashlightInPrimaryHandFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightInPrimaryHandColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightInPrimaryHandColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightInPrimaryHandColorBlue);
            break;
        }
    }
}
