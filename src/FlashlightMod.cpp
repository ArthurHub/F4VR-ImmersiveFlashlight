#include "FlashlightMod.h"

#include "RestrictionHandler.h"
#include "WeaponGripHandler.h"
#include "api/FRIKApi.h"
#include "api/FRIKApiV2.h"
#include "devbench/FlashlightDevBench.h"
#include "f4vr/F4VRUtils.h"
#include "modsconfig/ModsConfigFO4Api.h"

// This is the entry point to the mod.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_skse, F4SE::PluginInfo* a_info)
{
    return g_mod->onF4SEPluginQuery(a_skse, a_info);
}

// This is the entry point to the mod.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    return g_mod->onF4SEPluginLoad(a_f4se);
}

using namespace frik::api;

namespace ImFl
{
    bool FlashlightMod::isConfigOpen() const
    {
        return _config->isOpen();
    }

    /**
     * Run F4SE plugin load and initialize the plugin given the init handle.
     */
    void FlashlightMod::onModLoaded(const F4SE::LoadInterface*)
    {
        devbench::setupDevBenchTool();

        const int err = FRIKApiV2::initialize();
        if (err == 0) {
            logger::info("Disable FRIK flashlight feature");
            FRIKApiV2::inst->blockFeature("ImmersiveFlashlight", FRIKApiV2::Feature::Flashlight, true);
        }

        registerModsConfigEntry();
    }

    /**
     * Adds the function of the config spec's button that opens the beam tuning in the world, the game's shadow map
     * size, which its warning under Shadows asks, and whether movement follows the controller, which enables the reversed
     * movement fix. The framework connects the client at kPostLoad, and nothing is called while Mods Config FO4 VR is not
     * installed. The menu calls a press on the game thread, and is minimized while the config UI is open, and shown again
     * as that closes. Both states are the game's settings as it runs, which its settings menu changes with no restart,
     * and not its INI file.
     */
    void FlashlightMod::registerModsConfigEntry()
    {
        using namespace mods_config::api;
        ModsConfigFO4Client::get().addState("shadowMapResolution", [] {
            const auto setting = f4vr::getIniSetting("iShadowMapResolution:Display");
            return setting ? std::optional<Value>(setting->GetInt()) : std::nullopt;
        });
        ModsConfigFO4Client::get().addState("wandDirectionalMovement", [] { return std::optional<Value>(f4vr::useWandDirectionalMovement()); });
        ModsConfigFO4Client::get().addFunction("openBeamConfig", [this](const Params&) {
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->loadedData || !_flashlight) {
                return FunctionResult{ .done = false, .message = "The beam tuning opens once a game is loaded." };
            }
            _config->open();
            _openedFromModsConfig = true;
            ModsConfigFO4Client::get().minimizeMenu();
            return FunctionResult{ .done = true, .message = {} };
        });
    }

    /**
     * Shows Mods Config FO4 VR's menu again once the config UI that was opened from it is closed.
     */
    void FlashlightMod::restoreModsConfigMenuOnConfigClose()
    {
        if (_openedFromModsConfig && !_config->isOpen()) {
            _openedFromModsConfig = false;
            mods_config::api::ModsConfigFO4Client::get().restoreMenu();
        }
    }

    /**
     * On game fully loaded initialize things that should be initialized only once.
     */
    void FlashlightMod::onGameLoaded()
    {
        addEmbeddedFlashlightKeyword();

        RestrictionHandler::invalidate();
        WeaponGripHandler::initialize();

        _flashlight = std::make_unique<Flashlight>();
        _config = std::make_unique<config::BeamScreen>();

        initializeFRIKApi();
    }

    /**
     * Game session can be initialized multiple times as it is fired on new game and save loaded events.
     */
    void FlashlightMod::onGameSessionLoaded()
    {
        _config->close();
        _openedFromModsConfig = false;
        if (_flashlight) {
            _flashlight->onGameSessionLoaded();
        }
    }

    /**
     * On every frame if player is initialized.
     */
    void FlashlightMod::onFrameUpdate()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->loadedData || !_flashlight) {
            logger::sample(3000, "no player data - noop");
            return;
        }

        _flashlight->onFrameUpdate();

        _config->onFrameUpdate();
        restoreModsConfigMenuOnConfigClose();

        // Debug: dump the Immersive-rule headgear allow/block classification on request (one-shot INI token).
        if (g_config.checkDebugDumpDataOnceFor("headgear")) {
            RestrictionHandler::dumpHeadgear();
        }
    }

    /**
     * Add the keyword to have mining helmet style flashlight to the player.
     */
    void FlashlightMod::addEmbeddedFlashlightKeyword()
    {
        if (auto* armorObj = RE::TESForm::GetFormByID<RE::TESObjectARMO>(0x21B3B)) {
            if (const auto keywordObj = RE::TESForm::GetFormByID<RE::BGSKeyword>(0xB34A6)) {
                if (!armorObj->HasKeyword(keywordObj)) {
                    logger::info("Add embedded flashlight keyword to: '{}', keyword: 0x{:x}", armorObj->GetFullName(), keywordObj->formID);
                    armorObj->AddKeyword(keywordObj);
                } else {
                    logger::info("Embedded flashlight keyword already exists in '{}'", armorObj->GetFullName());
                }
            } else {
                logger::warn("Failed to add embedded flashlight, keyword not found");
            }
        } else {
            logger::warn("Failed to add embedded flashlight, armor not found");
        }
    }

    /**
     * Initialize FRIK's API, which the hand poses and the two-handed weapon grip use: without it FRIKApi::inst stays null.
     */
    void FlashlightMod::initializeFRIKApi()
    {
        const int err = FRIKApi::initialize(3);
        if (err != 0) {
            logger::error("FRIK API init failed with error: {}!", err);
            return;
        }
        logger::info("FRIK (v{}) API (v{}) init successful!", FRIKApi::inst->getModVersion(), FRIKApi::inst->getVersion());
    }
}
