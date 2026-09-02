#pragma once

#include <utility>
#include <vector>

#include "ConfigBase.h"
#include "Resources.h"
#include "api/FRIKApi.h"
#include "f4vr/WandActivationSphere.h"
#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    static const auto BASE_MOD_PATH = BASE_PATH + "\\" + std::string(Version::PROJECT);
    static const auto INI_PATH = BASE_MOD_PATH + "\\" + std::string(Version::PROJECT) + ".ini";

    /**
     * The possible flashlight locations in-game.
     */
    enum class FlashlightLocation : uint8_t
    {
        OnHead = 0,
        OnPAHead,
        InOffhand,
        InPrimaryHand,
        OnWeapon
    };

    enum class FlashlightConfigLocation : uint8_t
    {
        OnHead = 0,
        InOffhand,
        InPrimaryHand
    };

    /**
     * How the player is holding the flashlight in-hand.
     * Forward: thumb-up grip, controller forward axis ~aligned with light direction.
     * Overhand: fist grip (ice-pick style), controller forward axis ~pointing down/back, light still forward.
     */
    enum class FlashlightGripStyle : uint8_t
    {
        Forward = 0,
        Overhand
    };

    /**
     * Controls grip-style selection. Auto picks based on controller orientation;
     * the locked modes ignore the controller and always use the named style.
     */
    enum class FlashlightGripMode : uint8_t
    {
        Auto = 0,
        ForwardOnly,
        OverhandOnly
    };

    /**
     * Optional restriction on when the flashlight may sit on the (non-power-armor) head.
     * None: no restriction (any state may mount the head).
     * AnyHeadGear: requires any slot-30 headgear worn.
     * Immersive: requires light-capable headgear (keyword rule + allow/deny lists).
     * Power armor is always exempt (the PA helmet lamp applies regardless of this setting).
     */
    enum class FlashlightHeadgearRequirement : uint8_t
    {
        None = 0,
        AnyHeadGear,
        Immersive
    };

    /**
     * Optional restriction requiring a modeled flashlight on the equipped weapon before the light may sit
     * on it (FlashlightLocation::OnWeapon).
     * Disabled: no restriction (the weapon light always applies).
     * Enabled: always require a modeled flashlight mesh on the weapon.
     * AutoDetect: require it only when a supported weapon mod is installed (Config::weaponFlashlightAutoDetectPlugins).
     */
    enum class FlashlightWeaponMeshRequirement : uint8_t
    {
        Disabled = 0,
        Enabled,
        AutoDetect
    };

    class Config : public ConfigBase
    {
    public:
        explicit Config()
            : ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI)
        {}

        void setFlashlightLocation(FlashlightConfigLocation location, bool inPowerArmor);
        void setFlashlightFlagsBitmask(const std::string& bitmask);
        void setShowAllActivationSpheres(bool show);
        void setShowFlashlightOnBody(bool show);
        void setFlashlightHeadgearRequirement(FlashlightHeadgearRequirement requirement);
        void setWeaponFlashlightMeshRequirement(FlashlightWeaponMeshRequirement requirement);
        void setDisableVanillaFlashlightToggle(bool disabled);
        void setNpcDetectionEnabled(bool enabled);
        void saveFlashlightValues(FlashlightLocation location);
        void resetFlashlightValuesToDefault(FlashlightLocation location);

        // Resolve the active in-hand spatial config by hand / grip style / power-armor state.
        const RE::NiTransform& getFlashlightInHandLightTransform(bool isOffhand, FlashlightGripStyle grip) const;
        const RE::NiTransform& getFlashlightMeshTransform(FlashlightGripStyle grip, bool inPowerArmor) const;
        const frik::api::FRIKApi::HandPoseData& getFlashlightHandPose(FlashlightGripStyle grip, bool inPowerArmor) const;
        const RE::NiTransform& getFlashlightBodyTransform(bool inPowerArmor) const;

        // Flashlight location, configured independently for out of / in power armor (iFlashlightLocation / iFlashlightLocationInPA).
        FlashlightConfigLocation flashlightConfigLocation = FlashlightConfigLocation::OnHead;
        FlashlightConfigLocation flashlightConfigLocationInPA = FlashlightConfigLocation::OnHead;

        // flashlight values on head
        float flashlightOnHeadFade = 0.0f;
        int flashlightOnHeadRadius = 0;
        float flashlightOnHeadFov = 0.0f;
        int flashlightOnHeadColorRed = 0;
        int flashlightOnHeadColorGreen = 0;
        int flashlightOnHeadColorBlue = 0;
        std::string flashlightOnHeadGoboPath;
        RE::NiTransform flashlightOnHeadTransform{};

        // flashlight values on power armor head
        float flashlightOnPAHeadFade = 0.0f;
        int flashlightOnPAHeadRadius = 0;
        float flashlightOnPAHeadFov = 0.0f;
        int flashlightOnPAHeadColorRed = 0;
        int flashlightOnPAHeadColorGreen = 0;
        int flashlightOnPAHeadColorBlue = 0;
        std::string flashlightOnPAHeadGoboPath;
        RE::NiTransform flashlightOnPAHeadTransform{};

        // flashlight values in hands
        float flashlightInHandFade = 0.0f;
        int flashlightInHandRadius = 0;
        float flashlightInHandFov = 0.0f;
        int flashlightInHandColorRed = 0;
        int flashlightInHandColorGreen = 0;
        int flashlightInHandColorBlue = 0;
        std::string flashlightInHandGoboPath;
        // Per-hand pose of the flashlight light node relative to the hand wand node, for the Forward grip.
        RE::NiTransform flashlightInOffhandTransform{};
        RE::NiTransform flashlightInPrimaryHandTransform{};
        // Same, for the Overhand (fist) grip.
        RE::NiTransform flashlightInOffhandTransformOverhand{};
        RE::NiTransform flashlightInPrimaryHandTransformOverhand{};

        // flashlight values attached to weapon
        float flashlightOnWeaponFade = 0.0f;
        int flashlightOnWeaponRadius = 0;
        float flashlightOnWeaponFov = 0.0f;
        int flashlightOnWeaponColorRed = 0;
        int flashlightOnWeaponColorGreen = 0;
        int flashlightOnWeaponColorBlue = 0;
        std::string flashlightOnWeaponGoboPath;

        // global to all flashlight locations
        float flashlightNearDistance = 0;
        std::string flashlightFlagsBitmask;
        bool warnAboutFPSStabilizerMod = false;

        // flashlight mesh model in hand
        bool showFlashlightMesh = true;
        // Pose of the mesh for the Forward grip, authored for the right-hand bone. The left-hand bone (the
        // offhand, or the primary hand in left-handed mode) mirrors Z translate and heading at attach time.
        RE::NiTransform flashlightMeshTransform{};
        // Same, for the Overhand (fist) grip.
        RE::NiTransform flashlightMeshTransformOverhand{};
        // Power-armor variants of the mesh transforms; each defaults to the matching non-PA transform.
        RE::NiTransform flashlightMeshTransformPA{};
        RE::NiTransform flashlightMeshTransformOverhandPA{};

        // Hand pose to apply (via FRIK API) while holding the flashlight mesh.
        // Loaded as a 22-float packed list, see ConfigBase::getHandPoseValue.
        frik::api::FRIKApi::HandPoseData flashlightHandPose{};
        // Same, for the Overhand (fist) grip.
        frik::api::FRIKApi::HandPoseData flashlightHandPoseOverhand{};
        // Power-armor variants of the hand poses; each defaults to the matching non-PA pose.
        frik::api::FRIKApi::HandPoseData flashlightHandPosePA{};
        frik::api::FRIKApi::HandPoseData flashlightHandPoseOverhandPA{};

        // Grip-style selection: Auto/ForwardOnly/OverhandOnly. See FlashlightGripMode.
        FlashlightGripMode flashlightGripMode = FlashlightGripMode::Auto;

        // Optional restriction on when the flashlight may sit on the (non-PA) head. See
        // FlashlightHeadgearRequirement. The Immersive rule reads the three lists below. PA is always exempt.
        FlashlightHeadgearRequirement flashlightHeadgearRequirement = FlashlightHeadgearRequirement::None;

        // Immersive-mode rule inputs, parsed from the INI here and resolved to runtime FormIDs once at load
        // by RestrictionHandler::resolveForms(). Keywords are matched by editor ID (BGSKeyword editor
        // IDs are retained at runtime); allow/deny entries are (local FormID, plugin filename) pairs resolved
        // load-order-independently via TESDataHandler::LookupForm. The rule is:
        //   headLight = slot30Occupied && !deny(formID) && ( allow(formID) || hasAnyKeyword(item, keywords) )
        std::vector<std::string> headLightKeywordNames;
        std::vector<std::pair<std::uint32_t, std::string>> headLightAllowList;
        std::vector<std::pair<std::uint32_t, std::string>> headLightDenyList;

        // Optional restriction requiring a modeled flashlight on the equipped weapon before the light may
        // mount on it (FlashlightLocation::OnWeapon). See FlashlightWeaponMeshRequirement. When effectively
        // on (RestrictionHandler::isWeaponFlashlightMeshRequired() — Enabled, or AutoDetect with a supported
        // mod installed), RestrictionHandler detects the mesh node and turns an on-weapon light off when the
        // weapon carries no such mesh.
        FlashlightWeaponMeshRequirement weaponFlashlightMeshRequirement = FlashlightWeaponMeshRequirement::Disabled;
        // Plugins whose presence enables the requirement under AutoDetect. Parsed from a comma-separated INI
        // list of plugin filenames (.esp/.esm); matched against the loaded mods load-order-independently.
        std::vector<std::string> weaponFlashlightAutoDetectPlugins;
        // When on (and the mesh requirement is effectively on — detection only runs then), the weapon-mounted
        // beam is rooted at the detected flashlight mesh node with the mount offset below instead of the
        // generic tuned barrel offset. Has no effect while the requirement is off.
        bool weaponFlashlightMountBeamToMesh = true;
        // When on (and the mesh requirement is effectively on — detection only runs then), the primary-hand
        // activation sphere (checkPrimaryHandActivation) is anchored to the detected flashlight mesh node
        // instead of the primary-hand wand node, so the gesture is reached by bringing the offhand to the
        // gun's lamp. The sphere transform is then read in the mesh node's local space. Falls back to the
        // wand node when no mesh is found (or it isn't a node). Has no effect while the requirement is off.
        bool weaponFlashlightAnchorPrimaryHandSphereToMesh = true;
        // Flashlight mesh node names to search for under the equipped weapon 3D, in priority order (first
        // visible match wins). These are NIF node names, not editor IDs — verify in NifSkope. Parsed from a
        // comma-separated INI list. See the weapon_flashlight_detection reference.
        std::vector<std::string> weaponFlashlightMeshNodes;
        // Light-node rotation + position offset relative to the found flashlight mesh node when rooted
        // there. Tuned per the node's local axes (unknown up front, varies per mod). Not split by PA: the
        // offset is relative to the lamp node, which doesn't change with the gauntlet.
        RE::NiTransform weaponFlashlightMountTransform{};

        // Auto-detect tuning. Angle (degrees) between the controller's top axis and world up: 0 = top
        // pointing straight up (Forward grip), 180 = top pointing straight down (Overhand fist grip).
        // The grip switches to Overhand when the tilt exceeds this threshold; it switches back to
        // Forward when the tilt drops below threshold - hysteresis (a stable deadband).
        float flashlightGripOverhandTiltDegrees = 0.0f;
        float flashlightGripHysteresisDegrees = 0.0f;

        // Stowed flashlight model on the body: shown (beam-less) while the flashlight is off so the
        // player can grab it to turn it on into a hand, or put it back to turn it off.
        bool showFlashlightOnBody = true;

        // Mesh transform of the stowed model relative to the chest bone. Authored for a right-handed
        // player; auto-mirrored when the player is left-handed.
        RE::NiTransform flashlightBodyTransform{};
        RE::NiTransform flashlightBodyTransformPA{};

        // Body grab activation sphere (Flashlight::checkBodyGrab): reach a hand into the chest-stowed model's
        // grab zone and fire its binding to take/return the light. `primary` is the offhand grab, `secondary`
        // the primary-hand grab. `zone` is the grab-sphere transform (offset from the stowed model,
        // auto-mirrored when left-handed); `zonePA` is its optional power-armor variant (selected via
        // WandActivationConfig::zoneFor, falling back to `zone`). The base sphere mesh is unit-diameter, so
        // the zone scale is the grab diameter. Loaded from the [ImFl_BodyActivationSphere] INI section.
        f4vr::WandActivationConfig bodyActivation;

        // Head activation sphere (Flashlight::checkHeadActivation): bring the offhand near the HMD; `primary`
        // (tap) puts the light on the head / toggles it off, `secondary` (long-press) pulls a head-mounted
        // light to the offhand. Zone authored in HMD-local space (not mirrored, no PA variant — the HMD sits
        // the same in/out of power armor). Each binding "none" disables its gesture. Section [ImFl_HeadActivationSphere].
        f4vr::WandActivationConfig headActivation;

        // Primary-hand activation sphere (Flashlight::checkPrimaryHandActivation): bring the offhand near the
        // primary-hand wand; `primary` (tap) moves the light between the offhand and the primary hand / weapon
        // and toggles the on-weapon light, `secondary` (long-press) pulls the on-weapon light to the offhand.
        // Zone authored in primary-hand-local space (not mirrored, no PA variant). Only its scale is used when
        // the sphere is re-anchored to the weapon flashlight mesh (weaponFlashlightAnchorPrimaryHandSphereToMesh).
        // Section [ImFl_PrimaryHandActivationSphere].
        f4vr::WandActivationConfig primaryHandActivation;

        // Force every grab/activation sphere to always render — a global master that overrides each sphere's
        // own WandActivationConfig::showSphere to Always (for tuning or discovering the zones).
        bool showAllActivationSpheres = false;

        // Zone-less offhand toggle of the weapon-mounted light for two-handed weapon holds, where the offhand
        // grips the foregrip and can't reach the primary-hand activation sphere (checkWeaponFlashlightToggle).
        // Active only while the offhand is gripping the weapon (FRIK) and the light is / would be on the
        // weapon; a press toggles it on/off from anywhere. "none" disables it.
        vrcf::InputBinding toggleWeaponFlashlightTwoHandedBinding;

        // Disable the vanilla game's global flashlight toggle so only this mod's gestures control the light.
        bool disableVanillaFlashlightToggle = false;

        // NPC light detection: on a throttled tick while the light is on, NpcDetectionHandler posts a
        // player-owned detection event at what the beam touches — near the nearest lit NPC (with line of
        // sight), or on the beam's lit spot on world geometry — so NPCs notice the beam directionally.
        // See docs/tech/npc-light-detection.md.
        bool npcDetectionEnabled = true;
        // Restrict the whole feature to while the player is sneaking.
        bool npcDetectionOnlyWhenSneaking = false;
        // Only NPCs hostile to the player react to the beam (engine faction/relationship hostility), so the
        // per-tick line-of-sight raycast budget is spent on actual threats instead of settlers and townsfolk.
        // Off = everyone reacts (a guard notices your beam too), at the cost of crowds eating that budget.
        bool npcDetectionOnlyHostileNpcs = true;
        // Detection tick interval (ms). The engine's own detection AI updates at a similar cadence.
        int npcDetectionIntervalMs = 500;
        // Enable the direct-on-NPC detection path (event near the lit NPC). Lets it be toggled off for
        // debugging without losing the tuned sound level below.
        bool npcDetectionDirectEnabled = true;
        // Base sound level of the direct-hit event (the Papyrus-mod-equivalent 0-500 scale; 0 disables
        // the direct path). Scaled down with beam distance.
        int npcDetectionDirectSoundLevel = 100;
        // Enable the lit-spot detection path (event where the beam paints world geometry). Independently
        // toggleable from the direct path for debugging.
        bool npcDetectionLitSpotEnabled = true;
        // Base sound level of the lit-spot event when the beam touches nobody (0 disables the lit-spot path).
        int npcDetectionLitSpotSoundLevel = 40;
        // Detection reach cap (game units), independent of the visual beam radius (which reaches ~7000).
        float npcDetectionMaxRange = 2000.0f;
        // Fraction of the visual beam FOV used as the detection cone — the dim outer edge shouldn't alert.
        float npcDetectionFovMult = 0.75f;
        // Instant-spot escalation: a hostile NPC lit this hard while facing the player has *seen* the player,
        // so the detection event is placed on the player's own position instead of offset near the NPC — it
        // comes straight at you rather than investigating a spot nearby. Because the level scales with beam
        // strength at the target, this is effectively a "bright and close" threshold; set it above the highest
        // reachable level (iNpcDetectionDirectSoundLevel * 1.5) to never escalate.
        int npcDetectionSpottedEventLevel = 100;
        // The game's own item-picker collision filter: what other VR physics mods use to clip a ray against
        // world geometry, and unlike the player's own collision filter it doesn't collide with the invisible
        // actor-zone volumes that exist to catch character controllers.
        static constexpr std::uint32_t DEFAULT_LOS_COLLISION_FILTER = 0x02420028;

        // Havok collision-filter word for the line-of-sight / lit-spot raycast (low 7 bits = COL_LAYER).
        std::uint32_t npcDetectionLosCollisionFilter = DEFAULT_LOS_COLLISION_FILTER;

        // Feed the beam into the engine's own VISUAL detection by raising the player's cached light level
        // while the beam is on an NPC — the quantity a spot light fails to raise in vanilla. See docs 3.3.
        bool npcDetectionLightLevelEnabled = true;
        // Applied whenever the light is on — beam on someone or not, sneaking or not (carrying a light source
        // gives you away by itself). The ramp below rides on top; keep this under Min or weak hits vanish.
        float npcDetectionLightLevelBaseline = 40.0f;
        // Applied at the weakest qualifying hit and at point blank, interpolated by beam strength. Engine
        // scale: ~20-40 dim, ~150 street light, ~400 bright indoors. It is the PLAYER's light, all NPCs see it.
        float npcDetectionLightLevelMin = 70.0f;
        float npcDetectionLightLevelMax = 220.0f;
        // Shapes the ramp: 1 is linear, higher needs a closer, brighter hit to reach the top. Beam strength
        // falls off slowly over the beam's own radius (7000 units in hand), so raising this pulls the bright
        // end in toward the player rather than spreading it across the whole range.
        float npcDetectionLightLevelCurve = 1.0f;
        // How long the level fades out over once the beam leaves, after a one-tick hold. The hold is what
        // stops the gap between throttled ticks from becoming a visible sawtooth.
        float npcDetectionLightLevelDecayMs = 1000.0f;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
