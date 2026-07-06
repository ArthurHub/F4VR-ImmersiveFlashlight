#pragma once

#include <cstdint>

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

        static void runDetectionTick();
        static bool runNpcDirectDetection(const BeamCone& cone);
        static void runLitSpotDetection(const BeamCone& cone);
        static bool getBeamCone(BeamCone& cone);
        static RE::Actor* findNearestLitNpc(const BeamCone& cone);
        static bool isLineOfSightClear(const RE::NiPoint3& from, const RE::NiPoint3& to);
        static bool getBeamTerminationSpot(const BeamCone& cone, RE::NiPoint3& spot);
        static void postDetectionEvent(const RE::NiPoint3& location, int soundLevel);

        inline static uint64_t _lastTickTime = 0;
    };
}
