#include "RockBridge.h"

#include <Windows.h>

#include "api/ROCK/Animation.h"
#include "api/ROCK/Client.h"
#include "api/ROCK/Hands.h"
#include "api/ROCK/Input.h"
#include "api/ROCK/Weapon.h"
#include "vrcf/VRControllersManager.h"

using namespace rock::api;

namespace
{
    using ImFl::RockBridge;

    // ROCK-issued consumer token, non-zero once registered with ROCK and bound to its Hands interface.
    OwnerToken g_owner = 0;
    // ROCK interface tables bound to the owner; null when ROCK is absent or doesn't offer the interface.
    const hands::Api* g_hands = nullptr;
    const weapon::Api* g_weapon = nullptr;
    const input::Api* g_input = nullptr;
    // Run from ROCK's after-solve phase callback.
    void (*g_onWeaponSolved)() = nullptr;

    /**
     * Discover one of ROCK's interface tables through its query export and bind the owner to it with the given
     * permissions (local to that interface). Requests exactly the copied header's major, minor and table size:
     * ROCK only appends calls, and stops the game naming this DLL on a refused query, so never probe.
     * Unlike Client::acquire(), the descriptor's Core requirement isn't checked against the copied Core header's
     * minor: the Core in use is the DLL's own, which meets it, and that check would reject every interface once
     * ROCK bumps its Core minor. Logs why it's unavailable, with the consequence for this mod, and returns null then.
     */
    template <class Table>
    const Table* acquireRockInterface(const QueryInterfaceV1 query, const Client& client, const std::uint32_t permissions, const std::string_view name,
        const std::string_view consequence)
    {
        const InterfaceDescriptorV1* descriptor = nullptr;
        auto status = query(Table::interfaceId, Table::majorVersion, Table::minorVersion, sizeof(Table), &descriptor);
        if (status == Status::Ok &&
            (!descriptor || descriptor->size != sizeof(InterfaceDescriptorV1) || descriptor->interfaceId != Table::interfaceId || descriptor->major != Table::majorVersion ||
                descriptor->minor < Table::minorVersion || descriptor->tableByteSize < sizeof(Table) || !descriptor->table)) {
            status = Status::InvalidSize;
        }
        if (status == Status::Ok) {
            status = client.core()->bindInterface(client.owner(), Table::interfaceId, Table::majorVersion, permissions);
        }
        if (status != Status::Ok) {
            logger::warn("ROCK {} interface not available (status: {}), {}", name, static_cast<std::uint32_t>(status), consequence);
            return nullptr;
        }
        return static_cast<const Table*>(descriptor->table);
    }

    /**
     * ROCK solves and writes the weapon transform in its own frame update, which runs after this mod's. Its
     * AfterRock phase fires right after that solve, before the frame is rendered: run g_onWeaponSolved then.
     * Skipped while ROCK reports scene writes aren't allowed (menus, skeleton not ready). The callback is
     * registered with Animation, bound with its Callbacks permission (null when unavailable).
     */
    void registerWeaponSolvedCallback(const animation::Api* animationApi)
    {
        if (!animationApi || !g_onWeaponSolved) {
            return;
        }

        const auto callback = [](const animation::AnimationPhaseContext* context, void*) {
            if (!context || (context->flags & static_cast<std::uint32_t>(animation::AnimationPhaseContextFlag::VisualWritesAllowed)) == 0) {
                return;
            }
            g_onWeaponSolved();
        };

        animation::PhaseCallbackRegistration registration{};
        registration.phaseMask = animation::phaseBit(animation::AnimationPhase::AfterRock);
        registration.callback = callback;
        std::uint64_t callbackToken = 0;
        const auto status = animationApi->registerAnimationPhaseCallback(g_owner, &registration, &callbackToken);
        if (status != Status::Ok) {
            logger::warn("ROCK animation phase callback registration failed (status: {}), weapon-anchored transforms may lag ROCK's weapon solve",
                static_cast<std::uint32_t>(status));
            return;
        }
        logger::info("Registered with ROCK for the after weapon solve callback");
    }

    RockBridge::GripRole toGripRole(const hands::HandRole role)
    {
        switch (role) {
        case hands::HandRole::Firing:
            return RockBridge::GripRole::Firing;
        case hands::HandRole::Support:
            return RockBridge::GripRole::Support;
        default:
            return RockBridge::GripRole::None;
        }
    }

    /**
     * ROCK's role of one hand in the bridge's terms; the slot reads NO_WEAPON_SLOT whenever the role is None.
     */
    RockBridge::HandGrip toHandGrip(const hands::HandWeaponRole& role)
    {
        const auto gripRole = toGripRole(role.role);
        return { .weaponSlot = gripRole == RockBridge::GripRole::None ? RockBridge::NO_WEAPON_SLOT : role.weaponSlot, .role = gripRole };
    }
}

namespace ImFl
{
    /**
     * Connect to ROCK's provider API 2 (ROCK 1.0+): discover Core through ROCK.dll's query export, register this mod
     * as an owner, then bind the owner to each interface it reads — Hands (the grip roles, required), Weapon (the
     * game weapon's firing hand) and Input (the physical triggers) — and Animation's callbacks to run onWeaponSolved
     * after ROCK's weapon solve. Each call is refused without the ROCK-issued owner token and the interface's
     * permission. ROCK not being installed is the common case and only logged. An older ROCK refuses the query, and
     * ROCK itself then shows the player which mod needs an update. Call once, on game loaded; the registration is
     * kept for the process lifetime.
     * @return true if connected, with the hand grips readable.
     */
    bool RockBridge::initialize(void (*onWeaponSolved)())
    {
        const auto rockDll = GetModuleHandleA("ROCK.dll");
        const auto query = rockDll ? reinterpret_cast<QueryInterfaceV1>(GetProcAddress(rockDll, kQueryExportName)) : nullptr;
        if (!query) {
            logger::info("ROCK API not available, weapon grip is read from FRIK only");
            return false;
        }

        Client client;
        const auto status = client.connect(query, Version::PROJECT.data());
        if (status != Status::Ok) {
            logger::warn("ROCK connection failed (status: {}), weapon grip is read from FRIK only", static_cast<std::uint32_t>(status));
            return false;
        }
        const char* modVersion = nullptr;
        client.core()->getModVersion(client.owner(), &modVersion);
        logger::info("Registered with ROCK (v{})", modVersion ? modVersion : "unknown");

        g_hands = acquireRockInterface<hands::Api>(query, client, static_cast<std::uint32_t>(hands::Permission::Read), "Hands", "weapon grip is read from FRIK only");
        if (!g_hands) {
            client.close();
            return false;
        }
        g_owner = client.owner();

        constexpr auto triggerConsequence = "the free hand's trigger is unreadable while ROCK fires from the left hand";
        g_weapon = acquireRockInterface<weapon::Api>(query, client, static_cast<std::uint32_t>(weapon::Permission::Read), "Weapon", triggerConsequence);
        g_input = acquireRockInterface<input::Api>(query, client, static_cast<std::uint32_t>(input::Permission::Read), "Input", triggerConsequence);
        const auto* animationApi = acquireRockInterface<animation::Api>(query,
            client,
            static_cast<std::uint32_t>(animation::Permission::Callbacks),
            "Animation",
            "weapon-anchored transforms may lag ROCK's weapon solve");

        g_onWeaponSolved = onWeaponSolved;
        registerWeaponSolvedCallback(animationApi);
        return true;
    }

    /**
     * Which weapon slot each physical hand holds this frame, and by which grip, from ROCK's hand roles. A hand
     * holds at most one grip of one weapon. Both None without ROCK or a held weapon.
     */
    RockBridge::HandGrips RockBridge::getHandGrips()
    {
        hands::Roles roles{};
        if (!g_hands || g_hands->getRoles(g_owner, &roles) != Status::Ok) {
            return {};
        }
        return { .right = toHandGrip(roles.right), .left = toHandGrip(roles.left) };
    }

    /**
     * Is the game's weapon assigned to the physical left hand to fire, the condition of ROCK's trigger remap. The
     * assignment holds while the hand carries the weapon with the firing grip vacated, as the remap does.
     */
    bool RockBridge::isGameWeaponFiredByLeftHand()
    {
        weapon::WeaponState state{};
        return g_weapon && g_weapon->getWeaponState(g_owner, GAME_WEAPON_SLOT, &state) == Status::Ok && state.firingHand == Hand::Left;
    }

    /**
     * Can the physical triggers and the remap condition be read (isPhysicalTriggerHeld(), isGameWeaponFiredByLeftHand()).
     */
    bool RockBridge::canReadPhysicalTriggers()
    {
        return g_input && g_weapon;
    }

    /**
     * Is the physical trigger of the given hand held, from ROCK's raw wand state captured before its remap. Level
     * state only. Empty when unreadable, including while ROCK gates the raw state for a menu.
     */
    std::optional<bool> RockBridge::isPhysicalTriggerHeld(const bool left)
    {
        input::RawWandButtonState raw{};
        if (!g_input || g_input->getRawWandButtonState(g_owner, left ? Hand::Left : Hand::Right, vr::k_EButton_SteamVR_Trigger, &raw) != Status::Ok || !raw.available) {
            return std::nullopt;
        }
        return raw.held != 0;
    }
}
