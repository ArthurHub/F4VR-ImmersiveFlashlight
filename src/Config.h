#pragma once

#include <utility>
#include <vector>

#include "ConfigBase.h"
#include "Resources.h"
#include "api/FRIKApi.h"
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

    class Config : public ConfigBase
    {
    public:
        explicit Config()
            : ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI)
        {}

        void setFlashlightLocation(FlashlightConfigLocation location, bool inPowerArmor);
        void setFlashlightFlagsBitmask(const std::string& bitmask);
        void saveFlashlightValues(FlashlightLocation location);
        void resetFlashlightValuesToDefault(FlashlightLocation location);

        // Resolve the active in-hand spatial config by hand / grip style / power-armor state.
        const RE::NiTransform& getFlashlightInHandLightTransform(bool isOffhand, FlashlightGripStyle grip, bool inPowerArmor) const;
        const RE::NiTransform& getFlashlightMeshTransform(FlashlightGripStyle grip, bool inPowerArmor) const;
        const frik::api::FRIKApi::HandPoseData& getFlashlightHandPose(FlashlightGripStyle grip, bool inPowerArmor) const;
        const RE::NiTransform& getFlashlightBodyTransform(bool inPowerArmor) const;
        const RE::NiTransform& getFlashlightGrabSphereTransform(bool inPowerArmor) const;

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
        // Power-armor variants of the above. PA gauntlets are larger and posed differently than bare hands,
        // so the light needs its own offsets. Each defaults to the matching non-PA transform.
        RE::NiTransform flashlightInOffhandTransformPA{};
        RE::NiTransform flashlightInPrimaryHandTransformPA{};
        RE::NiTransform flashlightInOffhandTransformOverhandPA{};
        RE::NiTransform flashlightInPrimaryHandTransformOverhandPA{};

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
        // Primary-hand pose of the mesh for the Forward grip. Offhand mirrors Z translate and heading at attach time.
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
        // mount on it (FlashlightLocation::OnWeapon). When on, RestrictionHandler roots the beam at the
        // detected mesh node and turns an on-weapon light off when the weapon carries no such mesh.
        bool weaponFlashlightRequired = false;
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

        // Transform of the spherical grab zone, in the same body-bone space as the stowed model (mirrored
        // the same way, PA variant defaults to non-PA). The base sphere mesh is 1 unit, so the transform's
        // scale is the grab radius. Defaults to the stowed-model transform when omitted.
        RE::NiTransform flashlightGrabSphereTransform{};
        RE::NiTransform flashlightGrabSphereTransformPA{};

        // Head activation: bring the offhand near the HMD and fire the binding to put the flashlight on the
        // head (on from off, switch to head from a hand, or off when already head-mounted). Offhand only;
        // always active (set the binding to "none" to disable it).
        // Activation sphere around the HMD, authored in HMD-local space (not mirrored — centered on the
        // head). The base sphere mesh is unit-diameter, so the transform scale is the grab diameter. No PA
        // variant: the zone is measured off the HMD, which sits in the same place in and out of power armor.
        RE::NiTransform flashlightHeadSphereTransform{};

        // Activation sphere around the primary-hand wand node, authored in primary-hand-local space (not
        // mirrored — anchored to the hand, so one value reads for both handedness; no PA variant). The base
        // sphere mesh is unit-diameter, so the transform scale is the activation diameter.
        RE::NiTransform flashlightPrimaryHandSphereTransform{};

        // Render the grab/activation spheres (framework debug sphere mesh) at their exact size/location for tuning.
        bool debugShowGrabSphere = false;

        // Grab/return input bindings, one per hand. Set a hand to "none" to disable grabbing with it.
        vrcf::InputBinding grabFlashlightByOffhandBinding;
        vrcf::InputBinding grabFlashlightByPrimaryHandBinding;
        // Offhand-near-HMD head activation sphere (checkHeadActivation): the tap binding puts the light on the
        // head / toggles it off; the long-press binding pulls the head-mounted light to the offhand. Each
        // "none" disables its gesture.
        vrcf::InputBinding activateFlashlightOnHeadBinding;
        vrcf::InputBinding switchFlashlightFromHeadToOffhandBinding;

        // Offhand-near-primary-hand activation sphere (checkPrimaryHandActivation): with the light on, moves
        // it between the offhand and the primary hand / weapon and toggles the on-weapon light on/off; the
        // long-press binding pulls the on-weapon light back to the offhand. Each "none" disables its gesture.
        vrcf::InputBinding activateFlashlightOnPrimaryHandBinding;
        vrcf::InputBinding switchFlashlightFromWeaponToOffhandBinding;

        // Disable the vanilla game's global flashlight toggle so only this mod's gestures control the light.
        bool disableVanillaFlashlightToggle = true;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
