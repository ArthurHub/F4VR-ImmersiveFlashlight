#pragma once

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

    private:
        inline static std::unordered_map<std::string, RE::NiTexture*> _goboTextures;
    };
}
