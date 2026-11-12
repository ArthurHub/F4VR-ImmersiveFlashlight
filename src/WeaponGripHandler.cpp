#include "WeaponGripHandler.h"

#include "api/FRIKApi.h"
#include "api/RockBridge.h"
#include "f4vr/F4VRUtils.h"

namespace
{
    // ROCK's grip on the weapons, per hand of the player's handedness.
    struct RockGrip
    {
        bool twoHanded = false;
        bool weaponInOffhand = false;
        bool carriedByOffhand = false;
        bool secondWeaponInPrimaryHand = false;
        bool secondWeaponInOffhand = false;
    };

    /**
     * Map ROCK's per-physical-hand grips to the primary hand and offhand by handedness. The light follows the
     * game's weapon (slot 0); a hand on the second weapon is only reported as holding it. All false without ROCK
     * or a held weapon.
     */
    RockGrip readRockGrip()
    {
        using ImFl::RockBridge;
        const auto grips = RockBridge::getHandGrips();
        const bool leftHanded = f4vr::isLeftHandedMode();
        const auto& primary = leftHanded ? grips.left : grips.right;
        const auto& offhand = leftHanded ? grips.right : grips.left;
        const bool primaryOnGameWeapon = primary.weaponSlot == RockBridge::GAME_WEAPON_SLOT;
        const bool offhandOnGameWeapon = offhand.weaponSlot == RockBridge::GAME_WEAPON_SLOT;

        RockGrip grip;
        grip.twoHanded = primaryOnGameWeapon && offhandOnGameWeapon;
        grip.weaponInOffhand = offhandOnGameWeapon && offhand.role == RockBridge::GripRole::Firing;
        grip.carriedByOffhand = offhandOnGameWeapon && offhand.role == RockBridge::GripRole::Support && !primaryOnGameWeapon;
        grip.secondWeaponInPrimaryHand = primary.weaponSlot == RockBridge::SECOND_WEAPON_SLOT;
        grip.secondWeaponInOffhand = offhand.weaponSlot == RockBridge::SECOND_WEAPON_SLOT;
        return grip;
    }
}

namespace ImFl
{
    /**
     * Connect to the weapon-handling mods that need a handshake. Call once on game loaded, when the mod DLLs
     * are loaded. FRIK needs nothing here: its API is initialized by FlashlightMod and read live.
     */
    void WeaponGripHandler::initialize()
    {
        initializeRock();
    }

    /**
     * Connect to ROCK (see RockBridge::initialize()), forwarding its after-solve callback to the listener, and
     * restore the physical triggers while ROCK remaps them. ROCK not being installed is the common case.
     */
    void WeaponGripHandler::initializeRock()
    {
        const auto onWeaponSolved = [] {
            if (_weaponTransformFinalizedListener) {
                _weaponTransformFinalizedListener();
            }
        };
        if (RockBridge::initialize(onWeaponSolved)) {
            registerRockPhysicalTriggerRestore();
        }
    }

    /**
     * While the physical left hand fires, ROCK rewrites the controller state for every OpenVR reader (this mod
     * included): the left trigger is presented on the right controller and the left one reads empty, so the
     * free right hand's trigger is invisible. Restore both triggers' pressed bits from ROCK's pre-remap raw
     * wand state for this mod's input manager. Level state only (the manager derives the edges); the trigger
     * axis is not restored. Only while the remap is active, so ROCK's menu gating of the raw state never
     * reaches ordinary input.
     */
    void WeaponGripHandler::registerRockPhysicalTriggerRestore()
    {
        if (!RockBridge::canReadPhysicalTriggers()) {
            return;
        }

        vrcf::VRControllers.setControllerStateAdjuster([](const vr::ETrackedControllerRole role, vr::VRControllerState_t& state) {
            if (!_rockTriggerRemapActive) {
                return;
            }
            const auto held = RockBridge::isPhysicalTriggerHeld(role == vr::TrackedControllerRole_LeftHand);
            if (!held) {
                return;
            }
            const auto triggerMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
            state.ulButtonPressed = *held ? state.ulButtonPressed | triggerMask : state.ulButtonPressed & ~triggerMask;
        });
        logger::info("Restoring physical triggers from ROCK while it fires from the left hand");
    }

    /**
     * Set the function run after a weapon-handling mod has written this frame's final weapon transform, to
     * re-apply anything anchored to the weapon node. Only ROCK needs it: it solves the weapon (e.g. a two-handed
     * rifle pointing between both hands) in its own frame update, after this mod's, so a transform computed in
     * onFrameUpdate() sees the pre-solve pose. FRIK's pose is final before this mod's frame update, so without
     * ROCK it is never called. Runs on the game thread.
     */
    void WeaponGripHandler::setWeaponTransformFinalizedListener(std::function<void()> listener)
    {
        _weaponTransformFinalizedListener = std::move(listener);
    }

    /**
     * Sample the weapon grip from FRIK and ROCK once per frame. Must run on the game thread: ROCK answers the
     * weapon-state query only there. Grip changes are logged to make in-game testing of the ROCK integration
     * traceable.
     */
    void WeaponGripHandler::onFrameUpdate()
    {
        F4CF_PERF_FUNCTION();
        const bool frikTwoHanded = frik::api::FRIKApi::inst && frik::api::FRIKApi::inst->isOffHandGrippingWeapon();
        const auto rock = readRockGrip();

        const bool twoHanded = frikTwoHanded || rock.twoHanded;
        if (twoHanded != _twoHandedGripActive || rock.weaponInOffhand != _weaponInOffhand || rock.carriedByOffhand != _weaponCarriedByOffhand ||
            rock.secondWeaponInPrimaryHand != _secondWeaponInPrimaryHand || rock.secondWeaponInOffhand != _secondWeaponInOffhand) {
            logger::info("Weapon grip changed: two-handed={} (FRIK={}, ROCK={}), weapon in offhand={}, carried by offhand={}, second weapon in primary hand={}, in offhand={}",
                twoHanded,
                frikTwoHanded,
                rock.twoHanded,
                rock.weaponInOffhand,
                rock.carriedByOffhand,
                rock.secondWeaponInPrimaryHand,
                rock.secondWeaponInOffhand);
        }
        _twoHandedGripActive = twoHanded;
        _weaponInOffhand = rock.weaponInOffhand;
        _weaponCarriedByOffhand = rock.carriedByOffhand;
        _secondWeaponInPrimaryHand = rock.secondWeaponInPrimaryHand;
        _secondWeaponInOffhand = rock.secondWeaponInOffhand;
        // ROCK's remap keys on the physical left hand firing; ROCK holds the game's left-handed mode off.
        _rockTriggerRemapActive = RockBridge::isGameWeaponFiredByLeftHand();
    }

    /**
     * Is the weapon held with both hands, reported by FRIK or ROCK. Which hand fires is isWeaponInOffhand().
     */
    bool WeaponGripHandler::isTwoHandedGripActive()
    {
        return _twoHandedGripActive;
    }

    /**
     * Is the offhand the firing hand carrying the weapon (ROCK only), one-handed (the primary hand is free) or
     * two-handed (the primary hand supports).
     */
    bool WeaponGripHandler::isWeaponInOffhand()
    {
        return _weaponInOffhand;
    }

    /**
     * Is the weapon carried one-handed in the offhand, so the primary hand is free (ROCK only). A second weapon in
     * the primary hand keeps it busy.
     */
    bool WeaponGripHandler::isWeaponOneHandedInOffhand()
    {
        return _weaponInOffhand && !_twoHandedGripActive && !_secondWeaponInPrimaryHand;
    }

    /**
     * Is the offhand carrying the weapon with the firing grip detached (ROCK only): the offhand holds a weapon
     * it can't fire, and the primary hand is free unless it holds a second weapon.
     */
    bool WeaponGripHandler::isWeaponCarriedByOffhand()
    {
        return _weaponCarriedByOffhand;
    }

    /**
     * Is the offhand holding a weapon itself — firing or carrying the weapon, or holding ROCK's second weapon —
     * rather than supporting it.
     */
    bool WeaponGripHandler::isOffhandHoldingWeapon()
    {
        return _weaponInOffhand || _weaponCarriedByOffhand || _secondWeaponInOffhand;
    }

    /**
     * Is the primary hand free while a weapon is drawn, because the offhand holds the weapon alone and the primary
     * hand holds no second weapon.
     */
    bool WeaponGripHandler::isPrimaryHandFreeOfWeapon()
    {
        return isWeaponOneHandedInOffhand() || (_weaponCarriedByOffhand && !_secondWeaponInPrimaryHand);
    }

    /**
     * The binding as a suppressing consumer (e.g. an activation sphere) should use it this frame. While ROCK
     * remaps the trigger, the game-facing trigger no longer matches the physical one: suppressing a trigger
     * binding would hide the remapped fire trigger instead, and the free hand's physical trigger is already
     * hidden from the game by ROCK. So trigger bindings are fed unsuppressed then; otherwise unchanged.
     */
    vrcf::InputBinding WeaponGripHandler::adjustBindingForInputRemap(const vrcf::InputBinding& binding)
    {
        const bool onTrigger = binding.type == vrcf::ActivationType::AxisDirection ? binding.axis == vrcf::Axis::Trigger : binding.button == vr::k_EButton_SteamVR_Trigger;
        if (!_rockTriggerRemapActive || !onTrigger) {
            return binding;
        }
        auto adjusted = binding;
        adjusted.suppress = false;
        return adjusted;
    }
}
