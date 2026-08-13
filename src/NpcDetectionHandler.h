#pragma once

#include <cstdint>
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
         * Last posted detection event, kept only to feed the debug overlay between ticks.
         */
        struct DebugEventState
        {
            RE::NiPoint3 eventPos;
            RE::NiPoint3 npcPos; // valid only when direct
            int soundLevel = 0;
            bool direct = false;
            uint64_t timeMs = 0;
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
        static void drawDebugOverlay();
        static void drawProbesDebug();

        inline static uint64_t _lastTickTime = 0;
        inline static DebugEventState _debugEvent;
        // Debug-overlay diagnostics from the last tick: why the tick did or didn't post an event, how many
        // NPCs were inside the cone (to tell "nobody in cone" from "in cone but no LOS"), and how many were
        // dropped by the hostile-only filter (to tell "nobody in cone" from "in cone but not worth an
        // event"). The detection levels are diagnostic only — nothing in the tick reads them (docs 3.0) and
        // the native behind them is called only while the overlay is on.
        inline static std::string _debugReason;
        inline static int _debugConeCount = 0;
        inline static int _debugFriendlyCount = 0;
        inline static std::string _debugDetectionLevels;
        // Degrees the last hit NPC was facing away from the player (0 = straight at them), the number behind
        // the instant-spot facing gate.
        inline static float _debugFacingAngle = 0;
        // Every raycast the last tick made (LOS checks + the lit-spot probe), kept because the tick is
        // throttled while the overlay draws every frame.
        inline static std::vector<RayProbe> _debugProbes;
    };
}
