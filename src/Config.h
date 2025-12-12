#pragma once

#include "ConfigBase.h"
#include "Resources.h"

namespace ImFl
{
    static const auto BASE_MOD_PATH = BASE_PATH + "\\" + std::string(Version::PROJECT);
    static const auto INI_PATH = BASE_MOD_PATH + "\\" + std::string(Version::PROJECT) + ".ini";

    enum class FlashlightLocation : uint8_t
    {
        Head = 0,
        LeftArm,
        RightArm
    };

    class Config : public ConfigBase
    {
    public:
        explicit Config() :
            ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI) {}

        void setFlashlightLocation(FlashlightLocation location);

        // Flashlight
        FlashlightLocation flashlightLocation = FlashlightLocation::Head;

        // flashlight values on head
        float flashlightHeadFade;
        int flashlightHeadRadius;
        float flashlightHeadFov;
        int flashlightHeadColorRed;
        int flashlightHeadColorGreen;
        int flashlightHeadColorBlue;
        float flashlightInHandFade;

        // flashlight values in hand
        int flashlightInHandRadius;
        float flashlightInHandFov;
        int flashlightInHandColorRed;
        int flashlightInHandColorGreen;
        int flashlightInHandColorBlue;

        // button to use to switch flashlight between head and hand
        int switchTorchButton = 2;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
