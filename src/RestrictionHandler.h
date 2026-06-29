#pragma once

#include <cstdint>
#include <unordered_set>

namespace ImFl
{
    /**
     * Optional gameplay restrictions on where the flashlight may be active.
     *
     * Gates the head-mounted light (out of power armor) behind worn headgear per
     * Config::flashlightHeadgearRequirement, and optionally requires a modeled flashlight on the equipped
     * weapon before the light may mount on it (Config::weaponFlashlightMeshRequirement). Static like Utils:
     * there is a single player / flashlight, and the resolved form sets + detection caches are global state.
     */
    class RestrictionHandler
    {
    public:
        static bool isHeadFlashlightAllowed();
        static bool isWeaponEquipped();
        static bool isWeaponFlashlightAllowed();
        static bool isWeaponFlashlightMeshRequired();
        static bool onFrameUpdate();
        static void invalidate();
        static std::pair<RE::NiAVObject*, RE::NiTransform> getOnWeaponFlashlightMeshNode();
        static void dumpHeadgear();

    private:
        static void resolveForms();
        static bool resolveWeaponFlashlightMeshRequired();
        static bool checkWeaponChangeForFlashlightOnWeaponDetection();
        static void enforceRestrictions();
        static const RE::TESObjectARMO* getWornHeadgear();
        static bool isLightCapableHeadgear(const RE::TESObjectARMO* armor);
        static bool isLightCapableHeadgearWorn();
        static RE::NiAVObject* findWeaponFlashlightNode(RE::NiAVObject* weaponNode);

        // Immersive head-restriction rule, resolved from Config to runtime FormIDs by resolveForms().
        inline static std::unordered_set<std::uint32_t> _headLightKeywordIds;
        inline static std::unordered_set<std::uint32_t> _headLightAllowIds;
        inline static std::unordered_set<std::uint32_t> _headLightDenyIds;

        // Effective on/off of the weapon-flashlight-mesh requirement, resolved from
        // Config::weaponFlashlightMeshRequirement by resolveForms() (AutoDetect checks installed plugins).
        inline static bool _weaponFlashlightMeshRequired = false;

        // Weapon-flashlight detection state
        inline static bool _currentWeaponMelee = false;
        inline static RE::TESObjectWEAP* _currentWeapon = nullptr;
        inline static RE::NiAVObject* _weaponFlashlightNode = nullptr;
    };
}
