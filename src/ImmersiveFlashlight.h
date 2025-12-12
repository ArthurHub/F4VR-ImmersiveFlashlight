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

        std::unique_ptr<Flashlight> _flashlight;
    };

    // The ONE global to rule them ALL
    inline ImmersiveFlashlight g_imFl;
}
