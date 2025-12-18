#include "Utils.h"

#include "Config.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    /**
     * Switch the flashlight location to the given location, update the light values, and toggle the light to apply the changes.
     */
    void Utils::switchFlashlightLocation(const FlashlightLocation location)
    {
        if (g_config.flashlightLocation == location) {
            return;
        }

        g_config.setFlashlightLocation(location);

        // toggle the flashlight to reload the light values
        Utils::toggleLightRefreshValues();
    }

    /**
     * Toggle flashlight off/on and reload the light values from config.
     */
    void Utils::toggleLightRefreshValues()
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
        light->goboTexture.textureName = *g_config.flashlightGoboPath;
        loadGoboTexture(*g_config.flashlightGoboPath);
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
}
