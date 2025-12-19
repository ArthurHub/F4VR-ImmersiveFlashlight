#include "Config.h"

#include "Utils.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
}

namespace ImFl
{
    void Config::setFlashlightLocation(const FlashlightConfigLocation location)
    {
        flashlightConfigLocation = location;
        saveIniConfigValue(DEFAULT_SECTION, "iFlashlightLocation", static_cast<int>(flashlightConfigLocation));
    }

    void Config::saveFlashlightValues()
    {
        CSimpleIniA ini;
        if (!loadIniFromFile(ini)) {
            return;
        }

        switch (flashlightConfigLocation) {
        case FlashlightConfigLocation::OnHead:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", flashlightOnHeadFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", flashlightOnHeadRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", flashlightOnHeadFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", flashlightOnHeadColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", flashlightOnHeadColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", flashlightOnHeadColorBlue);
            ini.SetValue(DEFAULT_SECTION, "sFlashlightOnHeadGoboPath", flashlightOnHeadGoboPath.c_str());
            break;

        case FlashlightConfigLocation::InOffhand:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", flashlightInHandFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", flashlightInHandRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", flashlightInHandFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", flashlightInHandColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", flashlightInHandColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", flashlightInHandColorBlue);
            ini.SetValue(DEFAULT_SECTION, "sFlashlightInHandGoboPath", flashlightInHandGoboPath.c_str());
            break;

        case FlashlightConfigLocation::InPrimaryHand:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFade", flashlightOnWeaponFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponRadius", flashlightOnWeaponRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFov", flashlightOnWeaponFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorRed", flashlightOnWeaponColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorGreen", flashlightOnWeaponColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorBlue", flashlightOnWeaponColorBlue);
            ini.SetValue(DEFAULT_SECTION, "sFlashlightOnWeaponGoboPath", flashlightOnWeaponGoboPath.c_str());
            break;
        }

        saveIniToFile(ini);
    }

    /**
     * Load the default config and set the current flashlight values to the defaults.
     */
    void Config::resetFlashlightValuesToDefault()
    {
        Config defaultConfig;
        defaultConfig.loadEmbeddedDefaultOnly();

        switch (flashlightConfigLocation) {
        case FlashlightConfigLocation::OnHead:
            flashlightOnHeadFade = defaultConfig.flashlightOnHeadFade;
            flashlightOnHeadRadius = defaultConfig.flashlightOnHeadRadius;
            flashlightOnHeadFov = defaultConfig.flashlightOnHeadFov;
            flashlightOnHeadColorRed = defaultConfig.flashlightOnHeadColorRed;
            flashlightOnHeadColorGreen = defaultConfig.flashlightOnHeadColorGreen;
            flashlightOnHeadColorBlue = defaultConfig.flashlightOnHeadColorBlue;
            flashlightOnHeadGoboPath = defaultConfig.flashlightOnHeadGoboPath;
            break;
        case FlashlightConfigLocation::InOffhand:
            flashlightInHandFade = defaultConfig.flashlightInHandFade;
            flashlightInHandRadius = defaultConfig.flashlightInHandRadius;
            flashlightInHandFov = defaultConfig.flashlightInHandFov;
            flashlightInHandColorRed = defaultConfig.flashlightInHandColorRed;
            flashlightInHandColorGreen = defaultConfig.flashlightInHandColorGreen;
            flashlightInHandColorBlue = defaultConfig.flashlightInHandColorBlue;
            flashlightInHandGoboPath = defaultConfig.flashlightInHandGoboPath;
            break;
        case FlashlightConfigLocation::InPrimaryHand:
            flashlightOnWeaponFade = defaultConfig.flashlightOnWeaponFade;
            flashlightOnWeaponRadius = defaultConfig.flashlightOnWeaponRadius;
            flashlightOnWeaponFov = defaultConfig.flashlightOnWeaponFov;
            flashlightOnWeaponColorRed = defaultConfig.flashlightOnWeaponColorRed;
            flashlightOnWeaponColorGreen = defaultConfig.flashlightOnWeaponColorGreen;
            flashlightOnWeaponColorBlue = defaultConfig.flashlightOnWeaponColorBlue;
            flashlightOnWeaponGoboPath = defaultConfig.flashlightOnWeaponGoboPath;
            break;
        }
    }

    void Config::loadIniConfigInternal(const CSimpleIniA& ini)
    {
        // Flashlight location
        flashlightConfigLocation = static_cast<FlashlightConfigLocation>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightLocation", 0));

        // Head-mounted flashlight defaults
        flashlightOnHeadFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", 1.1));
        flashlightOnHeadRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", 2400));
        flashlightOnHeadFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", 100.0));
        flashlightOnHeadColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", 235));
        flashlightOnHeadColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", 224));
        flashlightOnHeadColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", 190));
        flashlightOnHeadGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightOnHeadGoboPath", R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)");

        // In hand flashlight defaults
        flashlightInHandFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", 1.3));
        flashlightInHandRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", 6000));
        flashlightInHandFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", 65.0));
        flashlightInHandColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", 240));
        flashlightInHandColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", 230));
        flashlightInHandColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", 225));
        flashlightInHandGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightInHandGoboPath", R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)");

        // Attached to weapon flashlight defaults
        flashlightOnWeaponFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFade", 1.3));
        flashlightOnWeaponRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponRadius", 5000));
        flashlightOnWeaponFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFov", 55.0));
        flashlightOnWeaponColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorRed", 240));
        flashlightOnWeaponColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorGreen", 230));
        flashlightOnWeaponColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorBlue", 225));
        flashlightOnWeaponGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightOnWeaponGoboPath", R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)");

        // change hand / head button
        switchTorchButton = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "SwitchTorchButton", 2));
    }
}
