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
            flashlightFade = &g_config.flashlightOnHeadFade;
            flashlightRadius = &g_config.flashlightOnHeadRadius;
            flashlightFov = &g_config.flashlightOnHeadFov;
            flashlightColorRed = &g_config.flashlightOnHeadColorRed;
            flashlightColorGreen = &g_config.flashlightOnHeadColorGreen;
            flashlightColorBlue = &g_config.flashlightOnHeadColorBlue;
            break;

        case FlashlightLocation::InOffhand:
            flashlightFade = &g_config.flashlightInHandFade;
            flashlightRadius = &g_config.flashlightInHandRadius;
            flashlightFov = &g_config.flashlightInHandFov;
            flashlightColorRed = &g_config.flashlightInHandColorRed;
            flashlightColorGreen = &g_config.flashlightInHandColorGreen;
            flashlightColorBlue = &g_config.flashlightInHandColorBlue;
            break;

        case FlashlightLocation::InPrimaryHand:
            flashlightFade = &g_config.flashlightOnWeaponFade;
            flashlightRadius = &g_config.flashlightOnWeaponRadius;
            flashlightFov = &g_config.flashlightOnWeaponFov;
            flashlightColorRed = &g_config.flashlightOnWeaponColorRed;
            flashlightColorGreen = &g_config.flashlightOnWeaponColorGreen;
            flashlightColorBlue = &g_config.flashlightOnWeaponColorBlue;
            break;
        }
    }
}
