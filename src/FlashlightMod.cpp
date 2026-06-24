#include "FlashlightMod.h"

#include "RestrictionHandler.h"
#include "api/FRIKApi.h"
#include "vrui/UIManager.h"

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
    /**
     * TODO: think about it, is it the best way to handle this dependency indirection.
     */
    class FrameUpdateContext : public vrui::UIModAdapter
    {
    public:
        virtual RE::NiPoint3 getInteractionBoneWorldPosition() override
        {
            return FRIKApi::inst->getIndexFingerTipPosition(FRIKApi::Hand::Offhand);
        }

        virtual void setInteractionHandPointing(const bool primaryHand, const bool toPoint) override
        {
            const auto hand = primaryHand ? FRIKApi::Hand::Primary : FRIKApi::Hand::Offhand;
            if (toPoint) {
                FRIKApi::inst->setHandPoseCustomFingerPositions("InFl_Config", hand, 0, 1, 0, 0, 0);
            } else {
                FRIKApi::inst->clearHandPose("InFl_Config", hand);
            }
        }
    };

    bool FlashlightMod::isConfigOpen() const
    {
        return _flashlightConfigMode->isOpen();
    }

    /**
     * Run F4SE plugin load and initialize the plugin given the init handle.
     */
    void FlashlightMod::onModLoaded(const F4SE::LoadInterface*)
    {
        const int err = FRIKApi::initialize(4);
        if (err == 0) {
            logger::info("Disable FRIK flashlight feature");
            FRIKApi::inst->blockFeature("ImmersiveFlashlight", FRIKApi::Feature::Flashlight, true);
        }
    }

    /**
     * On game fully loaded initialize things that should be initialized only once.
     */
    void FlashlightMod::onGameLoaded()
    {
        addEmbeddedFlashlightKeyword();

        RestrictionHandler::invalidate();

        _flashlight = std::make_unique<Flashlight>();
        _flashlightConfigMode = std::make_unique<FlashlightConfigMode>();

        _frikInitialized = registerOpenConfigViaFRIK();
        if (_frikInitialized) {
            _messaging->RegisterListener(onFRIKMessage, FRIKApi::FRIK_F4SE_MOD_NAME);
        }
    }

    /**
     * Game session can be initialized multiple times as it is fired on new game and save loaded events.
     */
    void FlashlightMod::onGameSessionLoaded()
    {
        _flashlightConfigMode->closeConfigMode();
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

        FrameUpdateContext context{};
        vrui::g_uiManager->onFrameUpdate(&context);

        _flashlight->onFrameUpdate();

        _flashlightConfigMode->onFrameUpdate();

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
     * Register for FRIK main config to have a button to open this mod config.
     */
    bool FlashlightMod::registerOpenConfigViaFRIK()
    {
        const int err = FRIKApi::initialize(3);
        if (err != 0) {
            logger::error("FRIK API init failed with error: {}!", err);
            return false;
        }
        logger::info("FRIK (v{}) API (v{}) init successful!", FRIKApi::inst->getModVersion(), FRIKApi::inst->getVersion());

        static std::string modName(Version::PROJECT);
        static std::string buttonIconNifPath = modName + "\\ui-config-main\\btn-flashlight.nif";
        FRIKApi::inst->registerOpenModSettingButtonToMainConfig({
            .buttonIconNifPath = buttonIconNifPath.c_str(),
            .callbackReceiverName = modName.c_str(),
            .callbackMessageType = 15,
        });
        return true;
    }

    /**
     * Receiving the registered open config message from FRIK.
     */
    void FlashlightMod::onFRIKMessage(F4SE::MessagingInterface::Message* aMsg)
    {
        if (aMsg->type == 15) {
            g_imFl._flashlightConfigMode->openConfigMode();
        } else {
            logger::error("ImmersiveFlashlight received unknown FRIK message type: {}!", aMsg->type);
        }
    }
}
