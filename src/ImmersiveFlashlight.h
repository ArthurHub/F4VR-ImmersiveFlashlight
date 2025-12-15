#pragma once

#include "Config.h"
#include "Flashlight.h"
#include "ModBase.h"

namespace ImFl
{
    class ImmersiveFlashlight : public ModBase
    {
    public:
        ImmersiveFlashlight() :
            ModBase(Settings(
                Version::PROJECT,
                Version::NAME,
                &g_config,
                32,
                true)) {}

    protected:
        virtual void onModLoaded(const F4SE::LoadInterface* f4SE) override;
        virtual void onGameLoaded() override;
        virtual void onGameSessionLoaded() override;
        virtual void onFrameUpdate() override;

    private:
        static void addEmbeddedFlashlightKeyword();
        static bool registerOpenConfigViaFRIK();
        static void onFRIKMessage(F4SE::MessagingInterface::Message* aMsg);

        std::unique_ptr<Flashlight> _flashlight;

        // check if FRIK mod detected and initialized to open config via FRIK config
        bool _frikInitialized = false;
    };

    // The ONE global to rule them ALL
    inline ImmersiveFlashlight g_imFl;
}
