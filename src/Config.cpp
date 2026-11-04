#include "Config.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <sstream>

#include "Utils.h"
#include "common/MatrixUtils.h"
#include "render/PrimitiveDraw.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();

    // Per-sphere config sections for the activation-sphere gestures (WandActivationConfig).
    constexpr const char* SECTION_BODY_ACTIVATION_SPHERE = "ImFl_BodyActivationSphere";
    constexpr const char* SECTION_HEAD_ACTIVATION_SPHERE = "ImFl_HeadActivationSphere";
    constexpr const char* SECTION_PRIMARY_HAND_ACTIVATION_SPHERE = "ImFl_PrimaryHandActivationSphere";

    // The activation-sphere icons, all in Data\Textures\ImmersiveFlashlightVR\ (the framework's own icons too, so
    // the config menu lists them in one folder). The head sphere names the hand icon, as the framework's default
    // is under f4cf\, where the mod has none.
    constexpr auto FLASHLIGHT_ICON = "activation-icon-flashlight.dds";
    constexpr auto HAND_ICON = "activation-icon-hand.dds";

    // Named config sections for the advanced, per-feature settings. Location, beam appearance and the global
    // light/toggle keys are in the main section (DEFAULT_SECTION); only the advanced groups are read from these.
    constexpr const char* SECTION_BEAM_GLOW = "ImFl_BeamGlow";
    constexpr const char* SECTION_HAND_MOUNT = "ImFl_HandMount";
    constexpr const char* SECTION_HEADGEAR_RESTRICTION = "ImFl_HeadgearRestriction";
    constexpr const char* SECTION_WEAPON_MOUNT = "ImFl_WeaponMount";
    constexpr const char* SECTION_BODY_STOW = "ImFl_BodyStow";
    constexpr const char* SECTION_NPC_DETECTION = "ImFl_NpcDetection";

    /**
     * Split a list as the INI had it, comma-separated text, into trimmed, non-empty items.
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
     * Parse "localFormID|plugin" entries into (local FormID, plugin) pairs.
     * The FormID is the local id as shown in xEdit (hex, with or without "0x", load-order prefix dropped).
     * Malformed entries (missing '|' or unparsable id) are skipped with a warning.
     */
    std::vector<std::pair<std::uint32_t, std::string>> parseFormPluginList(const std::vector<std::string>& entries)
    {
        std::vector<std::pair<std::uint32_t, std::string>> out;
        for (const auto& entry : entries) {
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

    /**
     * A beam color as the TOML text of its table, { rgb = [r, g, b] } in 0..255, as Mods Config FO4 VR takes a color.
     */
    std::string beamColorText(const RE::ColorStruct& color)
    {
        return std::format("{{ rgb = [{}, {}, {}] }}", +color.red, +color.green, +color.blue);
    }

    /**
     * A color read from the config, 0..1 per channel, as the 0..255 bytes the light form has; the alpha is left 0.
     */
    RE::ColorStruct toColorStruct(const render::Color& color)
    {
        const auto toByte = [](const float channel) { return static_cast<std::uint8_t>(std::clamp(std::lround(channel * 255.0f), 0l, 255l)); };
        return { .red = toByte(color.r), .green = toByte(color.g), .blue = toByte(color.b), .alpha = 0 };
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
            saveConfigValue(DEFAULT_SECTION, "iFlashlightLocationInPA", static_cast<int>(location));
        } else {
            flashlightConfigLocation = location;
            saveConfigValue(DEFAULT_SECTION, "iFlashlightLocation", static_cast<int>(location));
        }
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

    /**
     * A beam value of the location as a setting of Mods Config FO4 VR's config spec: its id, the key and its section as
     * the spec has a setting with no id of its own, and its current value in the setting's valueType (a color is the
     * TOML text of its table). The in-world beam tuning hands its edits to the menu with it.
     */
    std::pair<std::string, mods_config::api::Value> Config::getFlashlightValueSetting(const FlashlightLocation location, const FlashlightBeamValue value) const
    {
        struct BeamValues
        {
            std::string_view name;
            float fade;
            int radius;
            float fov;
            const RE::ColorStruct& color;
            const std::string& goboPath;
        };

        const auto values = [this, location]() -> BeamValues {
            switch (location) {
            case FlashlightLocation::OnHead:
                return { "OnHead", flashlightOnHeadFade, flashlightOnHeadRadius, flashlightOnHeadFov, flashlightOnHeadColor, flashlightOnHeadGoboPath };
            case FlashlightLocation::OnPAHead:
                return { "OnPAHead", flashlightOnPAHeadFade, flashlightOnPAHeadRadius, flashlightOnPAHeadFov, flashlightOnPAHeadColor, flashlightOnPAHeadGoboPath };
            case FlashlightLocation::InOffhand:
            case FlashlightLocation::InPrimaryHand:
                return { "InHand", flashlightInHandFade, flashlightInHandRadius, flashlightInHandFov, flashlightInHandColor, flashlightInHandGoboPath };
            case FlashlightLocation::OnWeapon:
                return { "OnWeapon", flashlightOnWeaponFade, flashlightOnWeaponRadius, flashlightOnWeaponFov, flashlightOnWeaponColor, flashlightOnWeaponGoboPath };
            }
            std::unreachable();
        }();

        const auto idOf = [&values](const std::string_view typePrefix, const std::string_view name) {
            return std::format("{}Flashlight{}{}:{}", typePrefix, values.name, name, DEFAULT_SECTION);
        };
        switch (value) {
        case FlashlightBeamValue::Fade:
            return { idOf("f", "Fade"), values.fade };
        case FlashlightBeamValue::Radius:
            return { idOf("i", "Radius"), values.radius };
        case FlashlightBeamValue::Fov:
            return { idOf("f", "Fov"), values.fov };
        case FlashlightBeamValue::Color:
            return { idOf("c", "Color"), beamColorText(values.color) };
        case FlashlightBeamValue::GoboPath:
            return { idOf("s", "GoboPath"), values.goboPath };
        }
        std::unreachable();
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
            flashlightOnHeadColor = defaultConfig.flashlightOnHeadColor;
            flashlightOnHeadGoboPath = defaultConfig.flashlightOnHeadGoboPath;
            break;
        case FlashlightLocation::OnPAHead:
            flashlightOnPAHeadFade = defaultConfig.flashlightOnPAHeadFade;
            flashlightOnPAHeadRadius = defaultConfig.flashlightOnPAHeadRadius;
            flashlightOnPAHeadFov = defaultConfig.flashlightOnPAHeadFov;
            flashlightOnPAHeadColor = defaultConfig.flashlightOnPAHeadColor;
            flashlightOnPAHeadGoboPath = defaultConfig.flashlightOnPAHeadGoboPath;
            break;
        case FlashlightLocation::InOffhand:
        case FlashlightLocation::InPrimaryHand:
            flashlightInHandFade = defaultConfig.flashlightInHandFade;
            flashlightInHandRadius = defaultConfig.flashlightInHandRadius;
            flashlightInHandFov = defaultConfig.flashlightInHandFov;
            flashlightInHandColor = defaultConfig.flashlightInHandColor;
            flashlightInHandGoboPath = defaultConfig.flashlightInHandGoboPath;
            break;
        case FlashlightLocation::OnWeapon:
            flashlightOnWeaponFade = defaultConfig.flashlightOnWeaponFade;
            flashlightOnWeaponRadius = defaultConfig.flashlightOnWeaponRadius;
            flashlightOnWeaponFov = defaultConfig.flashlightOnWeaponFov;
            flashlightOnWeaponColor = defaultConfig.flashlightOnWeaponColor;
            flashlightOnWeaponGoboPath = defaultConfig.flashlightOnWeaponGoboPath;
            break;
        }
    }

    void Config::loadConfigInternal(const toml::table& config)
    {
        // Flashlight location, separate for out of / in power armor. In-PA defaults to head (PA helmet lamp).
        flashlightConfigLocation = static_cast<FlashlightConfigLocation>(config[DEFAULT_SECTION]["iFlashlightLocation"].value_or(0));
        flashlightConfigLocationInPA = static_cast<FlashlightConfigLocation>(config[DEFAULT_SECTION]["iFlashlightLocationInPA"].value_or(0));

        // A beam color, { rgb = [r, g, b] } in 0..255, as the light form has it.
        const auto readBeamColor = [&config](const char* key, const RE::ColorStruct& defaultValue) {
            return toColorStruct(getColorValue(config, DEFAULT_SECTION, key, render::Color::rgba(defaultValue.red, defaultValue.green, defaultValue.blue)));
        };

        // Head-mounted flashlight defaults
        flashlightOnHeadFade = config[DEFAULT_SECTION]["fFlashlightOnHeadFade"].value_or(1.1f);
        flashlightOnHeadRadius = config[DEFAULT_SECTION]["iFlashlightOnHeadRadius"].value_or(2400);
        flashlightOnHeadFov = config[DEFAULT_SECTION]["fFlashlightOnHeadFov"].value_or(100.0f);
        flashlightOnHeadColor = readBeamColor("cFlashlightOnHeadColor", { 235, 224, 190 });
        flashlightOnHeadGoboPath = config[DEFAULT_SECTION]["sFlashlightOnHeadGoboPath"].value_or(std::string(R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)"));
        flashlightOnHeadTransform =
            getTransformValue(config, DEFAULT_SECTION, "tFlashlightOnHeadTransform", common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 0.0f, -5.0f, 0.0f));

        // Power armor head-mounted flashlight defaults
        flashlightOnPAHeadFade = config[DEFAULT_SECTION]["fFlashlightOnPAHeadFade"].value_or(flashlightOnHeadFade);
        flashlightOnPAHeadRadius = config[DEFAULT_SECTION]["iFlashlightOnPAHeadRadius"].value_or(flashlightOnHeadRadius);
        flashlightOnPAHeadFov = config[DEFAULT_SECTION]["fFlashlightOnPAHeadFov"].value_or(flashlightOnHeadFov);
        flashlightOnPAHeadColor = readBeamColor("cFlashlightOnPAHeadColor", flashlightOnHeadColor);
        flashlightOnPAHeadGoboPath = config[DEFAULT_SECTION]["sFlashlightOnPAHeadGoboPath"].value_or(flashlightOnHeadGoboPath);
        flashlightOnPAHeadTransform = getTransformValue(config, DEFAULT_SECTION, "tFlashlightOnPAHeadTransform", flashlightOnHeadTransform);

        // In hand flashlight defaults
        flashlightInHandFade = config[DEFAULT_SECTION]["fFlashlightInHandFade"].value_or(1.3f);
        flashlightInHandRadius = config[DEFAULT_SECTION]["iFlashlightInHandRadius"].value_or(6000);
        flashlightInHandFov = config[DEFAULT_SECTION]["fFlashlightInHandFov"].value_or(65.0f);
        flashlightInHandColor = readBeamColor("cFlashlightInHandColor", { 240, 230, 225 });
        flashlightInHandGoboPath = config[DEFAULT_SECTION]["sFlashlightInHandGoboPath"].value_or(std::string(R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)"));
        flashlightInOffhandTransform =
            getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightInOffhandTransform", common::MatrixUtils::getTransform(5.0f, -2.0f, -2.0f, 0.0f, -30.0f, -85.0f));
        flashlightInPrimaryHandTransform =
            getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightInPrimaryHandTransform", common::MatrixUtils::getTransform(5.0f, 2.0f, -2.0f, 0.0f, -30.0f, -95.0f));
        flashlightInOffhandTransformOverhand =
            getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightInOffhandTransformOverhand", common::MatrixUtils::getTransform(5.0f, -2.0f, -2.0f, 0.0f, 60.0f, -85.0f));
        flashlightInPrimaryHandTransformOverhand =
            getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightInPrimaryHandTransformOverhand", common::MatrixUtils::getTransform(5.0f, 2.0f, -2.0f, 0.0f, 60.0f, -95.0f));

        // Attached to weapon flashlight defaults
        flashlightOnWeaponFade = config[DEFAULT_SECTION]["fFlashlightOnWeaponFade"].value_or(1.3f);
        flashlightOnWeaponRadius = config[DEFAULT_SECTION]["iFlashlightOnWeaponRadius"].value_or(5000);
        flashlightOnWeaponFov = config[DEFAULT_SECTION]["fFlashlightOnWeaponFov"].value_or(55.0f);
        flashlightOnWeaponColor = readBeamColor("cFlashlightOnWeaponColor", { 240, 230, 225 });
        flashlightOnWeaponGoboPath = config[DEFAULT_SECTION]["sFlashlightOnWeaponGoboPath"].value_or(std::string(R"(data\Textures\Effects\Gobos\FlashlightGobo01.dds)"));

        // global to all flashlight locations
        flashlightNearDistance = config[DEFAULT_SECTION]["fFlashlightNearDistance"].value_or(30.0f);
        flashlightFlagsBitmask = config[DEFAULT_SECTION]["sFlashlightFlagsBitmask"].value_or(std::string("0000010000100001"));
        warnAboutFPSStabilizerMod = config[DEFAULT_SECTION]["bWarnAboutFPSStabilizerMod"].value_or(true);

        // visible beam glow at the hand-held model's lens and the weapon lamp
        beamGlowBaseIntensity = fmaxf(0.0f, config[SECTION_BEAM_GLOW]["fBeamGlowBaseIntensity"].value_or(0.2f));
        beamGlowIntensity = fmaxf(0.0f, config[SECTION_BEAM_GLOW]["fBeamGlowIntensity"].value_or(0.15f));
        beamGlowFovOffset = config[SECTION_BEAM_GLOW]["fBeamGlowFovOffset"].value_or(5.0f);

        // flashlight mesh model in hand
        showFlashlightMesh = config[SECTION_HAND_MOUNT]["bShowFlashlightMesh"].value_or(true);
        flashlightMeshTransform =
            getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightMeshTransform", common::MatrixUtils::getTransform(-2.0f, 3.0f, 3.0f, 25.0f, 0.0f, 90.0f));
        flashlightMeshTransformOverhand =
            getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightMeshTransformOverhand", common::MatrixUtils::getTransform(-2.0f, 3.0f, 3.0f, 25.0f, 90.0f, 90.0f));
        // Power-armor variants of the mesh transforms; default to the matching non-PA transform.
        flashlightMeshTransformPA = getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightMeshTransformPA", flashlightMeshTransform);
        flashlightMeshTransformOverhandPA = getTransformValue(config, SECTION_HAND_MOUNT, "tFlashlightMeshTransformOverhandPA", flashlightMeshTransformOverhand);

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
        flashlightHandPose = frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(config, SECTION_HAND_MOUNT, "hFlashlightHandPose", DEFAULT_HAND_POSE));

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
            frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(config, SECTION_HAND_MOUNT, "hFlashlightHandPoseOverhand", DEFAULT_HAND_POSE_OVERHAND));

        flashlightHandPosePA = frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(config, SECTION_HAND_MOUNT, "hFlashlightHandPosePA", flashlightHandPose.toFloats()));
        flashlightHandPoseOverhandPA =
            frik::api::FRIKApi::HandPoseData::fromFloats(getHandPoseValue(config, SECTION_HAND_MOUNT, "hFlashlightHandPoseOverhandPA", flashlightHandPoseOverhand.toFloats()));

        flashlightGripMode = static_cast<FlashlightGripMode>(config[SECTION_HAND_MOUNT]["iFlashlightGripMode"].value_or(0));
        flashlightGripOverhandTiltDegrees = config[SECTION_HAND_MOUNT]["fFlashlightGripOverhandTiltDegrees"].value_or(120.0f);
        flashlightGripHysteresisDegrees = config[SECTION_HAND_MOUNT]["fFlashlightGripHysteresisDegrees"].value_or(30.0f);

        flashlightHeadgearRequirement = static_cast<FlashlightHeadgearRequirement>(config[SECTION_HEADGEAR_RESTRICTION]["iFlashlightHeadgearRequirement"].value_or(0));
        headLightKeywordNames = getStringListValue(config, SECTION_HEADGEAR_RESTRICTION, "aHeadLightKeywords", {});
        headLightAllowList = parseFormPluginList(getStringListValue(config, SECTION_HEADGEAR_RESTRICTION, "aHeadLightAllowList", {}));
        headLightDenyList = parseFormPluginList(getStringListValue(config, SECTION_HEADGEAR_RESTRICTION, "aHeadLightDenyList", {}));

        weaponFlashlightMeshRequirement = static_cast<FlashlightWeaponMeshRequirement>(config[SECTION_WEAPON_MOUNT]["iWeaponFlashlightMeshRequired"].value_or(0));
        weaponFlashlightAutoDetectPlugins = getStringListValue(config, SECTION_WEAPON_MOUNT, "aWeaponFlashlightAutoDetectPlugins", {});
        weaponFlashlightMountBeamToMesh = config[SECTION_WEAPON_MOUNT]["bWeaponFlashlightMountBeamToMesh"].value_or(true);
        weaponFlashlightAnchorPrimaryHandSphereToMesh = config[SECTION_WEAPON_MOUNT]["bWeaponFlashlightAnchorPrimaryHandSphereToMesh"].value_or(true);
        weaponFlashlightMeshNodes = getStringListValue(config, SECTION_WEAPON_MOUNT, "aWeaponFlashlightMeshNodes", {});
        weaponFlashlightMountTransform =
            getTransformValue(config, SECTION_WEAPON_MOUNT, "tWeaponFlashlightMountTransform", common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 90.0f, 0.0f, -90.0f));

        // Stowed flashlight on the body (grab to turn on into a hand, put back to turn off).
        showFlashlightOnBody = config[SECTION_BODY_STOW]["bShowFlashlightOnBody"].value_or(true);
        // Placeholder offset; tune in-game via live-reload. Scale matches the in-hand model.
        RE::NiTransform bodyTransformDefault = common::MatrixUtils::getTransform(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bodyTransformDefault.scale = 1.3f;
        flashlightBodyTransform = getTransformValue(config, SECTION_BODY_STOW, "tFlashlightBodyTransform", bodyTransformDefault);
        flashlightBodyTransformPA = getTransformValue(config, SECTION_BODY_STOW, "tFlashlightBodyTransformPA", flashlightBodyTransform);

        // Activation-sphere gestures, each grouped in its own INI section (see WandActivationConfig). The
        // suppress flag is a per-binding token in the binding string; the default binding literals set
        // .suppress = true so the shipped gestures still intercept their button even if the INI omits it.
        showAllActivationSpheres = config[DEFAULT_SECTION]["bShowAllActivationSpheres"].value_or(false);

        RE::NiTransform grabSphereDefault = common::MatrixUtils::getTransform(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        grabSphereDefault.scale = 20.0f;
        bodyActivation = loadWandActivationConfig(config,
            SECTION_BODY_ACTIVATION_SPHERE,
            f4vr::WandActivationConfig{
                .zone = grabSphereDefault,
                .primary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .secondary = { .hand = vrcf::Hand::Primary, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .iconStyle = { .texture = FLASHLIGHT_ICON },
            });

        RE::NiTransform headSphereDefault = common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        headSphereDefault.scale = 24.0f;
        headActivation = loadWandActivationConfig(config,
            SECTION_HEAD_ACTIVATION_SPHERE,
            f4vr::WandActivationConfig{
                .zone = headSphereDefault,
                .primary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .secondary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::LongPress, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .showIcon = f4vr::ActivationSphereVisibility::Never,
                .iconStyle = { .texture = HAND_ICON },
            });

        RE::NiTransform primaryHandSphereDefault = common::MatrixUtils::getTransform(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        primaryHandSphereDefault.scale = 20.0f;
        primaryHandActivation = loadWandActivationConfig(config,
            SECTION_PRIMARY_HAND_ACTIVATION_SPHERE,
            f4vr::WandActivationConfig{
                .zone = primaryHandSphereDefault,
                .primary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .secondary = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::LongPress, .button = vr::k_EButton_SteamVR_Trigger, .suppress = true },
                .iconStyle = { .texture = FLASHLIGHT_ICON },
            });
        toggleWeaponFlashlightTwoHandedBinding = getInputBindingValue(config,
            SECTION_WEAPON_MOUNT,
            "kToggleWeaponFlashlightTwoHandedBinding",
            vrcf::InputBinding{ .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger });

        disableVanillaFlashlightToggle = config[DEFAULT_SECTION]["bDisableVanillaFlashlightToggle"].value_or(false);
        keepFlashlightOnInPipboy = config[DEFAULT_SECTION]["bKeepFlashlightOnInPipboy"].value_or(true);

        // NPC light detection (NpcDetectionHandler)
        npcDetectionEnabled = config[SECTION_NPC_DETECTION]["bNpcDetectionEnabled"].value_or(true);
        npcDetectionOnlyWhenSneaking = config[SECTION_NPC_DETECTION]["bNpcDetectionOnlyWhenSneaking"].value_or(false);
        npcDetectionOnlyHostileNpcs = config[SECTION_NPC_DETECTION]["bNpcDetectionOnlyHostileNpcs"].value_or(true);
        npcDetectionIntervalMs = config[SECTION_NPC_DETECTION]["iNpcDetectionIntervalMs"].value_or(500);
        npcDetectionDirectEnabled = config[SECTION_NPC_DETECTION]["bNpcDetectionDirectEnabled"].value_or(true);
        npcDetectionDirectSoundLevel = std::clamp(config[SECTION_NPC_DETECTION]["iNpcDetectionDirectSoundLevel"].value_or(100), 0, 500);
        npcDetectionLitSpotEnabled = config[SECTION_NPC_DETECTION]["bNpcDetectionLitSpotEnabled"].value_or(true);
        npcDetectionLitSpotSoundLevel = std::clamp(config[SECTION_NPC_DETECTION]["iNpcDetectionLitSpotSoundLevel"].value_or(40), 0, 500);
        npcDetectionMaxRange = config[SECTION_NPC_DETECTION]["fNpcDetectionMaxRange"].value_or(2000.0f);
        npcDetectionFovMult = config[SECTION_NPC_DETECTION]["fNpcDetectionFovMult"].value_or(0.75f);
        npcDetectionSpottedEventLevel = config[SECTION_NPC_DETECTION]["iNpcDetectionSpottedEventLevel"].value_or(100);
        npcDetectionLosCollisionFilter =
            parseCollisionFilter(config[SECTION_NPC_DETECTION]["sNpcDetectionLosCollisionFilter"].value_or(std::string()), DEFAULT_LOS_COLLISION_FILTER);
        npcDetectionLightLevelEnabled = config[SECTION_NPC_DETECTION]["bNpcDetectionLightLevelEnabled"].value_or(true);
        npcDetectionLightLevelBaseline = config[SECTION_NPC_DETECTION]["fNpcDetectionLightLevelBaseline"].value_or(40.0f);
        npcDetectionLightLevelMin = config[SECTION_NPC_DETECTION]["fNpcDetectionLightLevelMin"].value_or(70.0f);
        npcDetectionLightLevelMax = config[SECTION_NPC_DETECTION]["fNpcDetectionLightLevelMax"].value_or(220.0f);
        npcDetectionLightLevelCurve = config[SECTION_NPC_DETECTION]["fNpcDetectionLightLevelCurve"].value_or(1.0f);
        npcDetectionLightLevelDecayMs = config[SECTION_NPC_DETECTION]["fNpcDetectionLightLevelDecayMs"].value_or(1000.0f);

        _valuesReloaded = true;
    }

    /**
     * Move what the generic migration can't carry: a key that changed its name or type is dropped by it, and its
     * default applies unless it is moved here. Runs after the generic migration, on the default of the latest version
     * with the player's values set in it.
     * - Version 1 (from the INI, imported as version 0): each beam color is one table key,
     *   cFlashlight<location>Color = { rgb = [r, g, b] }, in place of the INI's three integer keys
     *   iFlashlight<location>ColorRed / Green / Blue; and each list is an array (aHeadLightKeywords, ...) in place of
     *   the INI's comma-separated text (sHeadLightKeywords, ...).
     */
    void Config::updateConfigToLatestVersionCustom(const int currentVersion, int, const toml::table& old, common::TomlEditor& latest) const
    {
        if (currentVersion < 1) {
            migrateBeamColorsToTables(old, latest);
            migrateCommaListsToArrays(old, latest);
        }
    }

    /**
     * Move each list from the INI's comma-separated text into its array. The INI import matches the INI's key
     * (sHeadLightKeywords) to the array's (aHeadLightKeywords) by its other type prefix, but can't read the text as an
     * array, so it writes the text under the array's name, which the generic migration leaves, as its type differs.
     */
    void Config::migrateCommaListsToArrays(const toml::table& old, common::TomlEditor& latest)
    {
        constexpr std::pair<const char*, const char*> LISTS[] = {
            { SECTION_HEADGEAR_RESTRICTION, "aHeadLightKeywords" },
            { SECTION_HEADGEAR_RESTRICTION, "aHeadLightAllowList" },
            { SECTION_HEADGEAR_RESTRICTION, "aHeadLightDenyList" },
            { SECTION_WEAPON_MOUNT, "aWeaponFlashlightAutoDetectPlugins" },
            { SECTION_WEAPON_MOUNT, "aWeaponFlashlightMeshNodes" },
        };
        for (const auto& [section, key] : LISTS) {
            const auto text = old[section][key].value<std::string>();
            if (!text) {
                continue;
            }

            toml::array items;
            for (const auto& item : parseCommaList(*text)) {
                items.push_back(item);
            }
            if (latest.set({ section, key }, items)) {
                logger::info("Migrated {}.{} into an array", section, key);
            } else {
                logger::warn("Failed to migrate {}.{} into an array: {}", section, key, latest.error());
            }
        }
    }

    /**
     * Move each beam color from the INI's three keys (iFlashlight<location>ColorRed / Green / Blue) into its table key
     * (cFlashlight<location>Color = { rgb = [r, g, b] }). The INI import writes those keys as text, as the default
     * doesn't have them. A channel the INI doesn't have keeps the default's, and a location with none keeps the
     * default color.
     */
    void Config::migrateBeamColorsToTables(const toml::table& old, common::TomlEditor& latest)
    {
        const auto readChannel = [&old](const std::string& key) -> std::optional<int> {
            const auto text = old[DEFAULT_SECTION][key].value<std::string>();
            int value = 0;
            if (!text || std::from_chars(text->data(), text->data() + text->size(), value).ec != std::errc{}) {
                return std::nullopt;
            }
            return std::clamp(value, 0, 255);
        };

        for (const auto location : { "OnHead", "OnPAHead", "InHand", "OnWeapon" }) {
            const auto red = readChannel(std::format("iFlashlight{}ColorRed", location));
            const auto green = readChannel(std::format("iFlashlight{}ColorGreen", location));
            const auto blue = readChannel(std::format("iFlashlight{}ColorBlue", location));
            if (!red && !green && !blue) {
                continue;
            }

            const auto key = std::format("cFlashlight{}Color", location);
            const auto defaultColor = toColorStruct(getColorValue(latest.table(), DEFAULT_SECTION, key.c_str(), {}));
            const toml::table color{ { "rgb", toml::array{ red.value_or(defaultColor.red), green.value_or(defaultColor.green), blue.value_or(defaultColor.blue) } } };
            if (latest.set({ DEFAULT_SECTION, key }, color)) {
                logger::info("Migrated the {} beam color into {}", location, key);
            } else {
                logger::warn("Failed to migrate the {} beam color into {}: {}", location, key, latest.error());
            }
        }
    }
}
