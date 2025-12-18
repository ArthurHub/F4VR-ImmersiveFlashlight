#pragma once
#include "Config.h"

namespace ImFl
{
    struct Utils
    {
        static void switchFlashlightLocation(FlashlightLocation location);
        static void toggleLightRefreshValues();
        static void setLightValues();

    private:
        static void loadGoboTexture(const std::string& goboFilePath);

        inline static std::unordered_map<std::string, RE::NiTexture*> _goboTextures;
    };
}
