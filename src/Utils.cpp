#include "Utils.h"

#include "Config.h"
#include "common/CommonUtils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    /**
     * Get a readable label for a VR controller hand enum.
     */
    const char* Utils::getHandLabel(const vrcf::Hand hand)
    {
        switch (hand) {
        case vrcf::Hand::Left:
            return "Left";
        case vrcf::Hand::Right:
            return "Right";
        case vrcf::Hand::Primary:
            return "Primary";
        case vrcf::Hand::Offhand:
            return "Offhand";
        default:
            return "Unknown";
        }
    }

    /**
     * Check if the "VR FPS Stabilizer - Fallout" mod is loaded to warn the user about the incompatibility.
     */
    bool Utils::isVRFPSStabilizerModInstalled()
    {
        if (common::isDLLModLoaded("VRFPSStabilizerFallout")) {
            logger::info("Detected incompatible 'VR FPS Stabilizer - Fallout' mod");
            return true;
        }
        return false;
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
     * Is Flashlight on.
     */
    bool Utils::isFlashlightOn()
    {
        return f4vr::isPipboyLightOn(f4vr::getPlayer());
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
     * Turns flashlight off if it's on.
     */
    void Utils::turnFlashlightOff()
    {
        const auto player = f4vr::getPlayer();
        if (f4vr::isPipboyLightOn(player)) {
            f4vr::togglePipboyLight(player);
        }
    }

    /**
     * Check if flashlight shadows are currently enabled in config.
     */
    bool Utils::areFlashlightShadowsEnabled()
    {
        return g_config.flashlightFlagsBitmask != FLASHLIGHT_FLAGS_NO_SHADOWS;
    }
}
