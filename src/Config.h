#pragma once

#include "ConfigBase.h"
#include "Resources.h"

namespace ImFl
{
    static const auto BASE_MOD_PATH = BASE_PATH + "\\" + std::string(Version::PROJECT);
    static const auto INI_PATH = BASE_MOD_PATH + "\\" + std::string(Version::PROJECT) + ".ini";

    enum class FlashlightLocation : uint8_t
    {
        OnHead = 0,
        InOffhand,
        InPrimaryHand
    };

    class Config : public ConfigBase
    {
    public:
        explicit Config() :
            ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI) {}

        void setFlashlightLocation(FlashlightLocation location);

        // Flashlight
        FlashlightLocation flashlightLocation = FlashlightLocation::OnHead;

        // flashlight values on head
        float flashlightOnHeadFade = 0.0f;
        int flashlightOnHeadRadius = 0;
        float flashlightOnHeadFov = 0.0f;
        int flashlightOnHeadColorRed = 0;
        int flashlightOnHeadColorGreen = 0;
        int flashlightOnHeadColorBlue = 0;

        // flashlight values in offhand hand
        float flashlightInOffhandFade = 0.0f;
        int flashlightInOffhandRadius = 0;
        float flashlightInOffhandFov = 0.0f;
        int flashlightInOffhandColorRed = 0;
        int flashlightInOffhandColorGreen = 0;
        int flashlightInOffhandColorBlue = 0;

        // flashlight values in primary hand
        float flashlightInPrimaryHandFade = 0.0f;
        int flashlightInPrimaryHandRadius = 0;
        float flashlightInPrimaryHandFov = 0.0f;
        int flashlightInPrimaryHandColorRed = 0;
        int flashlightInPrimaryHandColorGreen = 0;
        int flashlightInPrimaryHandColorBlue = 0;

        // button to use to switch flashlight between head and hand
        int switchTorchButton = 2;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
