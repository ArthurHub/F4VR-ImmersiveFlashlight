#pragma once

#include "Config.h"

#include <optional>

#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    struct Utils
    {
        static constexpr const char* FLASHLIGHT_FLAGS_WITH_SHADOWS = "0000010000100001";
        static constexpr const char* FLASHLIGHT_FLAGS_NO_SHADOWS = "0100000000100001";

        static void switchFlashlightConfigLocation(FlashlightConfigLocation location);
        static FlashlightConfigLocation getActiveFlashlightConfigLocation();
        static void setFlashlightRuntimeLocationOverride(std::optional<FlashlightLocation> locationOverride);
        static void refreshFlashlightLocation();
        static void refreshGripStyle();
        static void toggleLightRefreshValues();
        static void setLightValues();
        static bool isFlashlightOn();
        static void turnFlashlightOn();
        static void turnFlashlightOff();
        static bool isHeadMountedFlashlight();
        static bool isHandHeldFlashlight();
        static const char* getFlashlightConfigLocationLabel(FlashlightConfigLocation location);
        static const char* getGripStyleLabel(FlashlightGripStyle style);
        static bool areFlashlightShadowsEnabled();
        static bool isVRFPSStabilizerModInstalled();
        static const char* getHandLabel(vrcf::Hand hand);

        static inline FlashlightLocation flashlightLocation = FlashlightLocation::OnWeapon;
        static inline FlashlightGripStyle flashlightGripStyle = FlashlightGripStyle::Forward;

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
