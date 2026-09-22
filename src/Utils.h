#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    struct Utils
    {
        static constexpr const char* FLASHLIGHT_FLAGS_WITH_SHADOWS = "0000010000100001";
        static constexpr const char* FLASHLIGHT_FLAGS_NO_SHADOWS = "0100000000100001";

        static bool isVRFPSStabilizerModInstalled();
        static bool isPluginLoaded(std::string_view pluginName);
        static const char* getHandLabel(vrcf::Hand hand);
        static RE::NiTexture* loadGoboTexture(const std::string& goboFilePath);
        static bool isFlashlightOn();
        static void turnFlashlightOn();
        static void turnFlashlightOff();
        static bool areFlashlightShadowsEnabled();
        static void updateVanillaFlashlightToggleDisabled();
        static void updateKeepFlashlightOnInPipboy();
        static const char* findOpenGestureBlockingMenu();

    private:
        // An open menu with any of these flags is a "real" menu that takes the controller input (the always-open
        // HUD menus carry none of them), so it blocks the flashlight gestures unless allowed below.
        static constexpr std::uint32_t GESTURE_BLOCKING_MENU_FLAGS = std::to_underlying(RE::UI_MENU_FLAGS::kPausesGame) | std::to_underlying(RE::UI_MENU_FLAGS::kUsesCursor) |
            std::to_underlying(RE::UI_MENU_FLAGS::kUsesMenuContext) | std::to_underlying(RE::UI_MENU_FLAGS::kModal);

        // Menus that keep the gestures working even when their flags would block: the world keeps running around them.
        static constexpr std::array GESTURE_ALLOWED_MENUS{ "PipboyMenu", "PipboyHolotapeMenu", "ScopeMenu", "VATSMenu" };

        // Menus that block the gestures regardless of their flags: build mode doesn't pause the game, but the
        // trigger places and picks up objects.
        static constexpr std::array GESTURE_BLOCKING_MENUS{ "WorkshopMenu" };

        // Order-independent signature of the open menus and their blocking flags, to log the set only when it changes.
        inline static std::size_t _openMenusSignature = 0;

        inline static std::unordered_map<std::string, RE::NiTexture*> _goboTextures;

        // Tracks the applied vanilla Pipboy-light-toggle disable state (nullopt until first applied, so the
        // first call always runs) and the original "fPipboyLightDelay:Controls" value to restore on re-enable.
        inline static bool _vanillaFlashlightToggleDisabled = false;
        inline static float _originalPipboyLightDelay = -1.0f;

        // The keep-the-light-on-in-the-Pip-Boy state applied to the game code (false = the vanilla code, as loaded).
        inline static bool _keepFlashlightOnInPipboyApplied = false;
    };
}
