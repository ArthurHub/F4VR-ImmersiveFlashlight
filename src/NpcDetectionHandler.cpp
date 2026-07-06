#include "NpcDetectionHandler.h"

#include <algorithm>
#include <ranges>
#include <vector>

#include "Config.h"
#include "FlashlightState.h"
#include "Utils.h"
#include "common/CommonUtils.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

using namespace common;

namespace
{
    // Cone/LOS test point above the actor's feet location (rough human chest height, game units).
    constexpr float TARGET_HEIGHT_OFFSET = 110.0f;

    // The LOS target point sits inside the NPC's own collision capsule, so a clear path still reports a
    // hit just short of it; only a hit more than capsule-radius-plus-margin short counts as blocking.
    constexpr float LOS_TARGET_CAPSULE_MARGIN = 60.0f;

    // The posted sound level scales with the beam's strength at the target — distance falloff over the
    // FULL visual radius (not the fNpcDetectionMaxRange eligibility cap) times the beam intensity (fade) —
    // so a strong long-throw hand torch is more alarming at the same distance than a dim short head lamp.
    // Floored so any touch within the cap still registers, and capped so a bright close-up beam can
    // exceed the configured base level without running away.
    constexpr float STRENGTH_FLOOR = 0.25f;
    constexpr float STRENGTH_CAP = 1.5f;

    /**
     * Sound level scaled by the active beam's strength at the given distance: linear falloff over the
     * beam's full visual radius times its fade (intensity), clamped to [STRENGTH_FLOOR, STRENGTH_CAP].
     * Uses the per-location beam config, so the head lamp / hand torch / weapon lamp differ in
     * detectability even though the eligibility range is capped the same for all of them.
     */
    int scaleSoundLevelByBeamStrength(const int baseLevel, const float distance)
    {
        if (!ImFl::FlashlightState::flashlightRadius || !ImFl::FlashlightState::flashlightFade) {
            return baseLevel;
        }
        const auto visualRadius = static_cast<float>(*ImFl::FlashlightState::flashlightRadius);
        const float falloff = visualRadius > 0 ? (std::max)(0.0f, 1.0f - distance / visualRadius) : 0.0f;
        const float strength = std::clamp(falloff * *ImFl::FlashlightState::flashlightFade, STRENGTH_FLOOR, STRENGTH_CAP);
        return static_cast<int>(static_cast<float>(baseLevel) * strength);
    }
}

namespace ImFl
{
    /**
     * Run one detection tick if due: while the light is on and detection is active (not a config-UI preview,
     * not sneak-gated off), find what the beam is touching and post a single player-owned detection event
     * there — near the nearest lit NPC (offset toward the player), or on the beam's lit spot on world geometry
     * when it touches nobody.
     */
    void NpcDetectionHandler::onFrameUpdate()
    {
        if (!g_config.npcDetectionEnabled || !Utils::isFlashlightOn() || FlashlightState::isRuntimeLocationOverrideActive() ||
            (g_config.npcDetectionOnlyWhenSneaking && !f4vr::isPlayerSneaking())) {
            // disabled, config-UI beam preview, or sneak-gated off: nothing to alert
            return;
        }
        if (!isNowTimePassed(_lastTickTime, g_config.npcDetectionIntervalMs)) {
            return;
        }
        _lastTickTime = nowMillis();
        runDetectionTick();
    }

    /**
     * One detection tick: find what the beam is touching and post a single player-owned detection event
     * there — near the nearest lit NPC (offset toward the player) when the direct path is enabled, else on
     * the beam's lit spot on world geometry when the lit-spot path is enabled. The two paths are separately
     * gated (bNpcDetection{Direct,LitSpot}Enabled) so each can be toggled independently.
     */
    void NpcDetectionHandler::runDetectionTick()
    {
        BeamCone cone;
        if (!getBeamCone(cone)) {
            return;
        }

        if (runNpcDirectDetection(cone)) {
            return;
        }

        runLitSpotDetection(cone);
    }

    /**
     * Direct path: if enabled and an NPC is lit in the cone (with line of sight), post a detection event
     * pulled off that NPC toward the player and return true; otherwise return false so the caller falls back
     * to the lit-spot path. The event is pulled toward the player by a fraction of the NPC->player distance
     * (so distant NPCs still get a meaningful nudge toward you, not a token fixed shift), clamped to
     * [DIRECT_EVENT_OFFSET_MIN, DIRECT_EVENT_OFFSET_MAX] and never past the player — so the engine's
     * investigate-the-event-location behavior turns the NPC toward the light source instead of making it
     * search its own feet.
     */
    bool NpcDetectionHandler::runNpcDirectDetection(const BeamCone& cone)
    {
        if (!g_config.npcDetectionDirectEnabled) {
            return false;
        }

        const auto npc = findNearestLitNpc(cone);
        if (!npc) {
            return false;
        }

        constexpr float DIRECT_EVENT_OFFSET_FRACTION = 0.25f;
        constexpr float DIRECT_EVENT_OFFSET_MIN = 150.0f;
        constexpr float DIRECT_EVENT_OFFSET_MAX = 1000.0f;

        const auto player = f4vr::getPlayer();
        const RE::NiPoint3 npcPos = npc->data.location;
        const RE::NiPoint3 toPlayer = player->data.location - npcPos;
        const float toPlayerDist = MatrixUtils::vec3Len(toPlayer);
        const float offset = (std::min)(toPlayerDist, std::clamp(DIRECT_EVENT_OFFSET_FRACTION * toPlayerDist, DIRECT_EVENT_OFFSET_MIN, DIRECT_EVENT_OFFSET_MAX));
        const auto eventPos = toPlayerDist > 1.0f ? npcPos + toPlayer * (offset / toPlayerDist) : npcPos;
        const float beamDist = MatrixUtils::vec3Len(npcPos - cone.origin);
        logger::debug("NpcDetector: NPC {:08X} lit at beam-dist {:.0f}, event ({:.0f},{:.0f},{:.0f}) offset {:.0f} toward player",
            npc->formID,
            beamDist,
            eventPos.x,
            eventPos.y,
            eventPos.z,
            offset);
        postDetectionEvent(eventPos, scaleSoundLevelByBeamStrength(g_config.npcDetectionDirectSoundLevel, beamDist));
        return true;
    }

    /**
     * Lit-spot fallback (the beam touched no NPC directly): post a weaker detection event where the beam
     * center terminates on world geometry, so NPCs next to that bright patch investigate it. A no-op when the
     * lit-spot path is disabled or its sound level is zeroed, or when the beam ends in open air (nothing to
     * notice).
     */
    void NpcDetectionHandler::runLitSpotDetection(const BeamCone& cone)
    {
        if (!g_config.npcDetectionLitSpotEnabled || g_config.npcDetectionLitSpotSoundLevel <= 0) {
            return;
        }

        if (RE::NiPoint3 spot; getBeamTerminationSpot(cone, spot)) {
            const float spotDist = MatrixUtils::vec3Len(spot - cone.origin);
            postDetectionEvent(spot, scaleSoundLevelByBeamStrength(g_config.npcDetectionLitSpotSoundLevel, spotDist));
        }
    }

    /**
     * Build the world-space detection cone from the game light node the mod positions and the active
     * per-location beam config. The detection cone follows the visual beam: same origin/direction
     * (including weapon-lamp rooting), FOV narrowed to the bright core (fNpcDetectionFovMult), and reach
     * capped independently of the visual radius (fNpcDetectionMaxRange).
     */
    bool NpcDetectionHandler::getBeamCone(BeamCone& cone)
    {
        const auto lightNode = f4vr::getFirstChild(f4vr::getPlayerNodes()->HeadLightParentNode);
        if (!lightNode || !FlashlightState::flashlightRadius || !FlashlightState::flashlightFov) {
            return false;
        }

        // Flashlight::adjustFlashlightTransformToHandOrHead() sets the light node's local this frame but does
        // not propagate it to world (the engine only does that later, at render), so its world still holds the
        // stale HeadLightParentNode (head) transform. Push local->world now so the cone reads the real beam.
        f4vr::updateTransforms(lightNode);

        cone.origin = lightNode->world.translate;
        cone.direction = MatrixUtils::vec3Norm(lightNode->world.rotate.Transpose() * RE::NiPoint3(1, 0, 0));
        cone.range = (std::min)(static_cast<float>(*FlashlightState::flashlightRadius), g_config.npcDetectionMaxRange);
        const float halfAngleDeg = std::clamp(*FlashlightState::flashlightFov * 0.5f * g_config.npcDetectionFovMult, 1.0f, 89.0f);
        cone.cosHalfAngle = std::cos(MatrixUtils::degreesToRads(halfAngleDeg));

        logger::trace("Beam cone: origin=({:.0f},{:.0f},{:.0f}) dir=({:.2f},{:.2f},{:.2f}) range={:.0f} halfAngle={:.0f}",
            cone.origin.x,
            cone.origin.y,
            cone.origin.z,
            cone.direction.x,
            cone.direction.y,
            cone.direction.z,
            cone.range,
            halfAngleDeg);
        return cone.range > 0;
    }

    /**
     * The nearest loaded NPC inside the beam cone with clear line of sight from the beam origin, or null.
     * Candidates come from the engine's own radius query; the in-cone test is a dot product against the
     * cone's cosine at chest height; at most MAX_LOS_CHECKS_PER_TICK candidates are raycast, nearest first.
     */
    RE::Actor* NpcDetectionHandler::findNearestLitNpc(const BeamCone& cone)
    {
        const auto player = f4vr::getPlayer();
        if (!player) {
            return nullptr;
        }

        RE::BSScrapArray<RE::NiPointer<RE::Actor>> nearby;
        f4vr::getActorsWithinRangeOfPoint(cone.origin, cone.range, nearby);

        std::vector<std::pair<float, RE::Actor*>> inCone;
        for (const auto& handle : nearby) {
            const auto npc = handle.get();
            if (!npc || npc == player || npc->IsDead(true)) {
                continue;
            }
            if (g_config.npcDetectionIgnoreCompanions && RE::IsPlayerTeammate(*npc)) {
                continue;
            }
            const auto target = npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET);
            const auto toNpc = target - cone.origin;
            const float dist = MatrixUtils::vec3Len(toNpc);
            if (dist < 1.0f || dist > cone.range) {
                continue;
            }
            if (MatrixUtils::vec3Dot(toNpc * (1.0f / dist), cone.direction) < cone.cosHalfAngle) {
                continue;
            }
            inCone.emplace_back(dist, npc);
        }

        // Physics raycasts per tick spent finding a visible in-cone NPC (nearest first).
        constexpr int MAX_LOS_CHECKS_PER_TICK = 3;

        std::ranges::sort(inCone, {}, &std::pair<float, RE::Actor*>::first);
        int losChecks = 0;
        for (const auto& npc : inCone | std::views::values) {
            if (losChecks++ >= MAX_LOS_CHECKS_PER_TICK) {
                break;
            }
            if (isLineOfSightClear(cone.origin, npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET))) {
                return npc;
            }
        }
        return nullptr;
    }

    /**
     * Physics raycast from the beam origin to the NPC test point, clear when nothing blocks meaningfully
     * short of the target (the target point is inside the NPC's own capsule — see LOS_TARGET_CAPSULE_MARGIN).
     * Fails open when the raycast itself is unavailable, matching f4vr::isMovementSafe.
     */
    bool NpcDetectionHandler::isLineOfSightClear(const RE::NiPoint3& from, const RE::NiPoint3& to)
    {
        const auto player = f4vr::getPlayer();
        const auto projectile = f4vr::getAnyProjectile();
        if (!player || !projectile) {
            return true;
        }

        RE::bhkPickData pickData;
        pickData.SetStartEnd(from, to);
        pickData.collisionFilter = player->GetCollisionFilter();
        if (!RE::CombatUtilities::CalculateProjectileLOS(player, projectile, pickData)) {
            return true;
        }
        if (!pickData.HasHit()) {
            return true;
        }
        const float dist = MatrixUtils::vec3Len(to - from);
        return pickData.GetHitFraction() * dist >= dist - LOS_TARGET_CAPSULE_MARGIN;
    }

    /**
     * Where the beam center terminates on world geometry within the detection range, false when it ends
     * in open air (nothing for anyone to notice).
     */
    bool NpcDetectionHandler::getBeamTerminationSpot(const BeamCone& cone, RE::NiPoint3& spot)
    {
        const auto player = f4vr::getPlayer();
        const auto projectile = f4vr::getAnyProjectile();
        if (!player || !projectile) {
            return false;
        }

        RE::bhkPickData pickData;
        pickData.SetStartEnd(cone.origin, cone.origin + cone.direction * cone.range);
        pickData.collisionFilter = player->GetCollisionFilter();
        if (!RE::CombatUtilities::CalculateProjectileLOS(player, projectile, pickData) || !pickData.HasHit()) {
            return false;
        }
        spot = cone.origin + cone.direction * (cone.range * pickData.GetHitFraction());
        return true;
    }

    /**
     * Post a player-owned detection event at the location — the native behind Papyrus CreateDetectionEvent,
     * so nearby NPCs investigate the location and blame attaches to the player. The engine keeps one such
     * event per owner and decays it on its own.
     */
    void NpcDetectionHandler::postDetectionEvent(const RE::NiPoint3& location, const int soundLevel)
    {
        const auto player = f4vr::getPlayer();
        if (!player || !player->currentProcess || soundLevel <= 0) {
            return;
        }
        player->currentProcess->SetActorsDetectionEvent(player, location, soundLevel, nullptr);
        logger::debug("NpcDetector: Posted flashlight detection event at ({:.0f},{:.0f},{:.0f}) level={}", location.x, location.y, location.z, soundLevel);
    }
}
