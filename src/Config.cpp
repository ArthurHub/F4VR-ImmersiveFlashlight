#include "Config.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
}

namespace ImFl
{
    void Config::loadIniConfigInternal(const CSimpleIniA& ini)
    {
        // TODO: load config from ini
        myConfigValue = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fMyConfigValue", 3.0));
    }
}
