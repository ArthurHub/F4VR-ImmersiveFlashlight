#include "ImmersiveFlashlight.h"

#include "api/FRIKApi.h"

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

namespace ImFl
{
    /**
     * Run F4SE plugin load and initialize the plugin given the init handle.
     */
    void ImmersiveFlashlight::onModLoaded(const F4SE::LoadInterface*)
    {
        //noop
    }

    /**
     * On game fully loaded initialize things that should be initialized only once.
     */
    void ImmersiveFlashlight::onGameLoaded()
    {
        addEmbeddedFlashlightKeyword();

        _flashlight = std::make_unique<Flashlight>();

        _frikInitialized = registerOpenConfigViaFRIK();
        if (_frikInitialized) {
            _messaging->RegisterListener(onFRIKMessage, frik::api::FRIKApi::FRIK_F4SE_MOD_NAME);
        }
    }

    /**
     * Game session can be initialized multiple times as it is fired on new game and save loaded events.
     */
    void ImmersiveFlashlight::onGameSessionLoaded()
    {
        // noop
    }

    /**
     * On every frame if player is initialized.
     */
    void ImmersiveFlashlight::onFrameUpdate()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->loadedData || !_flashlight) {
            logger::sample(3000, "no player data - noop");
            return;
        }

        _flashlight->onFrameUpdate();
    }

    /**
     * Add the keyword to have mining helmet style flashlight to the player.
     */
    void ImmersiveFlashlight::addEmbeddedFlashlightKeyword()
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
     * Register for FRIK main config to have a button to open this mod config.
     */
    bool ImmersiveFlashlight::registerOpenConfigViaFRIK()
    {
        const int err = frik::api::FRIKApi::initialize();
        if (err != 0) {
            logger::error("FRIK API init failed with error: {}!", err);
            return false;
        }
        logger::info("FRIK (v{}) API (v{}) init successful!", frik::api::FRIKApi::inst->getModVersion(), frik::api::FRIKApi::inst->getVersion());

        const std::string modName(Version::PROJECT);
        frik::api::FRIKApi::inst->registerOpenModSettingButtonToMainConfig({
            .buttonIconNifPath = modName + "\\ui_config_btn_flashlight.nif",
            .callbackReceiverName = modName,
            .callbackMessageType = 15,
        });
        return true;
    }

    /**
     * Receiving the registered open config message from FRIK.
     */
    void ImmersiveFlashlight::onFRIKMessage(F4SE::MessagingInterface::Message* aMsg)
    {
        if (aMsg->type == 15) {
            // TODO: open config
            logger::warn("ImmersiveFlashlight received FRIK request to open config!");
        } else {
            logger::error("ImmersiveFlashlight received unknown FRIK message type: {}!", aMsg->type);
        }
    }
}
