#pragma once

#include "Config.h"

#include <optional>

namespace ImFl
{
    struct Utils
    {
        static void switchFlashlightConfigLocation(FlashlightConfigLocation location);
        static void setFlashlightRuntimeLocationOverride(std::optional<FlashlightLocation> locationOverride);
        static void refreshFlashlightLocation();
        static void toggleLightRefreshValues();
        static void setLightValues();
        static void turnFlashlightOn();
        static bool isHeadMountedFlashlight();

        static inline FlashlightLocation flashlightLocation = FlashlightLocation::OnWeapon;

        // References to the config values for the active runtime flashlight location.
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
        inline static std::optional<FlashlightLocation> _runtimeLocationOverride;
    };
}
