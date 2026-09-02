#pragma once

#include "Config.h"

#include <optional>

namespace ImFl
{
    /**
     * Static holder for the flashlight's runtime state and the transitions over it: the resolved runtime
     * location, the active grip style, the references into Config for the active location's beam values,
     * and the refresh/switch logic that pushes those values onto the game light (the low-level on/off
     * and texture/flag helpers live in Utils).
     */
    class FlashlightState
    {
    public:
        static bool isHeadMountedFlashlight();
        static bool isHandHeldFlashlight();

        static FlashlightConfigLocation getActiveFlashlightConfigLocation();
        static void switchFlashlightConfigLocation(FlashlightConfigLocation location);
        static void setFlashlightRuntimeLocationOverride(std::optional<FlashlightLocation> locationOverride);

        static void refreshFlashlightLocation();
        static void toggleLightRefreshValues();
        static void setLightValues();
        static void refreshGripStyle();

        // Whether a config-mode location override is currently forcing the runtime location (used to suppress
        // gameplay enforcement, e.g. RestrictionHandler, while previewing a location in the config UI).
        static bool isRuntimeLocationOverrideActive();
        static const char* getFlashlightConfigLocationLabel(FlashlightConfigLocation location);
        static const char* getGripStyleLabel(FlashlightGripStyle style);

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
        static FlashlightLocation getFlashlightLocation();
        static void refreshConfigReferences();

        inline static std::optional<FlashlightLocation> _runtimeLocationOverride;
    };
}
