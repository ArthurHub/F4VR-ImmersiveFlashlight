#include "devbench/FlashlightDevBench.h"

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "FlashlightMod.h"
#include "FlashlightState.h"
#include "NpcDetectionHandler.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "WeaponGripHandler.h"
#include "common/CommonUtils.h"
#include "devbench/DevBench.h"
#include "f4vr/F4VRUtils.h"

namespace ImFl::devbench
{
    namespace
    {
        using json = nlohmann::json;

        /**
         * One frame of the mod's state. Plain values only: a snapshot outlives its frame, and the weapon 3D, the
         * skeleton and the light are rebuilt under it.
         */
        struct FlashlightDevState
        {
            // whether the frame got as far as the flashlight: a player with 3D, and the mod set up
            bool ready = false;

            bool lightOn = false;
            bool previewOverride = false;
            const char* location = "";
            const char* configLocation = "";
            const char* gripStyle = "";
            float fade = 0;
            int radius = 0;
            float fov = 0;
            std::array<int, 3> color{};
            std::string gobo;

            bool flashlightAvailable = false;
            bool gesturesBlockedByMenu = false;
            bool inPowerArmor = false;
            std::string blockingMenu;

            bool twoHanded = false;
            bool weaponInOffhand = false;
            bool carriedByOffhand = false;
            bool offhandHoldingWeapon = false;
            bool primaryHandFree = false;

            bool headAllowed = false;
            bool weaponEquipped = false;
            bool weaponFlashlightAllowed = false;
            bool weaponMeshRequired = false;
            std::string weaponLampNode;

            bool configOpen = false;

            NpcDetectionHandler::PostedEvent npcLastEvent;
        };

        /**
         * One boolean of the state action: where it goes in the JSON, and which member holds it.
         */
        struct Flag
        {
            const char* group;
            const char* name;
            bool FlashlightDevState::*value;
        };

        /**
         * One label (an enum's name) of the state action, reported and evented like a flag.
         */
        struct Label
        {
            const char* group;
            const char* name;
            const char* FlashlightDevState::*value;
        };

        /**
         * Every flag the tool reports, in one list: the state JSON, its description and the transition events are all
         * built from it, so they can't drift apart. Flags of a group stay together.
         */
        constexpr std::array FLAGS = {
            Flag{ "light", "on", &FlashlightDevState::lightOn },
            Flag{ "light", "previewOverride", &FlashlightDevState::previewOverride },
            Flag{ "gating", "flashlightAvailable", &FlashlightDevState::flashlightAvailable },
            Flag{ "gating", "gesturesBlockedByMenu", &FlashlightDevState::gesturesBlockedByMenu },
            Flag{ "gating", "inPowerArmor", &FlashlightDevState::inPowerArmor },
            Flag{ "grip", "twoHanded", &FlashlightDevState::twoHanded },
            Flag{ "grip", "weaponInOffhand", &FlashlightDevState::weaponInOffhand },
            Flag{ "grip", "carriedByOffhand", &FlashlightDevState::carriedByOffhand },
            Flag{ "grip", "offhandHoldingWeapon", &FlashlightDevState::offhandHoldingWeapon },
            Flag{ "grip", "primaryHandFree", &FlashlightDevState::primaryHandFree },
            Flag{ "restrictions", "headAllowed", &FlashlightDevState::headAllowed },
            Flag{ "restrictions", "weaponEquipped", &FlashlightDevState::weaponEquipped },
            Flag{ "restrictions", "weaponFlashlightAllowed", &FlashlightDevState::weaponFlashlightAllowed },
            Flag{ "restrictions", "weaponMeshRequired", &FlashlightDevState::weaponMeshRequired },
            Flag{ "ui", "configOpen", &FlashlightDevState::configOpen },
        };

        constexpr std::array LABELS = {
            Label{ "light", "location", &FlashlightDevState::location },
            Label{ "light", "configLocation", &FlashlightDevState::configLocation },
            Label{ "light", "gripStyle", &FlashlightDevState::gripStyle },
        };

        // A value becomes an event once it has held this many frames, so one that flickers (a grip at its tilt
        // threshold, a menu opening and closing) can't flood the event ring every mod shares in devbench.
        constexpr std::uint32_t STABLE_FRAMES = 3;

        /**
         * The value each flag and label was last reported as, and how many frames a different one has held. Game
         * thread only.
         */
        struct Transitions
        {
            bool primed = false;
            std::array<bool, FLAGS.size()> reportedFlags{};
            std::array<std::uint32_t, FLAGS.size()> pendingFlags{};
            std::array<std::string_view, LABELS.size()> reportedLabels{};
            std::array<std::uint32_t, LABELS.size()> pendingLabels{};
        };

        Transitions g_transitions;

        /**
         * Track one value toward its event: true once a value other than the reported one has held STABLE_FRAMES frames.
         */
        template <typename T>
        bool settled(const T& value, T& reported, std::uint32_t& pendingFrames)
        {
            if (value == reported) {
                pendingFrames = 0;
                return false;
            }
            if (++pendingFrames < STABLE_FRAMES) {
                return false;
            }
            reported = value;
            pendingFrames = 0;
            return true;
        }

        /**
         * Publish <group>.<name> for every flag and label whose new value has held STABLE_FRAMES frames. It rides on
         * the state capture, so it runs only while the tool is armed and costs nothing beyond comparing what was
         * captured. Nothing is published before the flashlight is up.
         */
        void emitTransitions(const FlashlightDevState& state)
        {
            if (!state.ready) {
                return;
            }
            auto& t = g_transitions;
            if (!t.primed) {
                // the first capture after arming is the baseline: nothing has changed yet
                for (std::size_t i = 0; i < FLAGS.size(); ++i) {
                    t.reportedFlags[i] = state.*FLAGS[i].value;
                }
                for (std::size_t i = 0; i < LABELS.size(); ++i) {
                    t.reportedLabels[i] = state.*LABELS[i].value;
                }
                t.primed = true;
                return;
            }
            for (std::size_t i = 0; i < FLAGS.size(); ++i) {
                const bool value = state.*FLAGS[i].value;
                if (settled(value, t.reportedFlags[i], t.pendingFlags[i])) {
                    f4cf::devbench::emit(std::format("{}.{}", FLAGS[i].group, FLAGS[i].name), [&] { return json{ { "value", value }, { "location", state.location } }; });
                }
            }
            for (std::size_t i = 0; i < LABELS.size(); ++i) {
                const std::string_view value = state.*LABELS[i].value;
                if (settled(value, t.reportedLabels[i], t.pendingLabels[i])) {
                    f4cf::devbench::emit(std::format("{}.{}", LABELS[i].group, LABELS[i].name), [&] { return json{ { "value", value }, { "lightOn", state.lightOn } }; });
                }
            }
        }

        /**
         * Game thread, after every onFrameUpdate while the tool is armed.
         */
        void captureState(FlashlightDevState& state)
        {
            const auto player = RE::PlayerCharacter::GetSingleton();
            state.ready = player && player->loadedData && g_imFl.getConfigScreen();
            if (!state.ready) {
                return;
            }

            state.lightOn = Utils::isFlashlightOn();
            state.previewOverride = FlashlightState::isRuntimeLocationOverrideActive();
            state.location = FlashlightState::getFlashlightLocationLabel(FlashlightState::flashlightLocation);
            state.configLocation = FlashlightState::getFlashlightConfigLocationLabel(FlashlightState::getActiveFlashlightConfigLocation());
            state.gripStyle = FlashlightState::getGripStyleLabel(FlashlightState::flashlightGripStyle);
            if (FlashlightState::flashlightFade) {
                state.fade = *FlashlightState::flashlightFade;
                state.radius = *FlashlightState::flashlightRadius;
                state.fov = *FlashlightState::flashlightFov;
                state.color = { *FlashlightState::flashlightColorRed, *FlashlightState::flashlightColorGreen, *FlashlightState::flashlightColorBlue };
                state.gobo = *FlashlightState::flashlightGoboPath;
            }

            state.flashlightAvailable = RestrictionHandler::isFlashlightAvailable();
            state.blockingMenu = Utils::getGestureBlockingMenu();
            state.gesturesBlockedByMenu = !state.blockingMenu.empty();
            state.inPowerArmor = f4vr::isInPowerArmor();

            state.twoHanded = WeaponGripHandler::isTwoHandedGripActive();
            state.weaponInOffhand = WeaponGripHandler::isWeaponInOffhand();
            state.carriedByOffhand = WeaponGripHandler::isWeaponCarriedByOffhand();
            state.offhandHoldingWeapon = WeaponGripHandler::isOffhandHoldingWeapon();
            state.primaryHandFree = WeaponGripHandler::isPrimaryHandFreeOfWeapon();

            state.headAllowed = RestrictionHandler::isHeadFlashlightAllowed();
            state.weaponEquipped = RestrictionHandler::isWeaponEquipped();
            state.weaponFlashlightAllowed = RestrictionHandler::isWeaponFlashlightAllowed();
            state.weaponMeshRequired = RestrictionHandler::isWeaponFlashlightMeshRequired();
            const auto lampNode = RestrictionHandler::getOnWeaponFlashlightMeshNode().first;
            state.weaponLampNode = lampNode ? lampNode->name.c_str() : "";

            state.configOpen = g_imFl.isConfigOpen();
            state.npcLastEvent = NpcDetectionHandler::getLastEvent();

            emitTransitions(state);
        }

        /**
         * Listener thread: reads only its FlashlightDevState (and the clock).
         */
        json stateToJson(const FlashlightDevState& state)
        {
            json out = json::object();
            out["liveness"] = { { "ready", state.ready } };
            if (!state.ready) {
                return out;
            }

            for (const auto& flag : FLAGS) {
                out[flag.group][flag.name] = state.*flag.value;
            }
            for (const auto& label : LABELS) {
                out[label.group][label.name] = state.*label.value;
            }
            out["light"]["beam"] = { { "fade", state.fade }, { "radius", state.radius }, { "fov", state.fov }, { "color", state.color }, { "gobo", state.gobo } };
            out["gating"]["blockingMenu"] = state.blockingMenu.empty() ? json(nullptr) : json(state.blockingMenu);
            out["restrictions"]["weaponLampNode"] = state.weaponLampNode.empty() ? json(nullptr) : json(state.weaponLampNode);

            const auto& event = state.npcLastEvent;
            out["npc"]["lastEvent"] = !event.posted ? json(nullptr)
                                                    : json{
                                                          { "kind", event.direct ? "direct" : "litSpot" },
                                                          { "spotted", event.spotted },
                                                          { "soundLevel", event.soundLevel },
                                                          { "npc", std::format("{:08X}", event.npcFormId) },
                                                          { "ageMs", common::nowMillis() - event.timeMs },
                                                      };
            return out;
        }

        /**
         * What the state keys mean, for the state action's description, with the flags listed from FLAGS.
         */
        std::string stateDescription()
        {
            std::string groups;
            std::string_view group;
            for (const auto& flag : FLAGS) {
                if (flag.group != group) {
                    groups += std::format("{}{} (", group.empty() ? "" : "), ", flag.group);
                    group = flag.group;
                } else {
                    groups += ", ";
                }
                groups += flag.name;
            }
            groups += ")";
            return std::format(
                "flags {}. light also has location (runtime: OnHead|OnPAHead|InOffhand|InPrimaryHand|OnWeapon), configLocation (the chosen one for "
                "the current PA state: OnHead|InOffhand|InPrimaryHand), gripStyle and beam (the active location's fade, radius, fov, color, gobo); "
                "gating.blockingMenu; restrictions.weaponLampNode; npc.lastEvent (the last detection event posted: kind, spotted, soundLevel, npc, "
                "ageMs). liveness adds ready (false = no player 3D yet, and nothing else is reported). Every flag's and label's change is also an "
                "event, <group>.<name> with its value, once the new value has held {} frames. Other events: gesture (a gesture fired: gesture, hand, "
                "lightOn, location), restriction.lightOff (rule)",
                groups,
                STABLE_FRAMES);
        }

        /**
         * Throws unless the flashlight is up: a player with 3D, and the mod set up (onGameLoaded).
         */
        void requireFlashlight()
        {
            const auto player = f4vr::getPlayer();
            if (!g_imFl.getConfigScreen() || !player || !player->loadedData) {
                throw std::runtime_error("the flashlight isn't up: no player 3D yet");
            }
        }

        /**
         * light: switch the configured location and/or turn the light on / off, the way the gestures do (a location
         * switch is saved to the INI as theirs is).
         */
        json runLight(const json& args)
        {
            requireFlashlight();
            const auto location = args.value("location", std::string());
            const auto op = args.value("op", std::string());
            if (location.empty() && op.empty()) {
                throw std::invalid_argument("light: give op (on|off|toggle) and/or location (head|offhand|primaryHand)");
            }
            if (location == "head") {
                FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
            } else if (location == "offhand") {
                FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
            } else if (location == "primaryHand") {
                FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InPrimaryHand);
            } else if (!location.empty()) {
                throw std::invalid_argument(std::format("light: unknown location '{}': head|offhand|primaryHand", location));
            }
            if (op == "on" || (op == "toggle" && !Utils::isFlashlightOn())) {
                Utils::turnFlashlightOn();
            } else if (op == "off" || op == "toggle") {
                Utils::turnFlashlightOff();
            } else if (!op.empty()) {
                throw std::invalid_argument(std::format("light: unknown op '{}': on|off|toggle", op));
            }
            return {
                { "lightOn", Utils::isFlashlightOn() },
                { "location", FlashlightState::getFlashlightLocationLabel(FlashlightState::flashlightLocation) },
                { "configLocation", FlashlightState::getFlashlightConfigLocationLabel(FlashlightState::getActiveFlashlightConfigLocation()) },
            };
        }

        /**
         * ui: open or close the in-game config UI.
         */
        json runUi(const json& args)
        {
            requireFlashlight();
            const auto op = args.value("op", std::string("open"));
            if (op == "open") {
                g_imFl.getConfigScreen()->openConfigMode();
            } else if (op == "close") {
                g_imFl.getConfigScreen()->closeConfigMode();
            } else {
                throw std::invalid_argument(std::format("ui: unknown op '{}': open|close", op));
            }
            return { { "configOpen", g_imFl.isConfigOpen() } };
        }

        /**
         * restrictions: whether each restriction lets the light on the head / weapon, and why - the worn headgear and the
         * Immersive rule list that decided it, the weapon and the flashlight mesh found on it.
         */
        json runRestrictions(const json&)
        {
            requireFlashlight();
            const auto* headgear = RestrictionHandler::getWornHeadgear();
            const auto* lampNode = RestrictionHandler::getOnWeaponFlashlightMeshNode().first;
            return {
                { "headgear",
                    {
                        { "allowed", RestrictionHandler::isHeadFlashlightAllowed() },
                        { "worn", headgear ? json(std::format("{} [{:08X}]", headgear->GetFullName(), headgear->formID)) : json(nullptr) },
                        { "immersiveRule", RestrictionHandler::getHeadgearRuleMatch(headgear) },
                    } },
                { "weapon",
                    {
                        { "allowed", RestrictionHandler::isWeaponFlashlightAllowed() },
                        { "meshRequired", RestrictionHandler::isWeaponFlashlightMeshRequired() },
                        { "weapon", f4vr::isWeaponDrawn() ? json(f4vr::getEquippedWeaponName()) : json(nullptr) },
                        { "lampNode", lampNode ? json(lampNode->name.c_str()) : json(nullptr) },
                    } },
            };
        }

        json arg(const char* type, const char* description)
        {
            return { { "type", type }, { "description", description } };
        }
    }

    void setupDevBenchTool()
    {
        f4cf::devbench::setToolDescription(
            "Immersive Flashlight VR: the VR flashlight - where the light is mounted (head, power-armor head, offhand, primary hand, weapon), each "
            "mount's beam (fade, radius, FOV, color, gobo, shadows), the flashlight stowed on the chest and the gestures that move the light (body "
            "grab, offhand to head, offhand to primary hand, two-handed weapon toggle), the headgear and weapon-lamp restrictions, ROCK / FRIK "
            "weapon grips, NPCs noticing the beam, and the in-game config UI. Advanced INI sections are [ImFl_*] (set/config take section)");

        f4cf::devbench::setStateProvider<FlashlightDevState>(stateDescription(), &captureState, &stateToJson);

        f4cf::devbench::addAction({
            .name = "light",
            .description = "switch the configured location and/or turn the light on/off, the way the gestures do (the location is saved to the INI "
                           "as theirs is); returns lightOn, location, configLocation",
            .arguments = {
                { "op", arg("string", "light: on|off|toggle (omit to only switch location); ui: open (default)|close") },
                { "location", arg("string", "light: switch the configured location first: head|offhand|primaryHand (primaryHand lands on a drawn ranged weapon)") },
            },
            .handler = &runLight,
        });
        f4cf::devbench::addAction({
            .name = "ui",
            .description = "open or close the in-game config UI (op); opening turns the light on, as the FRIK button does",
            .handler = &runUi,
        });
        f4cf::devbench::addAction({
            .name = "restrictions",
            .description = "whether the headgear / weapon restriction allows the light on the head / weapon, and why: the worn headgear and the "
                           "Immersive rule list that decided it, the weapon and its flashlight mesh node",
            .handler = &runRestrictions,
        });
    }
}
