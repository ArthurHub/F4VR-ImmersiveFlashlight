#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace ImFl
{
    /**
     * Makes NPCs notice the flashlight beam — natively, directionally, and line-of-sight gated.
     *
     * Vanilla NPC visual detection only asks "how lit is the player", which a spot light barely raises
     * (the cone points away from the wearer), so shining a beam at an enemy goes unnoticed. On a throttled
     * tick while the light is on, this handler builds the beam cone from the game light node the mod itself
     * positions plus the active beam config (FOV/radius), finds the nearest loaded NPC inside the cone with
     * clear line of sight, and posts a native detection event (the engine primitive behind Papyrus
     * CreateDetectionEvent, owned by the player) near that NPC offset toward the player — so the NPC
     * investigates toward the light source. When the cone touches nobody, a weaker event is placed where the
     * beam terminates on world geometry — but only if some NPC could actually have seen that patch (near it,
     * facing it, with line of sight), because the event is a *sound* the engine broadcasts through walls and
     * a light on the floor should not be heard by an empty room. Events decay engine-side; turning the light
     * off just stops posting.
     *
     * Static like RestrictionHandler: single player, single light, no instance state beyond the tick timer.
     * Design + VR address provenance: docs/tech/npc-light-detection.md.
     */
    class NpcDetectionHandler
    {
    public:
        static void onFrameUpdate();

    private:
        struct BeamCone
        {
            RE::NiPoint3 origin;
            RE::NiPoint3 direction; // normalized world-space beam center
            float range = 0;
            float cosHalfAngle = 1;
        };

        /**
         * A loaded NPC the tick may act on, gathered once and shared by both paths — the direct path needs
         * the ones the beam is pointed at, the lit-spot path the ones standing near where it lands, and the
         * engine radius query behind them is the same call.
         */
        struct Candidate
        {
            // owning, because the list outlives the engine query's scrap array that handed the actors over
            RE::NiPointer<RE::Actor> actor;
            float distToOrigin = 0; // to the chest test point, from the beam origin
            bool inCone = false; // inside the detection cone, so a direct-path candidate
        };

        /**
         * One raycast and what stopped it. `what` is the whole point: a beam that dies in mid-air is only
         * explicable once you can read the hit's collision layer (it was `actorZone` — invisible AI trigger
         * volumes), and nothing about that is visible in-game.
         */
        struct RayProbe
        {
            std::string label; // what the ray was for ("los <npc>" / "lit spot"), also the watch-table key
            RE::NiPoint3 from;
            RE::NiPoint3 to;
            RE::NiPoint3 hitPos; // valid when blocked
            bool blocked = false;
            bool hitActor = false; // the ray stopped on a body rather than on cover
            std::string what; // "<node> [<layer>] at <dist>" when blocked, else why nothing stopped it
            std::string through; // the pass-through volumes crossed on the way, folded into `what` at the end
        };

        /**
         * Everything the debug overlay reads, and the only thing the detection path writes it through. It
         * outlives the tick that wrote it because the tick is throttled (iNpcDetectionIntervalMs) while the
         * overlay draws every frame, so the last tick's answers have to survive until the next one.
         */
        struct DebugState
        {
            /**
             * Last posted detection event, kept so the overlay can keep drawing it for a few ticks.
             */
            struct Event
            {
                RE::NiPoint3 pos;
                RE::NiPoint3 npcPos; // the NPC the event is about: the lit one (direct) or the witness (lit spot)
                int soundLevel = 0;
                bool direct = false;
                uint64_t timeMs = 0;
            };

            Event event;
            std::string reason; // why the last tick did or didn't post an event
            // how many NPCs were inside the cone (to tell "nobody in cone" from "in cone but no LOS"), and how
            // many of them the hostile-only filter dropped (to tell it from "in cone but not worth an event")
            int coneCount = 0;
            int friendlyCount = 0;
            // how many NPCs were near the lit spot and facing it (to tell "nobody could have seen the patch"
            // from "somebody was looking, but a wall was in the way")
            int litSpotLookingCount = 0;
            // the engine's own perception of the player per candidate NPC — diagnostic only: nothing in the
            // tick reads it (docs 3.0) and the native behind it is called only while the overlay is on
            std::string detectionLevels;
            // degrees the last hit NPC was facing away from the player (0 = straight at them), the number
            // behind the instant-spot facing gate
            float facingAngle = 0;
            std::vector<RayProbe> probes; // every raycast the last tick made (LOS checks + the lit-spot probe)

            static bool recording();
            static std::string losLabel(const RE::Actor* npc, std::string_view kind = "los");
            static std::string hitLabel(RE::bhkPickData& pickData, std::uint32_t hitFilter);

            void onTickStart();
            void setReason(std::string text);
            void addReason(const std::string& text);
            void recordNoNpcFound();
            void recordNoLitSpotWitness();
            void recordCandidate(RE::Actor* npc, RE::Actor* player);
            void recordDirectEvent(const RE::Actor* npc, const RE::NiPoint3& eventPos, int soundLevel, bool spotted);
            void recordLitSpotEvent(const RE::NiPoint3& spot, int soundLevel, const RE::Actor* witness);
            void startProbe(RayProbe& probe, const std::string& label, const RE::NiPoint3& from, const RE::NiPoint3& to) const;
            void passedThrough(RayProbe& probe, const std::string& hit) const;
            void draw();
            void drawProbes() const;

            /**
             * Finish a raycast with what ended it and keep the probe for the overlay, returning whether the
             * ray was blocked — so a cast can `return _debug.endProbe(probe, "...")` at every one of its exits
             * and read as its own flow.
             */
            template <typename... Args>
            bool endProbe(RayProbe& probe, const std::format_string<Args...> fmt, Args&&... args)
            {
                if (recording()) {
                    probe.what = std::format(fmt, std::forward<Args>(args)...);
                    if (!probe.through.empty()) {
                        probe.what += std::format(" (through {})", probe.through);
                    }
                    probes.push_back(probe);
                }
                return probe.blocked;
            }
        };

        static void runDetectionTick();
        static void gatherCandidates(const BeamCone& cone, std::vector<Candidate>& candidates);
        static bool isBeamReactingNpc(RE::Actor* npc);
        static bool runNpcDirectDetection(const BeamCone& cone, const std::vector<Candidate>& candidates);
        static void runLitSpotDetection(const BeamCone& cone, const std::vector<Candidate>& candidates);
        static bool getBeamCone(BeamCone& cone);
        static bool tryEscalateToSpotted(RE::Actor* npc, int soundLevel);
        static RE::Actor* findNearestLitNpc(const BeamCone& cone, const std::vector<Candidate>& candidates);
        static RE::Actor* findLitSpotWitness(const RE::NiPoint3& spot, const std::vector<Candidate>& candidates);
        static bool isLineOfSightClear(const RE::NiPoint3& from, const RE::NiPoint3& to, const std::string& label);
        static bool castRay(const RE::NiPoint3& from, const RE::NiPoint3& to, const std::string& label, RayProbe& probe);
        static bool getBeamTerminationSpot(const BeamCone& cone, RE::NiPoint3& spot);
        static void postDetectionEvent(const RE::NiPoint3& location, int soundLevel);

        inline static uint64_t _lastTickTime = 0;
        inline static DebugState _debug;
    };
}
