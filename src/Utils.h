#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    struct Utils
    {
        static constexpr const char* FLASHLIGHT_FLAGS_WITH_SHADOWS = "0000010000100001";
        static constexpr const char* FLASHLIGHT_FLAGS_NO_SHADOWS = "0100000000100001";

        static bool isVRFPSStabilizerModInstalled();
        static const char* getHandLabel(vrcf::Hand hand);
        static void loadGoboTexture(const std::string& goboFilePath);
        static bool isFlashlightOn();
        static void turnFlashlightOn();
        static void turnFlashlightOff();
        static bool areFlashlightShadowsEnabled();
        static void updateVanillaFlashlightToggleDisabled();

    private:
        inline static std::unordered_map<std::string, RE::NiTexture*> _goboTextures;

        // Tracks the applied vanilla Pipboy-light-toggle disable state (nullopt until first applied, so the
        // first call always runs) and the original "fPipboyLightDelay:Controls" value to restore on re-enable.
        inline static std::optional<bool> _vanillaFlashlightToggleDisabled;
        inline static float _originalPipboyLightDelay = -1.0f;
    };
}
