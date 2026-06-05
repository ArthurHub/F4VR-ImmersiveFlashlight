#include "Config.h"

#include "Utils.h"
#include "common/MatrixUtils.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
}

namespace ImFl
{
    /**
     * Persist the flashlight location for the requested power-armor state, leaving the other state's location untouched.
     */
    void Config::setFlashlightLocation(const FlashlightConfigLocation location, const bool inPowerArmor)
    {
        if (inPowerArmor) {
            flashlightConfigLocationInPA = location;
            saveIniConfigValue(DEFAULT_SECTION, "iFlashlightLocationInPA", static_cast<int>(location));
        } else {
            flashlightConfigLocation = location;
            saveIniConfigValue(DEFAULT_SECTION, "iFlashlightLocation", static_cast<int>(location));
        }
    }

    void Config::setFlashlightFlagsBitmask(const std::string& bitmask)
    {
        flashlightFlagsBitmask = bitmask;
        saveIniConfigValue(DEFAULT_SECTION, "sFlashlightFlagsBitmask", flashlightFlagsBitmask.c_str());
    }

    /**
     * Resolve the in-hand light-node transform for the given hand, grip style, and power-armor state.
     */
    const RE::NiTransform& Config::getFlashlightInHandLightTransform(const bool isOffhand, const FlashlightGripStyle grip, const bool inPowerArmor) const
    {
        const bool overhand = grip == FlashlightGripStyle::Overhand;
        if (isOffhand) {
            if (inPowerArmor) {
                return overhand ? flashlightInOffhandTransformOverhandPA : flashlightInOffhandTransformPA;
            }
            return overhand ? flashlightInOffhandTransformOverhand : flashlightInOffhandTransform;
        }
        if (inPowerArmor) {
            return overhand ? flashlightInPrimaryHandTransformOverhandPA : flashlightInPrimaryHandTransformPA;
        }
        return overhand ? flashlightInPrimaryHandTransformOverhand : flashlightInPrimaryHandTransform;
    }

    /**
     * Resolve the flashlight mesh transform for the given grip style and power-armor state.
     */
    const RE::NiTransform& Config::getFlashlightMeshTransform(const FlashlightGripStyle grip, const bool inPowerArmor) const
    {
        const bool overhand = grip == FlashlightGripStyle::Overhand;
        if (inPowerArmor) {
            return overhand ? flashlightMeshTransformOverhandPA : flashlightMeshTransformPA;
        }
        return overhand ? flashlightMeshTransformOverhand : flashlightMeshTransform;
    }

    /**
     * Resolve the FRIK hand pose for holding the flashlight for the given grip style and power-armor state.
     */
    const frik::api::FRIKApi::HandPoseData& Config::getFlashlightHandPose(const FlashlightGripStyle grip, const bool inPowerArmor) const
    {
        const bool overhand = grip == FlashlightGripStyle::Overhand;
        if (inPowerArmor) {
            return overhand ? flashlightHandPoseOverhandPA : flashlightHandPosePA;
        }
        return overhand ? flashlightHandPoseOverhand : flashlightHandPose;
    }

    void Config::saveFlashlightValues(const FlashlightLocation location)
    {
        CSimpleIniA ini;
        if (!loadIniFromFile(ini)) {
            return;
        }

        switch (location) {
        case FlashlightLocation::OnHead:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", flashlightOnHeadFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", flashlightOnHeadRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", flashlightOnHeadFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", flashlightOnHeadColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", flashlightOnHeadColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", flashlightOnHeadColorBlue);
            ini.SetValue(DEFAULT_SECTION, "sFlashlightOnHeadGoboPath", flashlightOnHeadGoboPath.c_str());
            break;

        case FlashlightLocation::OnPAHead:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnPAHeadFade", flashlightOnPAHeadFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadRadius", flashlightOnPAHeadRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightOnPAHeadFov", flashlightOnPAHeadFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadColorRed", flashlightOnPAHeadColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadColorGreen", flashlightOnPAHeadColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadColorBlue", flashlightOnPAHeadColorBlue);
            ini.SetValue(DEFAULT_SECTION, "sFlashlightOnPAHeadGoboPath", flashlightOnPAHeadGoboPath.c_str());
            break;

        case FlashlightLocation::InOffhand:
        case FlashlightLocation::InPrimaryHand:
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", flashlightInHandFade);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", flashlightInHandRadius);
            ini.SetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", flashlightInHandFov);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", flashlightInHandColorRed);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", flashlightInHandColorGreen);
            ini.SetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", flashlightInHandColorBlue);
            ini.SetValue(DEFAULT_SECTION, "sFlashlightInHandGoboPath", flashlightInHandGoboPath.c_str());
            break;

        case FlashlightLocation::OnWeapon:
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
    void Config::resetFlashlightValuesToDefault(const FlashlightLocation location)
    {
        Config defaultConfig;
        defaultConfig.loadEmbeddedDefaultOnly();

        switch (location) {
        case FlashlightLocation::OnHead:
            flashlightOnHeadFade = defaultConfig.flashlightOnHeadFade;
            flashlightOnHeadRadius = defaultConfig.flashlightOnHeadRadius;
            flashlightOnHeadFov = defaultConfig.flashlightOnHeadFov;
            flashlightOnHeadColorRed = defaultConfig.flashlightOnHeadColorRed;
            flashlightOnHeadColorGreen = defaultConfig.flashlightOnHeadColorGreen;
            flashlightOnHeadColorBlue = defaultConfig.flashlightOnHeadColorBlue;
            flashlightOnHeadGoboPath = defaultConfig.flashlightOnHeadGoboPath;
            break;
        case FlashlightLocation::OnPAHead:
            flashlightOnPAHeadFade = defaultConfig.flashlightOnPAHeadFade;
            flashlightOnPAHeadRadius = defaultConfig.flashlightOnPAHeadRadius;
            flashlightOnPAHeadFov = defaultConfig.flashlightOnPAHeadFov;
            flashlightOnPAHeadColorRed = defaultConfig.flashlightOnPAHeadColorRed;
            flashlightOnPAHeadColorGreen = defaultConfig.flashlightOnPAHeadColorGreen;
            flashlightOnPAHeadColorBlue = defaultConfig.flashlightOnPAHeadColorBlue;
            flashlightOnPAHeadGoboPath = defaultConfig.flashlightOnPAHeadGoboPath;
            break;
        case FlashlightLocation::InOffhand:
        case FlashlightLocation::InPrimaryHand:
            flashlightInHandFade = defaultConfig.flashlightInHandFade;
            flashlightInHandRadius = defaultConfig.flashlightInHandRadius;
            flashlightInHandFov = defaultConfig.flashlightInHandFov;
            flashlightInHandColorRed = defaultConfig.flashlightInHandColorRed;
            flashlightInHandColorGreen = defaultConfig.flashlightInHandColorGreen;
            flashlightInHandColorBlue = defaultConfig.flashlightInHandColorBlue;
            flashlightInHandGoboPath = defaultConfig.flashlightInHandGoboPath;
            break;
        case FlashlightLocation::OnWeapon:
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
        // Flashlight location, separate for out of / in power armor. In-PA defaults to head (PA helmet lamp).
        flashlightConfigLocation = static_cast<FlashlightConfigLocation>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightLocation", 0));
        flashlightConfigLocationInPA = static_cast<FlashlightConfigLocation>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightLocationInPA", 0));

        // Head-mounted flashlight defaults
        flashlightOnHeadFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFade", 1.1));
        flashlightOnHeadRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadRadius", 2400));
        flashlightOnHeadFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnHeadFov", 100.0));
        flashlightOnHeadColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorRed", 235));
        flashlightOnHeadColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorGreen", 224));
        flashlightOnHeadColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnHeadColorBlue", 190));
        flashlightOnHeadGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightOnHeadGoboPath", R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)");
        flashlightOnHeadTransform = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightOnHeadTransform", common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 0.0f, -5.0f, 0.0f));

        // Power armor head-mounted flashlight defaults
        flashlightOnPAHeadFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnPAHeadFade", flashlightOnHeadFade));
        flashlightOnPAHeadRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadRadius", flashlightOnHeadRadius));
        flashlightOnPAHeadFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnPAHeadFov", flashlightOnHeadFov));
        flashlightOnPAHeadColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadColorRed", flashlightOnHeadColorRed));
        flashlightOnPAHeadColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadColorGreen", flashlightOnHeadColorGreen));
        flashlightOnPAHeadColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnPAHeadColorBlue", flashlightOnHeadColorBlue));
        flashlightOnPAHeadGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightOnPAHeadGoboPath", flashlightOnHeadGoboPath.c_str());
        flashlightOnPAHeadTransform = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightOnPAHeadTransform", flashlightOnHeadTransform);

        // In hand flashlight defaults
        flashlightInHandFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFade", 1.3));
        flashlightInHandRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandRadius", 6000));
        flashlightInHandFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightInHandFov", 65.0));
        flashlightInHandColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorRed", 240));
        flashlightInHandColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorGreen", 230));
        flashlightInHandColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightInHandColorBlue", 225));
        flashlightInHandGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightInHandGoboPath", R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)");
        flashlightInOffhandTransform =
            getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInOffhandTransform", common::MatrixUtils::getTransform(5.0f, -2.0f, -2.0f, 0.0f, -30.0f, -85.0f));
        flashlightInPrimaryHandTransform =
            getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInPrimaryHandTransform", common::MatrixUtils::getTransform(5.0f, 2.0f, -2.0f, 0.0f, -30.0f, -95.0f));
        flashlightInOffhandTransformOverhand =
            getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInOffhandTransformOverhand", common::MatrixUtils::getTransform(5.0f, -2.0f, -2.0f, 0.0f, 60.0f, -85.0f));
        flashlightInPrimaryHandTransformOverhand =
            getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInPrimaryHandTransformOverhand", common::MatrixUtils::getTransform(5.0f, 2.0f, -2.0f, 0.0f, 60.0f, -95.0f));

        // Power-armor variants of the in-hand light transforms; default to the matching non-PA transform.
        flashlightInOffhandTransformPA = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInOffhandTransformPA", flashlightInOffhandTransform);
        flashlightInPrimaryHandTransformPA = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInPrimaryHandTransformPA", flashlightInPrimaryHandTransform);
        flashlightInOffhandTransformOverhandPA = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInOffhandTransformOverhandPA", flashlightInOffhandTransformOverhand);
        flashlightInPrimaryHandTransformOverhandPA =
            getTransformValue(ini, DEFAULT_SECTION, "tFlashlightInPrimaryHandTransformOverhandPA", flashlightInPrimaryHandTransformOverhand);

        // Attached to weapon flashlight defaults
        flashlightOnWeaponFade = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFade", 1.3));
        flashlightOnWeaponRadius = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponRadius", 5000));
        flashlightOnWeaponFov = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightOnWeaponFov", 55.0));
        flashlightOnWeaponColorRed = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorRed", 240));
        flashlightOnWeaponColorGreen = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorGreen", 230));
        flashlightOnWeaponColorBlue = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightOnWeaponColorBlue", 225));
        flashlightOnWeaponGoboPath = ini.GetValue(DEFAULT_SECTION, "sFlashlightOnWeaponGoboPath", R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)");

        // global to all flashlight locations
        flashlightNearDistance = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightNearDistance", 30.0));
        flashlightFlagsBitmask = ini.GetValue(DEFAULT_SECTION, "sFlashlightFlagsBitmask", "0000010000100001");
        warnAboutFPSStabilizerMod = ini.GetBoolValue(DEFAULT_SECTION, "bWarnAboutFPSStabilizerMod", true);

        // change hand / head button
        switchTorchButton = static_cast<int>(ini.GetLongValue(DEFAULT_SECTION, "SwitchTorchButton", 2));

        // flashlight mesh model in hand
        showFlashlightMesh = ini.GetBoolValue(DEFAULT_SECTION, "bShowFlashlightMesh", true);
        flashlightMeshTransform = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightMeshTransform", common::MatrixUtils::getTransform(-2.0f, 3.0f, 3.0f, 25.0f, 0.0f, 90.0f));
        flashlightMeshTransformOverhand =
            getTransformValue(ini, DEFAULT_SECTION, "tFlashlightMeshTransformOverhand", common::MatrixUtils::getTransform(-2.0f, 3.0f, 3.0f, 25.0f, 90.0f, 90.0f));
        // Power-armor variants of the mesh transforms; default to the matching non-PA transform.
        flashlightMeshTransformPA = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightMeshTransformPA", flashlightMeshTransform);
        flashlightMeshTransformOverhandPA = getTransformValue(ini, DEFAULT_SECTION, "tFlashlightMeshTransformOverhandPA", flashlightMeshTransformOverhand);

        // Hand pose applied via FRIK API while holding the flashlight.
        // Default below matches the previous per-finger curl defaults (prox=mid=dist, splay/palm zero).
        static constexpr std::array<float, 22> DEFAULT_HAND_POSE = {
            0.35f, 0.35f, 0.35f, 0.0f, // thumb
            0.20f, 0.20f, 0.20f, 0.0f, // index
            0.20f, 0.20f, 0.20f, 0.0f, // middle
            0.15f, 0.15f, 0.15f, 0.0f, // ring
            0.10f, 0.10f, 0.10f, 0.0f, // pinky
            0.0f, 0.0f // palmPitch, palmYaw
        };
        flashlightHandPose = frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, DEFAULT_SECTION, "hFlashlightHandPose", DEFAULT_HAND_POSE));

        // Overhand grip is a fist around the flashlight body — fingers fully curled by default.
        static constexpr std::array<float, 22> DEFAULT_HAND_POSE_OVERHAND = {
            0.0f, 0.0f, 0.0f, 0.0f, // thumb
            0.0f, 0.0f, 0.0f, 0.0f, // index
            0.0f, 0.0f, 0.0f, 0.0f, // middle
            0.0f, 0.0f, 0.0f, 0.0f, // ring
            0.0f, 0.0f, 0.0f, 0.0f, // pinky
            0.0f, 0.0f // palmPitch, palmYaw
        };
        flashlightHandPoseOverhand =
            frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, DEFAULT_SECTION, "hFlashlightHandPoseOverhand", DEFAULT_HAND_POSE_OVERHAND));

        // Power-armor variants of the hand poses; default to the matching non-PA pose.
        flashlightHandPosePA = frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, DEFAULT_SECTION, "hFlashlightHandPosePA", flashlightHandPose.toFloats()));
        flashlightHandPoseOverhandPA =
            frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, DEFAULT_SECTION, "hFlashlightHandPoseOverhandPA", flashlightHandPoseOverhand.toFloats()));

        // Grip-style controls
        flashlightGripMode = static_cast<FlashlightGripMode>(ini.GetLongValue(DEFAULT_SECTION, "iFlashlightGripMode", 0));
        flashlightGripOverhandTiltDegrees = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightGripOverhandTiltDegrees", 120.0));
        flashlightGripHysteresisDegrees = static_cast<float>(ini.GetDoubleValue(DEFAULT_SECTION, "fFlashlightGripHysteresisDegrees", 30.0));
    }
}
