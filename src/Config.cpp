#include "Config.h"

#include <algorithm>
#include <sstream>

#include "Utils.h"
#include "common/MatrixUtils.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();

    // Per-sphere INI sections for the activation-sphere gestures (WandActivationConfig).
    constexpr const char* SECTION_BODY_ACTIVATION_SPHERE = "ImFl_BodyActivationSphere";
    constexpr const char* SECTION_HEAD_ACTIVATION_SPHERE = "ImFl_HeadActivationSphere";
    constexpr const char* SECTION_PRIMARY_HAND_ACTIVATION_SPHERE = "ImFl_PrimaryHandActivationSphere";

    // Named INI sections for the advanced, per-feature settings (moved out of the main [ImmersiveFlashlightVR]
    // section in config version 15). Location, beam appearance and the global light/toggle keys stay in the
    // main section (DEFAULT_SECTION); only the advanced groups are read from these.
    constexpr const char* SECTION_HAND_MOUNT = "ImFl_HandMount";
    constexpr const char* SECTION_HEADGEAR_RESTRICTION = "ImFl_HeadgearRestriction";
    constexpr const char* SECTION_WEAPON_MOUNT = "ImFl_WeaponMount";
    constexpr const char* SECTION_BODY_STOW = "ImFl_BodyStow";
    constexpr const char* SECTION_NPC_DETECTION = "ImFl_NpcDetection";

    /**
     * Split a comma-separated INI value into trimmed, non-empty tokens.
     */
    std::vector<std::string> parseCommaList(const std::string& raw)
    {
        std::vector<std::string> out;
        std::stringstream ss(raw);
        std::string item;
        while (std::getline(ss, item, ',')) {
            const auto token = common::trim(item);
            if (!token.empty()) {
                out.push_back(token);
            }
        }
        return out;
    }

    /**
     * Parse a comma-separated list of "localFormID|plugin" entries into (local FormID, plugin) pairs.
     * The FormID is the local id as shown in xEdit (hex, with or without "0x", load-order prefix dropped).
     * Malformed entries (missing '|' or unparsable id) are skipped with a warning.
     */
    std::vector<std::pair<std::uint32_t, std::string>> parseFormPluginList(const std::string& raw)
    {
        std::vector<std::pair<std::uint32_t, std::string>> out;
        for (const auto& entry : parseCommaList(raw)) {
            const auto pipe = entry.find('|');
            if (pipe == std::string::npos) {
                logger::warn("Ignoring malformed form list entry (expected 'formID|plugin'): '{}'", entry);
                continue;
            }
            const auto idStr = common::trim(entry.substr(0, pipe));
            const auto plugin = common::trim(entry.substr(pipe + 1));
            if (idStr.empty() || plugin.empty()) {
                logger::warn("Ignoring malformed form list entry (empty formID or plugin): '{}'", entry);
                continue;
            }
            try {
                const auto localId = static_cast<std::uint32_t>(std::stoul(idStr, nullptr, 16));
                out.emplace_back(localId, plugin);
            } catch (const std::exception&) {
                logger::warn("Ignoring form list entry with unparsable formID: '{}'", entry);
            }
        }
        return out;
    }

    /**
     * Parse a hex Havok collision-filter word ("02420028", with or without "0x"). The low 7 bits are the
     * COL_LAYER; the high bits carry the collision group.
     */
    std::uint32_t parseCollisionFilter(const std::string& raw, const std::uint32_t defaultValue)
    {
        const auto value = common::trim(raw);
        if (value.empty()) {
            return defaultValue;
        }
        try {
            return static_cast<std::uint32_t>(std::stoul(value, nullptr, 16));
        } catch (const std::exception&) {
            logger::warn("Ignoring unparsable collision filter value: '{}'", value);
            return defaultValue;
        }
    }
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
     * Persist the "show all activation spheres" master toggle. Read live by Flashlight each frame.
     */
    void Config::setShowAllActivationSpheres(const bool show)
    {
        showAllActivationSpheres = show;
        saveIniConfigValue(DEFAULT_SECTION, "bShowAllActivationSpheres", show);
    }

    /**
     * Persist the stowed-on-body model toggle. Read live by Flashlight::updateBodyStow each frame.
     */
    void Config::setShowFlashlightOnBody(const bool show)
    {
        showFlashlightOnBody = show;
        saveIniConfigValue(SECTION_BODY_STOW, "bShowFlashlightOnBody", show);
    }

    /**
     * Persist the head-mounted-light headgear requirement. Read live by RestrictionHandler each frame; the
     * Immersive rule reuses the form lists already resolved at load (no re-resolve needed to switch modes).
     */
    void Config::setFlashlightHeadgearRequirement(const FlashlightHeadgearRequirement requirement)
    {
        flashlightHeadgearRequirement = requirement;
        saveIniConfigValue(SECTION_HEADGEAR_RESTRICTION, "iFlashlightHeadgearRequirement", static_cast<int>(requirement));
    }

    /**
     * Persist the weapon-flashlight-mesh requirement. The caller must follow with RestrictionHandler::invalidate()
     * so the mesh-detection cache and resolved effective state re-scan for the new value (programmatic saves
     * don't fire the hot-reload).
     */
    void Config::setWeaponFlashlightMeshRequirement(const FlashlightWeaponMeshRequirement requirement)
    {
        weaponFlashlightMeshRequirement = requirement;
        saveIniConfigValue(SECTION_WEAPON_MOUNT, "iWeaponFlashlightMeshRequired", static_cast<int>(requirement));
    }

    /**
     * Persist the vanilla-flashlight-toggle disable. The caller must follow with
     * Utils::updateVanillaFlashlightToggleDisabled() to apply/restore the game setting live.
     */
    void Config::setDisableVanillaFlashlightToggle(const bool disabled)
    {
        disableVanillaFlashlightToggle = disabled;
        saveIniConfigValue(DEFAULT_SECTION, "bDisableVanillaFlashlightToggle", disabled);
    }

    /**
     * Persist the NPC light-detection master toggle. Read live by NpcDetectionHandler on each tick.
     */
    void Config::setNpcDetectionEnabled(const bool enabled)
    {
        npcDetectionEnabled = enabled;
        saveIniConfigValue(SECTION_NPC_DETECTION, "bNpcDetectionEnabled", enabled);
    }

    /**
     * Resolve the in-hand light-node transform for the given hand and grip style.
     */
    const RE::NiTransform& Config::getFlashlightInHandLightTransform(const bool isOffhand, const FlashlightGripStyle grip) const
    {
        const bool overhand = grip == FlashlightGripStyle::Overhand;
        if (isOffhand) {
            return overhand ? flashlightInOffhandTransformOverhand : flashlightInOffhandTransform;
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

    /**
     * Resolve the stowed-on-body mesh transform for the given power-armor state.
     */
    const RE::NiTransform& Config::getFlashlightBodyTransform(const bool inPowerArmor) const
    {
        return inPowerArmor ? flashlightBodyTransformPA : flashlightBodyTransform;
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
            getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightInOffhandTransform", common::MatrixUtils::getTransform(5.0f, -2.0f, -2.0f, 0.0f, -30.0f, -85.0f));
        flashlightInPrimaryHandTransform =
            getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightInPrimaryHandTransform", common::MatrixUtils::getTransform(5.0f, 2.0f, -2.0f, 0.0f, -30.0f, -95.0f));
        flashlightInOffhandTransformOverhand =
            getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightInOffhandTransformOverhand", common::MatrixUtils::getTransform(5.0f, -2.0f, -2.0f, 0.0f, 60.0f, -85.0f));
        flashlightInPrimaryHandTransformOverhand =
            getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightInPrimaryHandTransformOverhand", common::MatrixUtils::getTransform(5.0f, 2.0f, -2.0f, 0.0f, 60.0f, -95.0f));

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

        // flashlight mesh model in hand
        showFlashlightMesh = ini.GetBoolValue(SECTION_HAND_MOUNT, "bShowFlashlightMesh", true);
        flashlightMeshTransform = getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightMeshTransform", common::MatrixUtils::getTransform(-2.0f, 3.0f, 3.0f, 25.0f, 0.0f, 90.0f));
        flashlightMeshTransformOverhand =
            getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightMeshTransformOverhand", common::MatrixUtils::getTransform(-2.0f, 3.0f, 3.0f, 25.0f, 90.0f, 90.0f));
        // Power-armor variants of the mesh transforms; default to the matching non-PA transform.
        flashlightMeshTransformPA = getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightMeshTransformPA", flashlightMeshTransform);
        flashlightMeshTransformOverhandPA = getTransformValue(ini, SECTION_HAND_MOUNT, "tFlashlightMeshTransformOverhandPA", flashlightMeshTransformOverhand);

        static constexpr std::array<float, 22> DEFAULT_HAND_POSE = {
            0.35f,
            0.35f,
            0.35f,
            0.0f, // thumb
            0.20f,
            0.20f,
            0.20f,
            0.0f, // index
            0.20f,
            0.20f,
            0.20f,
            0.0f, // middle
            0.15f,
            0.15f,
            0.15f,
            0.0f, // ring
            0.10f,
            0.10f,
            0.10f,
            0.0f, // pinky
            0.0f,
            0.0f // palmPitch, palmYaw
        };
        flashlightHandPose = frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, SECTION_HAND_MOUNT, "hFlashlightHandPose", DEFAULT_HAND_POSE));

        // Overhand grip is a fist around the flashlight body — fingers fully curled by default.
        static constexpr std::array<float, 22> DEFAULT_HAND_POSE_OVERHAND = {
            0.0f,
            0.0f,
            0.0f,
            0.0f, // thumb
            0.0f,
            0.0f,
            0.0f,
            0.0f, // index
            0.0f,
            0.0f,
            0.0f,
            0.0f, // middle
            0.0f,
            0.0f,
            0.0f,
            0.0f, // ring
            0.0f,
            0.0f,
            0.0f,
            0.0f, // pinky
            0.0f,
            0.0f // palmPitch, palmYaw
        };
        flashlightHandPoseOverhand =
            frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, SECTION_HAND_MOUNT, "hFlashlightHandPoseOverhand", DEFAULT_HAND_POSE_OVERHAND));

        flashlightHandPosePA = frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, SECTION_HAND_MOUNT, "hFlashlightHandPosePA", flashlightHandPose.toFloats()));
        flashlightHandPoseOverhandPA =
            frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(ini, SECTION_HAND_MOUNT, "hFlashlightHandPoseOverhandPA", flashlightHandPoseOverhand.toFloats()));

        flashlightGripMode = static_cast<FlashlightGripMode>(ini.GetLongValue(SECTION_HAND_MOUNT, "iFlashlightGripMode", 0));
        flashlightGripOverhandTiltDegrees = static_cast<float>(ini.GetDoubleValue(SECTION_HAND_MOUNT, "fFlashlightGripOverhandTiltDegrees", 120.0));
        flashlightGripHysteresisDegrees = static_cast<float>(ini.GetDoubleValue(SECTION_HAND_MOUNT, "fFlashlightGripHysteresisDegrees", 30.0));

        flashlightHeadgearRequirement = static_cast<FlashlightHeadgearRequirement>(ini.GetLongValue(SECTION_HEADGEAR_RESTRICTION, "iFlashlightHeadgearRequirement", 0));
        headLightKeywordNames = parseCommaList(ini.GetValue(SECTION_HEADGEAR_RESTRICTION, "sHeadLightKeywords", ""));
        headLightAllowList = parseFormPluginList(ini.GetValue(SECTION_HEADGEAR_RESTRICTION, "sHeadLightAllowList", ""));
        headLightDenyList = parseFormPluginList(ini.GetValue(SECTION_HEADGEAR_RESTRICTION, "sHeadLightDenyList", ""));

        weaponFlashlightMeshRequirement = static_cast<FlashlightWeaponMeshRequirement>(ini.GetLongValue(SECTION_WEAPON_MOUNT, "iWeaponFlashlightMeshRequired", 0));
        weaponFlashlightAutoDetectPlugins = parseCommaList(ini.GetValue(SECTION_WEAPON_MOUNT, "sWeaponFlashlightAutoDetectPlugins", ""));
        weaponFlashlightMountBeamToMesh = ini.GetBoolValue(SECTION_WEAPON_MOUNT, "bWeaponFlashlightMountBeamToMesh", true);
        weaponFlashlightAnchorPrimaryHandSphereToMesh = ini.GetBoolValue(SECTION_WEAPON_MOUNT, "bWeaponFlashlightAnchorPrimaryHandSphereToMesh", true);
        weaponFlashlightMeshNodes = parseCommaList(ini.GetValue(SECTION_WEAPON_MOUNT, "sWeaponFlashlightMeshNodes", ""));
        weaponFlashlightMountTransform =
            getTransformValue(ini, SECTION_WEAPON_MOUNT, "tWeaponFlashlightMountTransform", common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 90.0f, 0.0f, -90.0f));

        // Stowed flashlight on the body (grab to turn on into a hand, put back to turn off).
        showFlashlightOnBody = ini.GetBoolValue(SECTION_BODY_STOW, "bShowFlashlightOnBody", true);
        // Placeholder offset; tune in-game via live-reload. Scale matches the in-hand model.
        RE::NiTransform bodyTransformDefault = common::MatrixUtils::getTransform(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bodyTransformDefault.scale = 1.3f;
        flashlightBodyTransform = getTransformValue(ini, SECTION_BODY_STOW, "tFlashlightBodyTransform", bodyTransformDefault);
        flashlightBodyTransformPA = getTransformValue(ini, SECTION_BODY_STOW, "tFlashlightBodyTransformPA", flashlightBodyTransform);

        // Activation-sphere gestures, each grouped in its own INI section (see WandActivationConfig). The
        // suppress flag is a per-binding token in the binding string; the default binding literals set
        // .suppress = true so the shipped gestures still intercept their button even if the INI omits it.
        showAllActivationSpheres = ini.GetBoolValue(DEFAULT_SECTION, "bShowAllActivationSpheres", false);

        RE::NiTransform grabSphereDefault = common::MatrixUtils::getTransform(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        grabSphereDefault.scale = 20.0f;
        bodyActivation = loadWandActivationConfig(ini,
            SECTION_BODY_ACTIVATION_SPHERE,
            f4vr::WandActivationConfig{
                .zone = grabSphereDefault,
                .primary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .secondary = { .hand = vrcf::Hand::Primary, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
            });

        RE::NiTransform headSphereDefault = common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        headSphereDefault.scale = 24.0f;
        headActivation = loadWandActivationConfig(ini,
            SECTION_HEAD_ACTIVATION_SPHERE,
            f4vr::WandActivationConfig{
                .zone = headSphereDefault,
                .primary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .secondary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::LongPress, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
            });

        RE::NiTransform primaryHandSphereDefault = common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        primaryHandSphereDefault.scale = 20.0f;
        primaryHandActivation = loadWandActivationConfig(ini,
            SECTION_PRIMARY_HAND_ACTIVATION_SPHERE,
            f4vr::WandActivationConfig{
                .zone = primaryHandSphereDefault,
                .primary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .secondary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::LongPress, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
            });
        toggleWeaponFlashlightTwoHandedBinding = getInputBindingValue(ini,
            SECTION_WEAPON_MOUNT,
            "sToggleWeaponFlashlightTwoHandedBinding",
            vrcf::InputBinding{ .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger });

        disableVanillaFlashlightToggle = ini.GetBoolValue(DEFAULT_SECTION, "bDisableVanillaFlashlightToggle", false);

        // NPC light detection (NpcDetectionHandler)
        npcDetectionEnabled = ini.GetBoolValue(SECTION_NPC_DETECTION, "bNpcDetectionEnabled", true);
        npcDetectionOnlyWhenSneaking = ini.GetBoolValue(SECTION_NPC_DETECTION, "bNpcDetectionOnlyWhenSneaking", false);
        npcDetectionOnlyHostileNpcs = ini.GetBoolValue(SECTION_NPC_DETECTION, "bNpcDetectionOnlyHostileNpcs", true);
        npcDetectionIntervalMs = static_cast<int>(ini.GetLongValue(SECTION_NPC_DETECTION, "iNpcDetectionIntervalMs", 500));
        npcDetectionDirectEnabled = ini.GetBoolValue(SECTION_NPC_DETECTION, "bNpcDetectionDirectEnabled", true);
        npcDetectionDirectSoundLevel = std::clamp(static_cast<int>(ini.GetLongValue(SECTION_NPC_DETECTION, "iNpcDetectionDirectSoundLevel", 100)), 0, 500);
        npcDetectionLitSpotEnabled = ini.GetBoolValue(SECTION_NPC_DETECTION, "bNpcDetectionLitSpotEnabled", true);
        npcDetectionLitSpotSoundLevel = std::clamp(static_cast<int>(ini.GetLongValue(SECTION_NPC_DETECTION, "iNpcDetectionLitSpotSoundLevel", 40)), 0, 500);
        npcDetectionMaxRange = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionMaxRange", 2000.0));
        npcDetectionFovMult = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionFovMult", 0.75));
        npcDetectionSpottedEventLevel = static_cast<int>(ini.GetLongValue(SECTION_NPC_DETECTION, "iNpcDetectionSpottedEventLevel", 100));
        npcDetectionLosCollisionFilter = parseCollisionFilter(ini.GetValue(SECTION_NPC_DETECTION, "sNpcDetectionLosCollisionFilter", ""), DEFAULT_LOS_COLLISION_FILTER);
        npcDetectionLightLevelEnabled = ini.GetBoolValue(SECTION_NPC_DETECTION, "bNpcDetectionLightLevelEnabled", true);
        npcDetectionLightLevelBaseline = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionLightLevelBaseline", 40.0));
        npcDetectionLightLevelMin = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionLightLevelMin", 70.0));
        npcDetectionLightLevelMax = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionLightLevelMax", 220.0));
        npcDetectionLightLevelCurve = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionLightLevelCurve", 1.0));
        npcDetectionLightLevelDecayMs = static_cast<float>(ini.GetDoubleValue(SECTION_NPC_DETECTION, "fNpcDetectionLightLevelDecayMs", 1000.0));
    }
}
