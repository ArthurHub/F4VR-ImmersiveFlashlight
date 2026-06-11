#pragma once

#include "ConfigBase.h"
#include "Resources.h"
#include "api/FRIKApi.h"

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

    // TODO: should unify to 1 enum!
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

        // button to use to switch flashlight between head and hand
        int switchTorchButton = 2;

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
        // Auto-detect tuning. Angle (degrees) between the controller's top axis and world up: 0 = top
        // pointing straight up (Forward grip), 180 = top pointing straight down (Overhand fist grip).
        // The grip switches to Overhand when the tilt exceeds this threshold; it switches back to
        // Forward when the tilt drops below threshold - hysteresis (a stable deadband).
        float flashlightGripOverhandTiltDegrees = 0.0f;
        float flashlightGripHysteresisDegrees = 0.0f;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
