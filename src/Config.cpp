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
        flashlightOnHeadFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", 1.1));
        flashlightOnHeadRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", 2400));
        flashlightOnHeadFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", 100.0));
        flashlightOnHeadColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", 235));
        flashlightOnHeadColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", 224));
        flashlightOnHeadColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", 190));

        // In offhand flashlight defaults
        flashlightInOffhandFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInOffhandFade", 1.3));
        flashlightInOffhandRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInOffhandRadius", 6000));
        flashlightInOffhandFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInOffhandFov", 65.0));
        flashlightInOffhandColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInOffhandColorRed", 240));
        flashlightInOffhandColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInOffhandColorGreen", 230));
        flashlightInOffhandColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInOffhandColorBlue", 225));

        // In primary hand flashlight defaults
        flashlightInPrimaryHandFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInPrimaryHandFade", 1.3));
        flashlightInPrimaryHandRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInPrimaryHandRadius", 5000));
        flashlightInPrimaryHandFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInPrimaryHandFov", 55.0));
        flashlightInPrimaryHandColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInPrimaryHandColorRed", 240));
        flashlightInPrimaryHandColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInPrimaryHandColorGreen", 230));
        flashlightInPrimaryHandColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInPrimaryHandColorBlue", 225));

        // change hand / head button
        switchTorchButton = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "SwitchTorchButton", 2));
    }
}
