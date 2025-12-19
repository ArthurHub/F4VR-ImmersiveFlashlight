#pragma once

#include "Config.h"

namespace ImFl
{
    /**
     * The possible flashlight locations in-game.
     */
    enum class FlashlightLocation : uint8_t
    {
        OnHead = 0,
        InOffhand,
        InPrimaryHand,
        OnWeapon
    };

    struct Utils
    {
        static void switchFlashlightConfigLocation(FlashlightConfigLocation location);
        static void refreshFlashlightLocation();
        static void toggleLightRefreshValues();
        static void setLightValues();
        static void turnFlashlightOn();

        static inline FlashlightLocation flashlightLocation = FlashlightLocation::OnWeapon;

        // ref configs to specific flashlight values
        static inline float* flashlightFade = nullptr;
        static inline int* flashlightRadius = nullptr;
        static inline float* flashlightFov = nullptr;
        static inline int* flashlightColorRed = nullptr;
        static inline int* flashlightColorGreen = nullptr;
        static inline int* flashlightColorBlue = nullptr;
        static inline std::string* flashlightGoboPath = nullptr;

    private:
        static void loadGoboTexture(const std::string& goboFilePath);
        static FlashlightLocation getFlashlightLocation();
        static void refreshConfigReferences();

        inline static std::unordered_map<std::string, RE::NiTexture*> _goboTextures;
    };
}
