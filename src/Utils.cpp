#include "Utils.h"

#include <algorithm>
#include <format>

#include "Config.h"
#include "common/CommonUtils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/F4VRUtils.h"
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
     * Whether the given content plugin (.esp/.esm) is present in the current load order. Matched by filename
     * load-order-independently via TESDataHandler, so it works regardless of the plugin's index.
     */
    bool Utils::isPluginLoaded(const std::string_view pluginName)
    {
        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && dataHandler->LookupLoadedModByName(pluginName) != nullptr;
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

    /**
     * Enable or disable the vanilla game's global flashlight (Pipboy light) toggle by pushing the
     * "fPipboyLightDelay:Controls" game setting.
     */
    void Utils::updateVanillaFlashlightToggleDisabled()
    {
        if (_originalPipboyLightDelay <= 0) {
            const auto setting = f4vr::getIniSetting("fPipboyLightDelay:Controls", true);
            if (!setting) {
                return;
            }
            _originalPipboyLightDelay = setting->GetFloat();
        }

        if (_vanillaFlashlightToggleDisabled == g_config.disableVanillaFlashlightToggle) {
            return;
        }
        const auto setting = f4vr::getIniSetting("fPipboyLightDelay:Controls", true);
        if (g_config.disableVanillaFlashlightToggle) {
            logger::info("Disable vanilla flashlight toggle");
            setting->SetFloat(99);
        } else {
            logger::info("Restore vanilla flashlight toggle ({})", _originalPipboyLightDelay);
            setting->SetFloat(_originalPipboyLightDelay);
        }
        _vanillaFlashlightToggleDisabled = g_config.disableVanillaFlashlightToggle;
    }

    /**
     * The name of the first open menu that blocks the flashlight gestures, or null when none does. A menu blocks
     * when it is in GESTURE_BLOCKING_MENUS, or carries any GESTURE_BLOCKING_MENU_FLAGS and isn't in
     * GESTURE_ALLOWED_MENUS — so menus added by other mods block too, without being named. The always-open HUD
     * menus carry none of those flags.
     * Every change to the set of open menus (or their blocking flags) is logged with each menu's flags, since the
     * flag rule is the part that could misjudge a VR menu. The returned name lives in the game's string pool.
     */
    const char* Utils::findOpenGestureBlockingMenu()
    {
        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return nullptr;
        }

        const auto matchesAny = [](const char* name, const auto& names) {
            return std::ranges::any_of(names, [name](const char* candidate) { return _stricmp(name, candidate) == 0; });
        };

        RE::BSAutoReadLock lock{ RE::UI::GetMenuMapRWLock() };
        const auto forEachOpenMenu = [ui](const auto& visit) {
            for (const auto& [menuName, entry] : ui->menuMap) {
                if (entry.menu && entry.menu->OnStack() && menuName.c_str()) {
                    visit(menuName.c_str(), entry.menu->menuFlags.underlying());
                }
            }
        };

        const char* blockingMenu = nullptr;
        std::size_t signature = 0;
        forEachOpenMenu([&](const char* name, const std::uint32_t flags) {
            const auto blockingFlags = flags & GESTURE_BLOCKING_MENU_FLAGS;
            signature ^= std::hash<std::string_view>{}(name)*31 + blockingFlags;
            if (!blockingMenu && (matchesAny(name, GESTURE_BLOCKING_MENUS) || (blockingFlags != 0 && !matchesAny(name, GESTURE_ALLOWED_MENUS)))) {
                blockingMenu = name;
            }
        });

        if (signature != _openMenusSignature) {
            _openMenusSignature = signature;
            std::string openMenus;
            forEachOpenMenu([&](const char* name, const std::uint32_t flags) { openMenus += std::format(" {}[0x{:X}]", name, flags); });
            logger::info("Open menus changed (gestures blocked by: {}):{}", blockingMenu ? blockingMenu : "none", openMenus);
        }
        return blockingMenu;
    }
}
