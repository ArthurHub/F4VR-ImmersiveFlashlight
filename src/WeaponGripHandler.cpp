#include "WeaponGripHandler.h"

#include <Windows.h>

#include "api/FRIKApi.h"
#include "api/ROCK/Core.h"
#include "api/ROCK/Discovery.h"
#include "api/ROCK/Input.h"
#include "api/ROCK/Weapon.h"
#include "api/ROCK/WeaponParts.h"
#include "f4vr/F4VRUtils.h"

using namespace rock::api;

namespace
{
    // Table bytes covering the last slot this mod calls in each ROCK interface (see queryRockInterface()).
    constexpr auto ROCK_CORE_TABLE_BYTES = static_cast<std::uint32_t>(offsetof(core::ApiV1, bindInterface) + sizeof(void*));
    constexpr auto ROCK_WEAPON_TABLE_BYTES = static_cast<std::uint32_t>(offsetof(weapon::ApiV1, getEquippedWeaponGripStateV1) + sizeof(void*));
    constexpr auto ROCK_WEAPON_PARTS_TABLE_BYTES = static_cast<std::uint32_t>(offsetof(weaponparts::ApiV1, getWeaponPartGripStateV1) + sizeof(void*));
    constexpr auto ROCK_INPUT_TABLE_BYTES = static_cast<std::uint32_t>(offsetof(input::ApiV1, getRawWandButtonStateV1) + sizeof(void*));

    bool hasGripStateFlag(const weapon::EquippedWeaponGripStateV1& state, const weapon::EquippedWeaponGripStateFlagV1 flag)
    {
        return (state.flags & static_cast<std::uint32_t>(flag)) != 0;
    }

    /**
     * Discover one of ROCK's interface tables (major 1) through its query export.
     *
     * Asks only for what this mod calls so that ROCK updates don't break it: minor 0 (every call used here is
     * in the 1.0 table) and the table extent ending at the last slot called. ROCK only appends calls within a
     * major (a higher minor and a longer table) and keeps its records frozen, so a newer ROCK still matches
     * with the copied headers. Unlike the SDK's Client::acquire(), the descriptor's Core requirement isn't
     * checked against the copied Core header's minor: the Core in use is the DLL's own, which meets it, and
     * that check would reject every interface once ROCK bumps its Core minor.
     */
    template <class Table>
    const Table* queryRockInterface(const QueryInterfaceV1 query, const std::uint32_t tableBytes, Status& status)
    {
        const InterfaceDescriptorV1* descriptor = nullptr;
        status = query(Table::interfaceId, Table::majorVersion, 0, tableBytes, &descriptor);
        if (status != Status::Ok) {
            return nullptr;
        }
        if (!descriptor || descriptor->size < sizeof(InterfaceDescriptorV1) || descriptor->interfaceId != Table::interfaceId || descriptor->major != Table::majorVersion ||
            descriptor->tableByteSize < tableBytes || !descriptor->table) {
            status = Status::InvalidSize;
            return nullptr;
        }
        return static_cast<const Table*>(descriptor->table);
    }

    /**
     * Discover a ROCK interface table and bind the owner to it with the given permissions (local to that
     * interface). Logs why it's unavailable, with the consequence for this mod, and returns null then.
     */
    template <class Table>
    const Table* acquireRockInterface(const QueryInterfaceV1 query, const core::ApiV1* coreApi, const OwnerToken owner, const std::uint32_t tableBytes,
        const std::uint32_t permissions, const std::string_view name, const std::string_view consequence)
    {
        Status status;
        const auto* table = queryRockInterface<Table>(query, tableBytes, status);
        if (table) {
            status = coreApi->bindInterface(owner, Table::interfaceId, Table::majorVersion, permissions);
        }
        if (status != Status::Ok) {
            logger::warn("ROCK {} interface not available (status: {}), {}", name, static_cast<std::uint32_t>(status), consequence);
            return nullptr;
        }
        return table;
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
     * Connect to ROCK's API: discover Core through ROCK.dll's query export, register this mod as an owner, then
     * bind the owner to each interface it reads — Weapon (the grip state, required), WeaponParts (the offhand
     * carry) and Input (the physical triggers) — plus Core callbacks for the after-solve callback (see
     * registerRockWeaponSolvedCallback()). Each call is refused without the ROCK-issued owner token and the
     * interface's permission. ROCK not being installed is the common case and only logged. The registration is
     * kept for the process lifetime.
     */
    void WeaponGripHandler::initializeRock()
    {
        const auto rockDll = GetModuleHandleA("ROCK.dll");
        const auto query = rockDll ? reinterpret_cast<QueryInterfaceV1>(GetProcAddress(rockDll, kQueryExportName)) : nullptr;
        if (!query) {
            logger::info("ROCK API not available, weapon grip is read from FRIK only");
            return;
        }

        Status status;
        const auto* coreApi = queryRockInterface<core::ApiV1>(query, ROCK_CORE_TABLE_BYTES, status);
        if (!coreApi) {
            logger::warn("ROCK Core interface not available (status: {}), weapon grip is read from FRIK only", static_cast<std::uint32_t>(status));
            return;
        }

        core::RegistrationV1 registration{};
        strncpy_s(registration.modName, Version::PROJECT.data(), _TRUNCATE);
        core::OwnerV1 owner{};
        status = coreApi->registerConsumerV1(&registration, &owner);
        if (status != Status::Ok) {
            logger::warn("ROCK consumer registration failed (status: {}), weapon grip is read from FRIK only", static_cast<std::uint32_t>(status));
            return;
        }
        const char* modVersion = nullptr;
        coreApi->getModVersion(owner.ownerToken, &modVersion);
        logger::info("Registered with ROCK (v{})", modVersion ? modVersion : "unknown");

        const auto read = static_cast<std::uint32_t>(weapon::PermissionV1::Read);
        _rockWeapon = acquireRockInterface<weapon::ApiV1>(query, coreApi, owner.ownerToken, ROCK_WEAPON_TABLE_BYTES, read, "Weapon", "weapon grip is read from FRIK only");
        if (!_rockWeapon) {
            coreApi->unregisterConsumerV1(owner.ownerToken);
            return;
        }
        _rockOwnerToken = owner.ownerToken;

        _rockWeaponParts = acquireRockInterface<weaponparts::ApiV1>(query,
            coreApi,
            _rockOwnerToken,
            ROCK_WEAPON_PARTS_TABLE_BYTES,
            read,
            "WeaponParts",
            "a weapon carried by the offhand isn't detected");
        _rockInput = acquireRockInterface<input::ApiV1>(query,
            coreApi,
            _rockOwnerToken,
            ROCK_INPUT_TABLE_BYTES,
            read,
            "Input",
            "the free hand's trigger is unreadable while ROCK fires from the left hand");

        registerRockWeaponSolvedCallback(coreApi);
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
        if (!_rockInput) {
            return;
        }

        vrcf::VRControllers.setControllerStateAdjuster([](const vr::ETrackedControllerRole role, vr::VRControllerState_t& state) {
            if (!_rockTriggerRemapActive) {
                return;
            }
            const auto hand = role == vr::TrackedControllerRole_LeftHand ? Hand::Left : Hand::Right;
            input::RawWandButtonStateV1 raw{};
            if (_rockInput->getRawWandButtonStateV1(_rockOwnerToken, hand, vr::k_EButton_SteamVR_Trigger, &raw) != Status::Ok || !raw.available) {
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
     * Registering needs Core's callbacks permission on top of the Read that registration grants.
     */
    void WeaponGripHandler::registerRockWeaponSolvedCallback(const core::ApiV1* coreApi)
    {
        const auto callback = [](const core::AnimationPhaseContextV1* context, void*) {
            if (!context || context->phase != core::AnimationPhaseV1::AfterRock || !_weaponTransformFinalizedListener) {
                return;
            }
            if ((context->flags & static_cast<std::uint32_t>(core::AnimationPhaseContextFlagV1::VisualWritesAllowed)) == 0) {
                return;
            }
            _weaponTransformFinalizedListener();
        };

        const auto permissions = static_cast<std::uint32_t>(core::PermissionV1::Read) | static_cast<std::uint32_t>(core::PermissionV1::Callbacks);
        auto status = coreApi->bindInterface(_rockOwnerToken, core::kInterfaceId, core::kMajor, permissions);
        std::uint64_t callbackToken = 0;
        if (status == Status::Ok) {
            status = coreApi->registerAnimationPhaseCallbackV1(_rockOwnerToken, callback, nullptr, &callbackToken);
        }
        if (status != Status::Ok) {
            logger::warn("ROCK animation phase callback registration failed (status: {}), weapon-anchored transforms may lag ROCK's weapon solve",
                static_cast<std::uint32_t>(status));
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
        if (!_rockWeapon) {
            return false;
        }

        weapon::EquippedWeaponGripStateV1 state{};
        if (_rockWeapon->getEquippedWeaponGripStateV1(_rockOwnerToken, &state) != Status::Ok || !hasGripStateFlag(state, weapon::EquippedWeaponGripStateFlagV1::Valid)) {
            return false;
        }

        twoHanded = hasGripStateFlag(state, weapon::EquippedWeaponGripStateFlagV1::TwoHandGripActive);
        firingHandLeft = hasGripStateFlag(state, weapon::EquippedWeaponGripStateFlagV1::FiringHandLeft);
        return true;
    }

    /**
     * Is the given physical hand carrying the weapon with its firing grip detached (ROCK's part carry, e.g. a
     * rifle held by the foregrip only). The carrying hand holds the weapon but can't fire it.
     */
    bool WeaponGripHandler::isRockHandCarryingWeapon(const bool left)
    {
        if (!_rockWeaponParts) {
            return false;
        }
        weaponparts::WeaponPartGripStateV1 state{};
        return _rockWeaponParts->getWeaponPartGripStateV1(_rockOwnerToken, left ? Hand::Left : Hand::Right, &state) == Status::Ok && state.active &&
            state.gripKind == weaponparts::WeaponPartGripKindV1::PartCarry;
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
