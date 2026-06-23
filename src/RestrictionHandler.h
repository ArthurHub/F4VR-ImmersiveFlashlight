#pragma once

#include <cstdint>
#include <unordered_set>

namespace ImFl
{
    /**
     * Optional gameplay restrictions on where the flashlight may be active.
     *
     * Currently gates the head-mounted light (out of power armor) behind worn headgear per
     * Config::flashlightHeadgearRequirement; weapon-mount restriction is planned to live here too.
     * Static like Utils: there is a single player / flashlight, and the resolved form sets are global state.
     */
    class RestrictionHandler
    {
    public:
        // Resolve the configured restriction lists (the Immersive head keyword / allow / deny entries) from
        // Config to runtime FormIDs. Call once after game data is ready, and again on config reload.
        static void resolveForms();

        // Whether the flashlight is currently allowed on the (non-PA) head per the configured requirement.
        static bool isHeadFlashlightAllowed();

        // Enforce all active restrictions for this frame (currently: turn an on-head light off when the
        // headgear requirement is no longer met). Call each frame after the runtime location is resolved.
        static void enforce();

    private:
        // The armor worn in the head slot (slot 30 / biped index 0 = kHairTop), or nullptr if nothing is worn there.
        static const RE::TESObjectARMO* getWornHeadgear();
        // Immersive rule: the worn headgear is light-capable (not denied, and either allow-listed or carries a configured keyword).
        static bool isLightCapableHeadgearWorn();

        // Immersive head-restriction rule, resolved from Config to runtime FormIDs by resolveForms().
        inline static std::unordered_set<std::uint32_t> _headLightKeywordIds;
        inline static std::unordered_set<std::uint32_t> _headLightAllowIds;
        inline static std::unordered_set<std::uint32_t> _headLightDenyIds;
    };
}
