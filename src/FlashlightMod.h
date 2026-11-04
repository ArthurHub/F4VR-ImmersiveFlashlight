#pragma once

#include "Config.h"
#include "Flashlight.h"
#include "ModBase.h"
#include "config-mode/BeamScreen.h"

namespace ImFl
{
    class FlashlightMod : public ModBase
    {
    public:
        FlashlightMod()
            : ModBase(Settings(Version::PROJECT, Version::NAME, &g_config, 64, true))
        {
            // the activation-sphere icons and config UI panels draw on the framework's overlay
            _settings.preloadRendering = true;
        }

        bool isConfigOpen() const;

        config::BeamScreen* getConfigScreen() const
        {
            return _config.get();
        }

    protected:
        virtual void onModLoaded(const F4SE::LoadInterface* f4SE) override;
        virtual void onGameLoaded() override;
        virtual void onGameSessionLoaded() override;
        virtual void onFrameUpdate() override;

    private:
        static void addEmbeddedFlashlightKeyword();
        static void initializeFRIKApi();
        void registerModsConfigEntry();
        void restoreModsConfigMenuOnConfigClose();

        std::unique_ptr<Flashlight> _flashlight;

        std::unique_ptr<config::BeamScreen> _config;

        // the config UI was opened from Mods Config FO4 VR's menu, which is shown again as it closes
        bool _openedFromModsConfig = false;
    };

    // The ONE global to rule them ALL
    inline FlashlightMod g_imFl;
}
