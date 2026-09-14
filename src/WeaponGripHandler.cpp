#include "WeaponGripHandler.h"

#include <Windows.h>

#include "api/FRIKApi.h"
#include "api/ROCKProviderApi.h"
#include "f4vr/F4VRUtils.h"

using namespace rock::provider;

namespace
{
    bool hasGripStateFlag(const RockProviderEquippedWeaponGripStateV1& state, const RockProviderEquippedWeaponGripStateFlagV1 flag)
    {
        return (state.flags & static_cast<std::uint32_t>(flag)) != 0;
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
     * Negotiate ROCK's provider API and register as a consumer granted the equipped-weapon grip-state
     * capability; the grip-state query refuses a caller without the ROCK-issued owner token. The animation-phase
     * capability is requested too, for the after-solve callback (see registerRockWeaponSolvedCallback()). ROCK
     * not being installed is the common case and only logged. The registration is kept for the process lifetime.
     */
    void WeaponGripHandler::initializeRock()
    {
        const int err = RockProviderApi::initialize();
        if (err != 0) {
            logger::info("ROCK API not available (error: {}), weapon grip is read from FRIK only", err);
            return;
        }
        logger::info("ROCK (v{}) API (v{}) init successful!", RockProviderApi::inst->getModVersion(), RockProviderApi::negotiatedApiVersion);

        if (!supportsEquippedWeaponGripStateV1()) {
            logger::warn("ROCK doesn't support the equipped-weapon grip state, weapon grip is read from FRIK only");
            return;
        }

        RockProviderConsumerRegistrationV1 registration{};
        strncpy_s(registration.modName, Version::PROJECT.data(), _TRUNCATE);
        registration.requestedCapabilities = static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::EquippedWeaponGripState);
        if (supportsAnimationPhasesV1()) {
            registration.requestedCapabilities |= static_cast<std::uint32_t>(RockProviderConsumerCapabilityV1::AnimationPhases);
        }
        RockProviderConsumerHandleV1 handle{};
        const auto result = RockProviderApi::inst->registerConsumerV1(&registration, &handle);
        if (result != RockProviderResultV1::Ok || !hasConsumerCapabilityV1(handle.grantedCapabilities, RockProviderConsumerCapabilityV1::EquippedWeaponGripState)) {
            logger::warn("ROCK consumer registration failed (result: {}, granted: 0x{:x}), weapon grip is read from FRIK only",
                static_cast<std::uint32_t>(result),
                handle.grantedCapabilities);
            return;
        }

        _rockOwnerToken = handle.ownerToken;
        logger::info("Registered with ROCK for the equipped-weapon grip state");

        _rockPartGripStateSupported = supportsWeaponPartGripStateV1();
        if (!_rockPartGripStateSupported) {
            logger::warn("ROCK doesn't support the weapon part grip state, a weapon carried by the offhand isn't detected");
        }

        if (hasConsumerCapabilityV1(handle.grantedCapabilities, RockProviderConsumerCapabilityV1::AnimationPhases)) {
            registerRockWeaponSolvedCallback();
        } else {
            logger::warn("ROCK didn't grant animation phases, weapon-anchored transforms may lag ROCK's weapon solve");
        }

        registerRockPhysicalTriggerRestore();
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
        if (!supportsRawWandButtonStateV1()) {
            logger::warn("ROCK doesn't support raw wand button state, the free hand's trigger is unreadable while ROCK fires from the left hand");
            return;
        }

        vrcf::VRControllers.setControllerStateAdjuster([](const vr::ETrackedControllerRole role, vr::VRControllerState_t& state) {
            if (!_rockTriggerRemapActive) {
                return;
            }
            const auto hand = role == vr::TrackedControllerRole_LeftHand ? RockProviderHand::Left : RockProviderHand::Right;
            RockProviderRawWandButtonStateV1 raw{};
            if (!RockProviderApi::inst->getRawWandButtonStateV1(hand, vr::k_EButton_SteamVR_Trigger, &raw) || !raw.available) {
                return;
            }
            const auto triggerMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
            state.ulButtonPressed = raw.held ? state.ulButtonPressed | triggerMask : state.ulButtonPressed & ~triggerMask;
        });
        logger::info("Restoring physical triggers from ROCK while it fires from the left hand");
    }

    /**
     * ROCK solves and writes the weapon transform in its own frame update, which runs after this mod's, so a
     * transform computed from the weapon node in onFrameUpdate() sees the pre-solve pose (following the primary
     * hand instead of pointing between both hands). ROCK's AfterRock phase fires right after that solve, before
     * the frame is rendered: forward it to the listener so weapon-anchored transforms are re-applied on the
     * final pose. Skipped while ROCK reports scene writes aren't allowed (menus, skeleton not ready).
     */
    void WeaponGripHandler::registerRockWeaponSolvedCallback()
    {
        const auto callback = [](const RockProviderAnimationPhaseContextV1* context, void*) {
            if (!context || context->phase != RockProviderAnimationPhaseV1::AfterRock || !_weaponTransformFinalizedListener) {
                return;
            }
            if ((context->flags & static_cast<std::uint32_t>(RockProviderAnimationPhaseContextFlagV1::VisualWritesAllowed)) == 0) {
                return;
            }
            _weaponTransformFinalizedListener();
        };

        std::uint64_t callbackToken = 0;
        const auto result = RockProviderApi::inst->registerAnimationPhaseCallbackV1(_rockOwnerToken, callback, nullptr, &callbackToken);
        if (result != RockProviderResultV1::Ok) {
            logger::warn("ROCK animation phase callback registration failed (result: {}), weapon-anchored transforms may lag ROCK's weapon solve",
                static_cast<std::uint32_t>(result));
            return;
        }
        logger::info("Registered with ROCK for the after weapon solve callback");
    }

    /**
     * Set the function run after a weapon-handling mod has written this frame's final weapon transform, to
     * re-apply anything anchored to the weapon node. Only ROCK needs it (FRIK's pose is final before this mod's
     * frame update), so without ROCK it is never called. Runs on the game thread.
     */
    void WeaponGripHandler::setWeaponTransformFinalizedListener(std::function<void()> listener)
    {
        _weaponTransformFinalizedListener = std::move(listener);
    }

    /**
     * Sample the weapon grip from FRIK and ROCK once per frame. Must run on the game thread: ROCK answers the
     * grip-state query only on its animation owner thread. Grip changes are logged to make in-game testing of
     * the ROCK integration traceable.
     */
    void WeaponGripHandler::onFrameUpdate()
    {
        const bool frikTwoHanded = frik::api::FRIKApi::inst && frik::api::FRIKApi::inst->isOffHandGrippingWeapon();

        bool rockTwoHanded = false;
        bool rockFiringHandLeft = false;
        const bool rockGripValid = queryRockGripState(rockTwoHanded, rockFiringHandLeft);
        // ROCK reports the firing hand as a physical hand; map it to the offhand by handedness.
        const bool rockWeaponInOffhand = rockGripValid && rockFiringHandLeft != f4vr::isLeftHandedMode();

        // The offhand is the physical left hand unless the player is left-handed.
        const bool rockCarriedByOffhand = rockGripValid && isRockHandCarryingWeapon(!f4vr::isLeftHandedMode());

        const bool twoHanded = frikTwoHanded || rockTwoHanded;
        if (twoHanded != _twoHandedGripActive || rockWeaponInOffhand != _weaponInOffhand || rockCarriedByOffhand != _weaponCarriedByOffhand) {
            logger::info("Weapon grip changed: two-handed={} (FRIK={}, ROCK={}), weapon in offhand={}, carried by offhand={}",
                twoHanded,
                frikTwoHanded,
                rockTwoHanded,
                rockWeaponInOffhand,
                rockCarriedByOffhand);
        }
        _twoHandedGripActive = twoHanded;
        _weaponInOffhand = rockWeaponInOffhand;
        _weaponCarriedByOffhand = rockCarriedByOffhand;
        // ROCK's remap keys on the physical left hand firing, regardless of the game's handedness setting.
        _rockTriggerRemapActive = rockFiringHandLeft;
    }

    /**
     * Read ROCK's grip on the equipped weapon (both false without ROCK or a weapon).
     * @return true if ROCK reported a valid grip state (ROCK installed and a weapon drawn).
     */
    bool WeaponGripHandler::queryRockGripState(bool& twoHanded, bool& firingHandLeft)
    {
        if (_rockOwnerToken == 0) {
            return false;
        }

        RockProviderEquippedWeaponGripStateV1 state{};
        if (!RockProviderApi::inst->getEquippedWeaponGripStateV1(_rockOwnerToken, &state) || !hasGripStateFlag(state, RockProviderEquippedWeaponGripStateFlagV1::Valid)) {
            return false;
        }

        twoHanded = hasGripStateFlag(state, RockProviderEquippedWeaponGripStateFlagV1::TwoHandGripActive);
        firingHandLeft = hasGripStateFlag(state, RockProviderEquippedWeaponGripStateFlagV1::FiringHandLeft);
        return true;
    }

    /**
     * Is the given physical hand carrying the weapon with its firing grip detached (ROCK's part carry, e.g. a
     * rifle held by the foregrip only). The carrying hand holds the weapon but can't fire it.
     */
    bool WeaponGripHandler::isRockHandCarryingWeapon(const bool left)
    {
        if (!_rockPartGripStateSupported) {
            return false;
        }
        RockProviderWeaponPartGripStateV1 state{};
        return RockProviderApi::inst->getWeaponPartGripStateV1(left ? RockProviderHand::Left : RockProviderHand::Right, &state) && state.active &&
            state.gripKind == RockProviderWeaponPartGripKindV1::PartCarry;
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
     * Is the weapon carried one-handed in the offhand, so the primary hand is free (ROCK only).
     */
    bool WeaponGripHandler::isWeaponOneHandedInOffhand()
    {
        return _weaponInOffhand && !_twoHandedGripActive;
    }

    /**
     * Is the offhand carrying the weapon with the firing grip detached (ROCK only): the offhand holds a weapon
     * it can't fire, and the primary hand is free.
     */
    bool WeaponGripHandler::isWeaponCarriedByOffhand()
    {
        return _weaponCarriedByOffhand;
    }

    /**
     * Is the offhand holding the weapon itself — as the firing hand or carrying it — rather than supporting it.
     */
    bool WeaponGripHandler::isOffhandHoldingWeapon()
    {
        return _weaponInOffhand || _weaponCarriedByOffhand;
    }

    /**
     * Is the primary hand free while a weapon is drawn, because the offhand holds the weapon alone.
     */
    bool WeaponGripHandler::isPrimaryHandFreeOfWeapon()
    {
        return isWeaponOneHandedInOffhand() || _weaponCarriedByOffhand;
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
