#include "Config.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
}

namespace ImFl
{
    void Config::setFlashlightLocation(const FlashlightLocation location)
    {
        flashlightLocation = location;
        refreshConfigReferences();
        saveIniConfigValue(DEFAULT_SECTION, "iFlashlightLocation", static_cast<int>(flashlightLocation));
    }

    void Config::saveFlashlightValues()
    {
        CSimpleIniA ini;
        if (!loadIniFromFile(ini)) {
            return;
        }

        switch (flashlightLocation) {
        case FlashlightLocation::OnHead:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", flashlightOnHeadFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", flashlightOnHeadRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", flashlightOnHeadFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", flashlightOnHeadColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", flashlightOnHeadColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", flashlightOnHeadColorBlue);
            break;

        case FlashlightLocation::InOffhand:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", flashlightInHandFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", flashlightInHandRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", flashlightInHandFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", flashlightInHandColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", flashlightInHandColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", flashlightInHandColorBlue);
            break;

        case FlashlightLocation::InPrimaryHand:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFade", flashlightOnWeaponFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponRadius", flashlightOnWeaponRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFov", flashlightOnWeaponFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorRed", flashlightOnWeaponColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorGreen", flashlightOnWeaponColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorBlue", flashlightOnWeaponColorBlue);
            break;
        }

        saveIniToFile(ini);
    }

    /**
     * Load the default config and set the current flashlight values to the defaults.
     */
    void Config::resetFlashlightValuesToDefault() const
    {
        Config defaultConfig;
        defaultConfig.loadEmbeddedDefaultOnly();
        defaultConfig.flashlightLocation = flashlightLocation;
        defaultConfig.refreshConfigReferences();

        *flashlightFade = *defaultConfig.flashlightFade;
        *flashlightRadius = *defaultConfig.flashlightRadius;
        *flashlightFov = *defaultConfig.flashlightFov;
        *flashlightColorRed = *defaultConfig.flashlightColorRed;
        *flashlightColorGreen = *defaultConfig.flashlightColorGreen;
        *flashlightColorBlue = *defaultConfig.flashlightColorBlue;
    }

    void Config::loadIniConfigInternal(const CSimpleIniA& ini)
    {
        // Flashlight location
        flashlightLocation = static_cast<FlashlightLocation>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightLocation", 0));

        // Head-mounted flashlight defaults
        flashlightOnHeadFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", 1.1));
        flashlightOnHeadRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", 2400));
        flashlightOnHeadFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", 100.0));
        flashlightOnHeadColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", 235));
        flashlightOnHeadColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", 224));
        flashlightOnHeadColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", 190));

        // In hand flashlight defaults
        flashlightInHandFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", 1.3));
        flashlightInHandRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", 6000));
        flashlightInHandFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", 65.0));
        flashlightInHandColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", 240));
        flashlightInHandColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", 230));
        flashlightInHandColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", 225));

        // Attached to weapon flashlight defaults
        flashlightOnWeaponFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFade", 1.3));
        flashlightOnWeaponRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponRadius", 5000));
        flashlightOnWeaponFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFov", 55.0));
        flashlightOnWeaponColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorRed", 240));
        flashlightOnWeaponColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorGreen", 230));
        flashlightOnWeaponColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorBlue", 225));

        // change hand / head button
        switchTorchButton = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "SwitchTorchButton", 2));

        refreshConfigReferences();
    }

    /**
     * Set references to the config by the current flashlight location.
     * So it will be easy to read and modify without needing to check location each time.
     */
    void Config::refreshConfigReferences()
    {
        switch (g_config.flashlightLocation) {
        case FlashlightLocation::OnHead:
            flashlightFade = &flashlightOnHeadFade;
            flashlightRadius = &flashlightOnHeadRadius;
            flashlightFov = &flashlightOnHeadFov;
            flashlightColorRed = &flashlightOnHeadColorRed;
            flashlightColorGreen = &flashlightOnHeadColorGreen;
            flashlightColorBlue = &flashlightOnHeadColorBlue;
            break;

        case FlashlightLocation::InOffhand:
            flashlightFade = &flashlightInHandFade;
            flashlightRadius = &flashlightInHandRadius;
            flashlightFov = &flashlightInHandFov;
            flashlightColorRed = &flashlightInHandColorRed;
            flashlightColorGreen = &flashlightInHandColorGreen;
            flashlightColorBlue = &flashlightInHandColorBlue;
            break;

        case FlashlightLocation::InPrimaryHand:
            flashlightFade = &flashlightOnWeaponFade;
            flashlightRadius = &flashlightOnWeaponRadius;
            flashlightFov = &flashlightOnWeaponFov;
            flashlightColorRed = &flashlightOnWeaponColorRed;
            flashlightColorGreen = &flashlightOnWeaponColorGreen;
            flashlightColorBlue = &flashlightOnWeaponColorBlue;
            break;
        }
    }
}
