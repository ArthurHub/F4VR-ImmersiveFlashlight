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

        light->fade = *g_config.flashlightFade;
        light->data.radius = *g_config.flashlightRadius;
        light->data.fov = *g_config.flashlightFov;
        light->data.color.red = static_cast<std::uint8_t>(*g_config.flashlightColorRed);
        light->data.color.green = static_cast<std::uint8_t>(*g_config.flashlightColorGreen);
        light->data.color.blue = static_cast<std::uint8_t>(*g_config.flashlightColorBlue);
    }
}
