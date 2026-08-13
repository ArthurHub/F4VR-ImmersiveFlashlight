#pragma once

#include <cstdint>
#include <format>
#include <string>
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
     * beam terminates on world geometry, so NPCs near the lit patch react to it. Events decay engine-side;
     * turning the light off just stops posting.
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
                RE::NiPoint3 npcPos; // valid only when direct
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
            // the engine's own perception of the player per candidate NPC — diagnostic only: nothing in the
            // tick reads it (docs 3.0) and the native behind it is called only while the overlay is on
            std::string detectionLevels;
            // degrees the last hit NPC was facing away from the player (0 = straight at them), the number
            // behind the instant-spot facing gate
            float facingAngle = 0;
            std::vector<RayProbe> probes; // every raycast the last tick made (LOS checks + the lit-spot probe)

            static bool recording();
            static std::string losLabel(const RE::Actor* npc);
            static std::string hitLabel(RE::bhkPickData& pickData, std::uint32_t hitFilter);

            void onTickStart();
            void setReason(std::string text);
            void addReason(const std::string& text);
            void recordNoNpcFound();
            void recordCandidate(RE::Actor* npc, RE::Actor* player);
            void recordDirectEvent(const RE::Actor* npc, const RE::NiPoint3& eventPos, int soundLevel, bool spotted);
            void recordLitSpotEvent(const RE::NiPoint3& spot, int soundLevel);
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
        static bool runNpcDirectDetection(const BeamCone& cone);
        static void runLitSpotDetection(const BeamCone& cone);
        static bool getBeamCone(BeamCone& cone);
        static bool tryEscalateToSpotted(RE::Actor* npc, int soundLevel);
        static RE::Actor* findNearestLitNpc(const BeamCone& cone);
        static bool isLineOfSightClear(const RE::NiPoint3& from, const RE::NiPoint3& to, const std::string& label);
        static bool castRay(const RE::NiPoint3& from, const RE::NiPoint3& to, const std::string& label, RayProbe& probe);
        static bool getBeamTerminationSpot(const BeamCone& cone, RE::NiPoint3& spot);
        static void postDetectionEvent(const RE::NiPoint3& location, int soundLevel);

        inline static uint64_t _lastTickTime = 0;
        inline static DebugState _debug;
    };
}
