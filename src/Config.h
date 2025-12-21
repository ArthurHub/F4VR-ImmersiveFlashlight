#pragma once

#include "ConfigBase.h"
#include "Resources.h"

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

    class Config : public ConfigBase
    {
    public:
        explicit Config() :
            ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI) {}

        void setFlashlightLocation(FlashlightConfigLocation location);
        void saveFlashlightValues(FlashlightLocation location);
        void resetFlashlightValuesToDefault(FlashlightLocation location);

        // Flashlight
        FlashlightConfigLocation flashlightConfigLocation = FlashlightConfigLocation::OnHead;

        // flashlight values on head
        float flashlightOnHeadFade = 0.0f;
        int flashlightOnHeadRadius = 0;
        float flashlightOnHeadFov = 0.0f;
        int flashlightOnHeadColorRed = 0;
        int flashlightOnHeadColorGreen = 0;
        int flashlightOnHeadColorBlue = 0;
        std::string flashlightOnHeadGoboPath;
        float flashlightOnHeadAngleOffset = 0;

        // flashlight values in hands
        float flashlightInHandFade = 0.0f;
        int flashlightInHandRadius = 0;
        float flashlightInHandFov = 0.0f;
        int flashlightInHandColorRed = 0;
        int flashlightInHandColorGreen = 0;
        int flashlightInHandColorBlue = 0;
        std::string flashlightInHandGoboPath;
        float flashlightInHandControllerAngleOffset = 0;

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

        // button to use to switch flashlight between head and hand
        int switchTorchButton = 2;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
