#pragma once

#include <cstdint>
#include <functional>

#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    /**
     * Which hand(s) hold the equipped weapon, abstracted over the mods that drive weapon handling, so the rest
     * of the mod doesn't care which one provides it.
     *
     * FRIK reports a two-handed grip (the offhand on the weapon). ROCK, when installed, takes over weapon
     * handling and disables FRIK's two-handed grip: it reports its own two-handed grip, and can also make the
     * offhand the firing hand — one-handed that leaves the primary hand free — or detach the firing hand and let
     * the offhand carry the weapon it can't fire (a rifle). A grip reported by either mod counts. ROCK can also
     * put a second weapon in the other hand: the light follows the game's weapon (ROCK's slot 0) and only treats
     * the hand holding the second one as busy.
     *
     * ROCK also moves the weapon itself (e.g. solving a two-handed rifle to point between the hands) after this
     * mod's frame update has run, so anything anchored to the weapon node must be re-applied once that pose is
     * final — see setWeaponTransformFinalizedListener(). And while the left hand fires, ROCK remaps the
     * trigger between the hands for every OpenVR reader, so the physical triggers are restored from ROCK for
     * this mod's input (see adjustBindingForInputRemap()).
     * Static like RestrictionHandler: there is a single player and the state is sampled once per frame.
     */
    class WeaponGripHandler
    {
    public:
        static void initialize();
        static void onFrameUpdate();
        static bool isTwoHandedGripActive();
        static bool isWeaponInOffhand();
        static bool isWeaponOneHandedInOffhand();
        static bool isWeaponCarriedByOffhand();
        static bool isOffhandHoldingWeapon();
        static bool isPrimaryHandFreeOfWeapon();
        static vrcf::InputBinding adjustBindingForInputRemap(const vrcf::InputBinding& binding);
        static void setWeaponTransformFinalizedListener(std::function<void()> listener);

    private:
        static void initializeRock();
        static void registerRockPhysicalTriggerRestore();

        // Invoked after a weapon-handling mod has written this frame's final weapon transform.
        inline static std::function<void()> _weaponTransformFinalizedListener;

        // State sampled by onFrameUpdate().
        inline static bool _twoHandedGripActive = false;
        inline static bool _weaponInOffhand = false;
        inline static bool _weaponCarriedByOffhand = false;
        // A hand holds ROCK's second weapon (slot 1), so it isn't free for the light.
        inline static bool _secondWeaponInPrimaryHand = false;
        inline static bool _secondWeaponInOffhand = false;
        // ROCK is remapping the trigger between the hands for OpenVR readers (the physical left hand fires).
        inline static bool _rockTriggerRemapActive = false;
    };
}
