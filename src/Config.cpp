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
        saveIniConfigValue(DEFAULT_SECTION, "iFlashlightLocation", static_cast<int>(flashlightLocation));
    }

    void Config::loadIniConfigInternal(const CSimpleIniA& ini)
    {
        // Flashlight location
        flashlightLocation = static_cast<FlashlightLocation>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightLocation", 0));

        // Head-mounted flashlight defaults
        flashlightHeadFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightHeadFade", 1.1));
        flashlightHeadRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightHeadRadius", 2400));
        flashlightHeadFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightHeadFov", 100.0));
        flashlightHeadColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightHeadColorRed", 235));
        flashlightHeadColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightHeadColorGreen", 224));
        flashlightHeadColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightHeadColorBlue", 190));

        // In-hand flashlight defaults (same values)
        flashlightInHandFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", 1.3));
        flashlightInHandRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", 6000));
        flashlightInHandFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", 65.0));
        flashlightInHandColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", 240));
        flashlightInHandColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", 230));
        flashlightInHandColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", 225));

        // change hand / head button
        switchTorchButton = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "SwitchTorchButton", 2));
    }
}
