#pragma once

#include <cstdint>
#include <optional>

namespace ImFl
{
    /**
     * The few facts this mod reads from ROCK, in the mod's own types, so a ROCK API change stays in RockBridge.cpp
     * (the only file that includes ROCK's headers, copied as-is into api/ROCK/). Everything is per physical hand: mapping to the primary hand
     * and offhand, and what the facts mean for the light, is WeaponGripHandler's.
     * Without ROCK (or before initialize()) every read reports nothing held. Game thread only.
     */
    class RockBridge
    {
    public:
        // ROCK's weapon slots: the game's own equipped weapon, and the second weapon ROCK places in the other hand.
        static constexpr std::uint32_t GAME_WEAPON_SLOT = 0;
        static constexpr std::uint32_t SECOND_WEAPON_SLOT = 1;
        static constexpr std::uint32_t NO_WEAPON_SLOT = 0xFFFFFFFFu;

        // How a hand holds a weapon: its firing grip, or another grip of it (support, or a part carried with the
        // firing grip vacated).
        enum class GripRole : std::uint8_t
        {
            None,
            Firing,
            Support,
        };

        struct HandGrip
        {
            std::uint32_t weaponSlot = NO_WEAPON_SLOT;
            GripRole role = GripRole::None;
        };

        struct HandGrips
        {
            HandGrip right;
            HandGrip left;
        };

        static bool initialize(void (*onWeaponSolved)());
        static HandGrips getHandGrips();
        static bool isGameWeaponFiredByLeftHand();
        static bool canReadPhysicalTriggers();
        static std::optional<bool> isPhysicalTriggerHeld(bool left);
    };
}
