#include "NpcDetectionHandler.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>
#include <string>
#include <vector>

#include "Config.h"
#include "FlashlightState.h"
#include "Utils.h"
#include "common/CommonUtils.h"
#include "common/MatrixUtils.h"
#include "debug/DebugDraw.h"
#include "f4vr/CollisionLayers.h"
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

    // Half-angle off the NPC's own heading within which a point counts as being in front of it — 90 is the
    // whole forward hemisphere, which is as loose as "looking at it" can be without becoming "facing away
    // from it". Used for the player, for the instant-spot escalation, and for the lit patch, for the
    // lit-spot witness test.
    constexpr float SPOTTED_FACING_DEGREES = 90.0f;

    // How close an NPC has to be to the lit patch to count as able to notice it, as a fraction of the beam's
    // own eligibility reach (fNpcDetectionMaxRange). A bright spot on a wall is a far weaker cue than the
    // beam in your eyes, so it carries a fraction of the distance; the shorter reach also keeps the witness
    // scan and its raycasts cheap.
    constexpr float LIT_SPOT_WITNESS_RANGE_MULT = 0.5f;

    // The witness ray starts on the very surface the beam painted, so on uneven ground — a slope, rubble, a
    // doorstep — that surface shadows its own ray and every witness reads as blocked by nothing. Lifting the
    // origin clears the local bumps without changing what the ray asks. The event itself still goes on the
    // spot; only the sight-line test starts above it.
    constexpr float LIT_SPOT_LOS_LIFT = 60.0f;

    /**
     * How far from the lit patch an NPC may stand and still be a candidate for having noticed it.
     */
    float litSpotWitnessRange()
    {
        return ImFl::g_config.npcDetectionMaxRange * LIT_SPOT_WITNESS_RANGE_MULT;
    }

    // The posted sound level scales with the beam's strength at the target — distance falloff over the
    // FULL visual radius (not the fNpcDetectionMaxRange eligibility cap) times the beam intensity (fade) —
    // so a strong long-throw hand torch is more alarming at the same distance than a dim short head lamp.
    // Floored so any touch within the cap still registers, and capped so a bright close-up beam can
    // exceed the configured base level without running away.
    constexpr float STRENGTH_FLOOR = 0.25f;
    constexpr float STRENGTH_CAP = 1.5f;

    // Collision layers the beam passes through instead of terminating on: invisible gameplay volumes and
    // see-through geometry, none of which can stop light. kActorZone is the one that forced this — those
    // volumes exist to catch character controllers, so they stop any ray carrying a character-controller-ish
    // filter, and the beam died in mid-air. The rest are the same kind of thing (triggers, portals, acoustic
    // spaces, camera/door detection boxes, stair and avoid helpers) plus glass, which you can see through.
    constexpr std::uint64_t PASS_THROUGH_LAYERS = f4vr::collisionLayerMask({
        RE::COL_LAYER::kTransparent,
        RE::COL_LAYER::kTransparentSmall,
        RE::COL_LAYER::kTransparentSmallAnim,
        RE::COL_LAYER::kTrigger,
        RE::COL_LAYER::kNonCollidable,
        RE::COL_LAYER::kCloudTrap,
        RE::COL_LAYER::kGasTrap,
        RE::COL_LAYER::kPortal,
        RE::COL_LAYER::kAcousticSpace,
        RE::COL_LAYER::kActorZone,
        RE::COL_LAYER::kProjectileZone,
        RE::COL_LAYER::kInvisibleWall,
        RE::COL_LAYER::kStairHelper,
        RE::COL_LAYER::kAvoidBox,
        RE::COL_LAYER::kCameraSphere,
        RE::COL_LAYER::kDoorDetection,
    });

    // The layers an actor's own collision lives on. A ray stopping on one of these reached a body rather
    // than cover — for the LOS test that's the good outcome (the target point sits inside that capsule, see
    // LOS_TARGET_CAPSULE_MARGIN), so the debug overlay draws it in its own color.
    constexpr std::uint64_t ACTOR_LAYERS = f4vr::collisionLayerMask({
        RE::COL_LAYER::kCharController,
        RE::COL_LAYER::kBiped,
        RE::COL_LAYER::kBipedNoCC,
        RE::COL_LAYER::kDeadBip,
    });

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
        _debug.draw();
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
     *
     * Both paths answer a question about the NPCs around the beam, so the actors are gathered once here and
     * handed to each in turn rather than queried and walked twice per tick.
     */
    void NpcDetectionHandler::runDetectionTick()
    {
        _debug.onTickStart();
        BeamCone cone;
        if (!getBeamCone(cone)) {
            _debug.setReason("no beam cone");
            return;
        }

        std::vector<Candidate> candidates;
        gatherCandidates(cone, candidates);

        if (runNpcDirectDetection(cone, candidates)) {
            return;
        }

        runLitSpotDetection(cone, candidates);
    }

    /**
     * The loaded NPCs this tick may act on, from the engine's own radius query, filtered down by the checks
     * that cost nothing (alive, not the player, not a teammate) and tagged with whether each one stands
     * inside the detection cone — which is what splits the direct path's candidates from the lit-spot path's.
     *
     * The query reaches past the cone by the lit-spot witness range: the lit patch can sit a full cone range
     * out, and an NPC standing next to it is that much further from the beam origin again, so a cone-sized
     * query would systematically miss exactly the witnesses the lit-spot path is looking for.
     *
     * Deliberately does NOT apply the hostile-only filter — that one costs an engine call per actor, and both
     * paths narrow their candidates down to a handful first (see isBeamReactingNpc).
     */
    void NpcDetectionHandler::gatherCandidates(const BeamCone& cone, std::vector<Candidate>& candidates)
    {
        const auto player = f4vr::getPlayer();
        if (!player) {
            return;
        }

        RE::BSScrapArray<RE::NiPointer<RE::Actor>> nearby;
        f4vr::getActorsWithinRangeOfPoint(cone.origin, cone.range + litSpotWitnessRange(), nearby);

        for (const auto& handle : nearby) {
            const auto npc = handle.get();
            if (!npc || npc == player || npc->IsDead(true)) {
                continue;
            }
            // Companions/teammates already know where the player is - no point investigating the beam.
            if (RE::IsPlayerTeammate(*npc)) {
                continue;
            }
            const auto target = npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET);
            const auto toNpc = target - cone.origin;
            const float dist = MatrixUtils::vec3Len(toNpc);
            if (dist < 1.0f) {
                continue;
            }
            const bool inCone = dist <= cone.range && MatrixUtils::vec3Dot(toNpc * (1.0f / dist), cone.direction) >= cone.cosHalfAngle;
            candidates.push_back({ .actor = handle, .distToOrigin = dist, .inCone = inCone });
        }
    }

    /**
     * Whether this NPC would act on the beam at all, i.e. survives the hostile-only filter
     * (bNpcDetectionOnlyHostileNpcs). The engine call inside is why both paths call this last, once their
     * cheap geometric filters have cut the list down to the few actors the beam actually reaches.
     */
    bool NpcDetectionHandler::isBeamReactingNpc(RE::Actor* npc)
    {
        if (!g_config.npcDetectionOnlyHostileNpcs) {
            return true;
        }
        const auto player = f4vr::getPlayer();
        return player && npc->GetHostileToActor(player);
    }

    /**
     * Direct path: when enabled and an NPC is lit in the cone (with line of sight), post a detection event and
     * return true; otherwise false, so the caller falls back to the lit-spot path. The event sits between the
     * NPC and the player — the engine's investigate-the-location behavior then turns them toward the light
     * instead of their own feet — or on the player at point blank / on a hit counting as being seen.
     */
    bool NpcDetectionHandler::runNpcDirectDetection(const BeamCone& cone, const std::vector<Candidate>& candidates)
    {
        if (!g_config.npcDetectionDirectEnabled) {
            _debug.setReason("direct off");
            return false;
        }

        const auto npc = findNearestLitNpc(cone, candidates);
        if (!npc) {
            _debug.recordNoNpcFound();
            return false;
        }

        constexpr float DIRECT_EVENT_OFFSET_FRACTION = 0.3f;
        constexpr float DIRECT_EVENT_OFFSET_MIN = 200.0f;
        constexpr float DIRECT_EVENT_OFFSET_MAX = 1000.0f;

        const auto player = f4vr::getPlayer();
        const RE::NiPoint3 npcPos = npc->data.location;
        const RE::NiPoint3 toPlayer = player->data.location - npcPos;
        const float toPlayerDist = MatrixUtils::vec3Len(toPlayer);
        const float beamDist = MatrixUtils::vec3Len(npcPos - cone.origin);
        const int soundLevel = scaleSoundLevelByBeamStrength(g_config.npcDetectionDirectSoundLevel, beamDist);

        // The event goes on the PLAYER rather than near the NPC when the beam counts as having been seen, or at
        // point blank - inside DIRECT_EVENT_OFFSET_MIN the offset below would reach the player anyway.
        const bool spotted = tryEscalateToSpotted(npc, soundLevel);
        const bool onPlayer = spotted || toPlayerDist <= DIRECT_EVENT_OFFSET_MIN;
        const float offset = std::clamp(DIRECT_EVENT_OFFSET_FRACTION * toPlayerDist, DIRECT_EVENT_OFFSET_MIN, DIRECT_EVENT_OFFSET_MAX);
        const auto eventPos = onPlayer ? player->data.location : npcPos + toPlayer * (offset / toPlayerDist);
        // the event position itself is logged by postDetectionEvent below, so this only adds who and how far
        logger::sampleDebug(3000,
            "NpcDetector: NPC {:08X} lit at beam-dist {:.0f}, event {}",
            npc->formID,
            beamDist,
            onPlayer ? (spotted ? "ON the player (spotted)" : "ON the player (point blank)") : std::format("offset {:.0f} toward player", offset));
        postDetectionEvent(eventPos, soundLevel);
        _debug.recordDirectEvent(npc, eventPos, soundLevel, spotted);
        return true;
    }

    /**
     * Whether this direct hit counts as the NPC having been *spotted* by the beam rather than merely alerted:
     * the scaled sound level reached iNpcDetectionSpottedEventLevel (which, since the level scales with beam
     * strength at the target, means a bright beam at close range), the NPC is hostile, and it is facing the
     * player. A light in the face from a few meters away is seen, and the caller answers that by putting the
     * detection event on the player instead of off to one side.
     */
    bool NpcDetectionHandler::tryEscalateToSpotted(RE::Actor* npc, const int soundLevel)
    {
        const auto player = f4vr::getPlayer();
        if (!player) {
            return false;
        }
        // Measured first and unconditionally so the overlay can report it even on the ticks that don't
        // escalate - "how far off is it looking" is the number to read when tuning the facing gate.
        const float facingAngle = f4vr::getActorFacingAngleTo(npc, player->data.location);
        _debug.facingAngle = facingAngle;
        if (soundLevel < g_config.npcDetectionSpottedEventLevel) {
            return false;
        }
        if (std::abs(facingAngle) > SPOTTED_FACING_DEGREES || !npc->GetHostileToActor(player)) {
            return false;
        }
        logger::sampleDebug(3000, "NpcDetector: NPC {:08X} spotted the player at level {}, facing {:.0f} deg off", npc->formID, soundLevel, facingAngle);
        return true;
    }

    /**
     * Lit-spot fallback (the beam touched no NPC directly): post a weaker detection event where the beam
     * center terminates on world geometry, so an NPC next to that bright patch investigates it. A no-op when
     * the lit-spot path is disabled or its sound level is zeroed, when the beam ends in open air (nothing to
     * notice), or when nobody could have seen the patch.
     *
     * That last gate is the point of the path's shape: the event is a *sound*, which the engine broadcasts to
     * every actor in range without a facing or line-of-sight test of its own, so posting one wherever the
     * beam happens to land has a light on the floor pulling in NPCs behind walls and NPCs looking the other
     * way. Requiring a witness first (findLitSpotWitness) makes the event mean "somebody saw your light" —
     * their reaction can still alert others through the wall, which is the engine's own behavior and fine.
     */
    void NpcDetectionHandler::runLitSpotDetection(const BeamCone& cone, const std::vector<Candidate>& candidates)
    {
        if (!g_config.npcDetectionLitSpotEnabled || g_config.npcDetectionLitSpotSoundLevel <= 0) {
            _debug.addReason("lit-spot off");
            return;
        }
        // no one within reach of wherever the beam lands: skip the termination raycast too, since no spot
        // could produce a witness anyway (the common case of walking a lit corridor alone)
        if (candidates.empty()) {
            _debug.addReason("lit spot: nobody nearby");
            return;
        }

        RE::NiPoint3 spot;
        if (!getBeamTerminationSpot(cone, spot)) {
            _debug.addReason("no beam termination spot found");
            return;
        }

        const auto witness = findLitSpotWitness(spot, candidates);
        if (!witness) {
            _debug.recordNoLitSpotWitness();
            return;
        }

        const float spotDist = MatrixUtils::vec3Len(spot - cone.origin);
        const int soundLevel = scaleSoundLevelByBeamStrength(g_config.npcDetectionLitSpotSoundLevel, spotDist);
        logger::sampleDebug(3000, "NpcDetector: lit spot seen by NPC {:08X}, event at beam-dist {:.0f}", witness->formID, spotDist);
        postDetectionEvent(spot, soundLevel);
        _debug.recordLitSpotEvent(spot, soundLevel, witness);
    }

    /**
     * The nearest NPC that could actually have noticed the lit patch, or null: standing within
     * litSpotWitnessRange() of the spot, facing it (SPOTTED_FACING_DEGREES — the same "looking at it"
     * half-angle the instant-spot escalation uses on the player), reacting to the beam at all
     * (isBeamReactingNpc), and with clear line of sight to it. Nearest first, at most
     * MAX_LIT_SPOT_LOS_CHECKS_PER_TICK raycasts, mirroring the direct path's budget.
     *
     * The ray runs spot -> NPC rather than the other way so it starts on the lit surface instead of inside
     * the NPC's own collision capsule (which it would hit immediately), which also means the target point is
     * inside that capsule exactly as the direct path's rays expect — so isLineOfSightClear's capsule margin
     * reads the same here. It starts LIT_SPOT_LOS_LIFT above that surface so the ground the beam is painting
     * doesn't shadow its own ray.
     */
    RE::Actor* NpcDetectionHandler::findLitSpotWitness(const RE::NiPoint3& spot, const std::vector<Candidate>& candidates)
    {
        const float range = litSpotWitnessRange();

        std::vector<std::pair<float, RE::Actor*>> looking;
        for (const auto& candidate : candidates) {
            const auto target = candidate.actor->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET);
            const float dist = MatrixUtils::vec3Len(target - spot);
            if (dist > range) {
                continue;
            }
            if (!f4vr::isActorFacing(candidate.actor.get(), spot, SPOTTED_FACING_DEGREES)) {
                continue;
            }
            // the only engine call in the filter, so it runs last (see isBeamReactingNpc)
            if (!isBeamReactingNpc(candidate.actor.get())) {
                continue;
            }
            looking.emplace_back(dist, candidate.actor.get());
        }
        _debug.litSpotLookingCount = static_cast<int>(looking.size());

        // Physics raycasts per tick spent finding an NPC that can see the lit spot (nearest first).
        constexpr int MAX_LIT_SPOT_LOS_CHECKS_PER_TICK = 2;

        const auto rayFrom = spot + RE::NiPoint3(0, 0, LIT_SPOT_LOS_LIFT);
        std::ranges::sort(looking, {}, &std::pair<float, RE::Actor*>::first);
        int losChecks = 0;
        for (const auto& npc : looking | std::views::values) {
            if (losChecks++ >= MAX_LIT_SPOT_LOS_CHECKS_PER_TICK) {
                break;
            }
            if (isLineOfSightClear(rayFrom, npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET), DebugState::losLabel(npc, "spot los"))) {
                return npc;
            }
        }
        return nullptr;
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
     * The nearest NPC inside the beam cone with clear line of sight from the beam origin, or null. The
     * in-cone split is already made by gatherCandidates (a dot product against the cone's cosine at chest
     * height); at most MAX_LOS_CHECKS_PER_TICK of them are raycast, nearest first. Non-hostile actors are
     * dropped here when bNpcDetectionOnlyHostileNpcs is on, so the raycast budget isn't spent on a crowd
     * that wouldn't act on the beam anyway.
     */
    RE::Actor* NpcDetectionHandler::findNearestLitNpc(const BeamCone& cone, const std::vector<Candidate>& candidates)
    {
        const auto player = f4vr::getPlayer();
        if (!player) {
            return nullptr;
        }

        std::vector<std::pair<float, RE::Actor*>> inCone;
        for (const auto& candidate : candidates) {
            if (!candidate.inCone) {
                continue;
            }
            // The only engine call in the filter, so it runs last: just the handful of actors the beam
            // actually touches are worth asking about.
            if (!isBeamReactingNpc(candidate.actor.get())) {
                _debug.friendlyCount++;
                continue;
            }
            _debug.recordCandidate(candidate.actor.get(), player);
            inCone.emplace_back(candidate.distToOrigin, candidate.actor.get());
        }
        _debug.coneCount = static_cast<int>(inCone.size());

        // Physics raycasts per tick spent finding a visible in-cone NPC (nearest first).
        constexpr int MAX_LOS_CHECKS_PER_TICK = 3;

        std::ranges::sort(inCone, {}, &std::pair<float, RE::Actor*>::first);
        int losChecks = 0;
        for (const auto& npc : inCone | std::views::values) {
            if (losChecks++ >= MAX_LOS_CHECKS_PER_TICK) {
                break;
            }
            if (isLineOfSightClear(cone.origin, npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET), DebugState::losLabel(npc))) {
                return npc;
            }
        }
        return nullptr;
    }

    /**
     * Physics raycast from the beam origin to the NPC test point, clear when nothing blocks meaningfully
     * short of the target (the target point is inside the NPC's own capsule — see LOS_TARGET_CAPSULE_MARGIN).
     * Fails open when the raycast itself is unavailable (no physics world mid-cell-transition).
     */
    bool NpcDetectionHandler::isLineOfSightClear(const RE::NiPoint3& from, const RE::NiPoint3& to, const std::string& label)
    {
        RayProbe probe;
        if (!castRay(from, to, label, probe)) {
            return true;
        }
        const float dist = MatrixUtils::vec3Len(to - from);
        return MatrixUtils::vec3Len(probe.hitPos - from) >= dist - LOS_TARGET_CAPSULE_MARGIN;
    }

    /**
     * Where the beam center terminates on world geometry within the detection range, false when it ends
     * in open air (nothing for anyone to notice).
     */
    bool NpcDetectionHandler::getBeamTerminationSpot(const BeamCone& cone, RE::NiPoint3& spot)
    {
        RayProbe probe;
        if (!castRay(cone.origin, cone.origin + cone.direction * cone.range, "lit spot", probe)) {
            return false;
        }
        spot = probe.hitPos;
        return true;
    }

    /**
     * The raycast behind both LOS and the lit spot: from -> to in game space (bhkPickData converts to Havok
     * units), against the physics world of the player's cell, filtered by Config::npcDetectionLosCollisionFilter.
     * Returns whether something solid blocked it (then probe.hitPos is where), and records what it hit for the
     * debug overlay.
     *
     * Hits on layers light passes through anyway (PASS_THROUGH_LAYERS) don't end the ray: the cast resumes
     * just past them, so the beam terminates on real geometry instead of on an invisible trigger box. Running
     * out of retries means the beam is threading a stack of volumes, which is open air, not cover.
     */
    bool NpcDetectionHandler::castRay(const RE::NiPoint3& from, const RE::NiPoint3& to, const std::string& label, RayProbe& probe)
    {
        // Every exit reports what ended the ray as it returns whether the ray was blocked; the message is
        // kept only while the overlay is on, and the callers read just the result / probe.hitPos.
        _debug.startProbe(probe, label, from, to);

        const auto player = f4vr::getPlayer();
        const auto cell = player ? player->parentCell : nullptr;
        const auto world = cell ? cell->GetbhkWorld() : nullptr;
        if (!world) {
            return _debug.endProbe(probe, "no physics world");
        }
        const float rayLen = MatrixUtils::vec3Len(to - from);
        if (rayLen < 1.0f) {
            return _debug.endProbe(probe, "degenerate ray");
        }
        const auto direction = (to - from) * (1.0f / rayLen);

        constexpr int MAX_PASS_THROUGH_HITS = 2;

        auto start = from;
        for (int pass = 0; pass <= MAX_PASS_THROUGH_HITS; pass++) {
            RE::bhkPickData pickData;
            pickData.SetStartEnd(start, to);
            pickData.collisionFilter.filter = g_config.npcDetectionLosCollisionFilter;

            if (!world->PickObject(pickData) || !pickData.HasHit()) {
                return _debug.endProbe(probe, "clear over {:.0f}", rayLen);
            }

            // hit distance along the ORIGINAL ray, so hitPos stays comparable across retries
            const float hitDist = MatrixUtils::vec3Len(start - from) + pickData.GetHitFraction() * MatrixUtils::vec3Len(to - start);
            const auto hitFilter = pickData.result.hitBodyInfo.m_shapeCollisionFilterInfo.storage;
            const auto hit = DebugState::hitLabel(pickData, hitFilter);

            if (f4vr::isCollisionLayerInMask(hitFilter, PASS_THROUGH_LAYERS)) {
                constexpr float PASS_THROUGH_STEP = 2.0f;
                if (pass >= MAX_PASS_THROUGH_HITS) {
                    return _debug.endProbe(probe, "pass-through limit");
                }
                _debug.passedThrough(probe, hit);
                if (hitDist + PASS_THROUGH_STEP >= rayLen) {
                    return _debug.endProbe(probe, "nothing beyond"); // the volume sat at the very end of the ray
                }
                start = from + direction * (hitDist + PASS_THROUGH_STEP);
                continue;
            }

            probe.blocked = true;
            probe.hitActor = f4vr::isCollisionLayerInMask(hitFilter, ACTOR_LAYERS);
            probe.hitPos = from + direction * hitDist;
            return _debug.endProbe(probe, "{} at {:.0f}", hit, hitDist);
        }
        return _debug.endProbe(probe, "pass-through limit");
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
        logger::sampleDebug(3000, "NpcDetector: Posted flashlight detection event at ({:.0f},{:.0f},{:.0f}) level={}", location.x, location.y, location.z, soundLevel);
    }

    // ------------------------------------------------------------------------------------------------
    // Debug state and overlay. Nothing above this line does more than call into it, and nothing below it
    // affects the detection outcome — see NpcDetectionHandler::DebugState.
    // ------------------------------------------------------------------------------------------------

    /**
     * Whether the debug overlay is on, and with it this whole struct. Every method that formats a string or
     * makes a diagnostic engine call checks it first: with the overlay off none of this is ever read.
     */
    bool NpcDetectionHandler::DebugState::recording()
    {
        return g_config.debug.drawEnabled;
    }

    /**
     * The watch-table key / in-world label for an NPC's line-of-sight ray, empty when not recording (the
     * label is threaded down into castRay purely so the probe can be identified in the overlay). `kind`
     * separates the two rays that can be cast at the same NPC in a tick: the direct one from the beam
     * origin, and the lit-spot one from the patch on the wall.
     */
    std::string NpcDetectionHandler::DebugState::losLabel(const RE::Actor* npc, const std::string_view kind)
    {
        return recording() ? std::format("{} {:08X}", kind, npc->formID) : std::string();
    }

    /**
     * "<node> [<layer>]" for what a raycast hit — the collision layer being the part that matters, since a
     * beam dying in mid-air is only explicable once you can see it stopped on an invisible volume. The scene
     * node lookup is an engine call, so it happens only while recording.
     */
    std::string NpcDetectionHandler::DebugState::hitLabel(RE::bhkPickData& pickData, const std::uint32_t hitFilter)
    {
        if (!recording()) {
            return {};
        }
        const auto node = pickData.GetNiAVObject();
        return std::format("{} [{}]", node && !node->name.empty() ? node->name.c_str() : "?", f4vr::getCollisionLayerName(hitFilter));
    }

    /**
     * Drop the previous tick's answers, so a field left unwritten reads as "this tick didn't get that far"
     * rather than as a stale answer from an older one. `facingAngle` is deliberately kept: it is the last
     * measured value, and 0 would read as "looking straight at the player".
     */
    void NpcDetectionHandler::DebugState::onTickStart()
    {
        reason.clear();
        coneCount = 0;
        friendlyCount = 0;
        litSpotLookingCount = 0;
        detectionLevels.clear();
        probes.clear();
    }

    /**
     * Why this tick did what it did, replacing whatever an earlier stage of the tick had to say.
     */
    void NpcDetectionHandler::DebugState::setReason(std::string text)
    {
        if (recording()) {
            reason = std::move(text);
        }
    }

    /**
     * Same, but keeping what an earlier stage already reported — the lit-spot path runs after the direct
     * path missed, and "why no NPC" plus "why no lit spot" together are the answer to "why nothing happened".
     */
    void NpcDetectionHandler::DebugState::addReason(const std::string& text)
    {
        if (recording()) {
            reason = reason.empty() ? text : reason + " - " + text;
        }
    }

    /**
     * Why the direct path found no NPC to light up: the counters separate "nobody was in the cone" from
     * "somebody was, but behind cover" and from "somebody was, but the hostile-only filter dropped them".
     */
    void NpcDetectionHandler::DebugState::recordNoNpcFound()
    {
        if (!recording()) {
            return;
        }
        if (coneCount > 0) {
            setReason(std::format("{} in cone, no LOS", coneCount));
        } else if (friendlyCount > 0) {
            setReason(std::format("in cone but ignored: {} friendly", friendlyCount));
        } else {
            setReason("no npc in cone");
        }
    }

    /**
     * Why the lit-spot path posted nothing despite the beam landing on geometry: whether anyone was near the
     * patch and facing it separates "nobody could have seen it" from "somebody was looking, but through a
     * wall".
     */
    void NpcDetectionHandler::DebugState::recordNoLitSpotWitness()
    {
        if (!recording()) {
            return;
        }
        if (litSpotLookingCount > 0) {
            addReason(std::format("lit spot: {} looking, no LOS", litSpotLookingCount));
        } else {
            addReason("lit spot: nobody looking at it");
        }
    }

    /**
     * Note an NPC the beam is touching, with how well the engine itself thinks it can see the player. That
     * read is diagnostic only — the tick deliberately doesn't act on it (docs 3.0) — and the native behind it
     * creates actor-knowledge state, which a normal play session shouldn't be paying for a number nothing
     * reads; hence recording-only.
     */
    void NpcDetectionHandler::DebugState::recordCandidate(RE::Actor* npc, RE::Actor* player)
    {
        if (!recording()) {
            return;
        }
        detectionLevels +=
            std::format("{}{:08X} lvl {}{}", detectionLevels.empty() ? "" : ", ", npc->formID, f4vr::getDetectionLevel(npc, player), f4vr::isInActiveCombat(npc) ? " COMBAT" : "");
    }

    /**
     * The event just posted onto a lit NPC. The NPC's own position is kept alongside the event position
     * because the two differ (the event is offset toward the player, or moved onto them when spotted) and
     * the overlay draws the line between them.
     */
    void NpcDetectionHandler::DebugState::recordDirectEvent(const RE::Actor* npc, const RE::NiPoint3& eventPos, const int soundLevel, const bool spotted)
    {
        if (!recording()) {
            return;
        }
        event = Event{ .pos = eventPos, .npcPos = npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET), .soundLevel = soundLevel, .direct = true, .timeMs = nowMillis() };
        setReason(std::format("hit npc {:08X}, direct lvl {}{}, ({} in cone, {} friendly)",
            npc->formID,
            soundLevel,
            spotted ? " -> SPOTTED (event on player)" : "",
            coneCount,
            friendlyCount));
    }

    /**
     * The event just posted on the beam's lit patch of world geometry (no NPC was hit). The witness is kept
     * alongside it because the patch alone doesn't explain the event — the overlay draws the line from the
     * patch to the NPC that could see it, which is the whole reason it was posted.
     */
    void NpcDetectionHandler::DebugState::recordLitSpotEvent(const RE::NiPoint3& spot, const int soundLevel, const RE::Actor* witness)
    {
        if (!recording()) {
            return;
        }
        event = Event{ .pos = spot, .npcPos = witness->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET), .soundLevel = soundLevel, .direct = false, .timeMs = nowMillis() };
        // the miss reasons are moot once one is posted
        reason = std::format("lit spot seen by {:08X}, lvl {} ({} looking)", witness->formID, soundLevel, litSpotLookingCount);
    }

    /**
     * Start recording a raycast: the ray itself, which is all that can be known before it is cast.
     */
    void NpcDetectionHandler::DebugState::startProbe(RayProbe& probe, const std::string& label, const RE::NiPoint3& from, const RE::NiPoint3& to) const
    {
        if (recording()) {
            probe.label = label;
            probe.from = from;
            probe.to = to;
        }
    }

    /**
     * Note a volume the ray crossed without stopping, reported alongside whatever ends up stopping it.
     */
    void NpcDetectionHandler::DebugState::passedThrough(RayProbe& probe, const std::string& hit) const
    {
        if (recording()) {
            probe.through += (probe.through.empty() ? "" : ", ") + hit;
        }
    }

    /**
     * Render the live detection state via the framework debug overlay (gated by its bDebugDrawEnabled),
     * called every frame the feature is active so the visuals track the beam and auto-vanish with it:
     * the detection cone (yellow — narrower and shorter than the visual beam by design), the last
     * tick's event as a sphere + world label (red = direct on an NPC with a magenta line from the
     * beam origin to the lit NPC, orange = lit-spot on geometry), and a watch table (framework default
     * HUD in front of the HMD) reporting the cone, next tick, last event, and — key for tuning — WHY
     * the last tick did or didn't post an event ("npc why"). All draws are tagged with the
     * "NPC-DETECTION" channel so they can be silenced via sDebugDrawDisabledChannels without turning
     * the overlay off.
     */
    void NpcDetectionHandler::DebugState::draw()
    {
        if (!recording()) {
            return;
        }
        auto& dd = debug::dd();
        const auto channel = dd.channelScope("NPC-DETECTION");

        BeamCone cone;
        if (!getBeamCone(cone)) {
            dd.watch("BEAM", "none");
            return;
        }
        const float fovDeg = 2.0f * MatrixUtils::radsToDegrees(std::acos(std::clamp(cone.cosHalfAngle, -1.0f, 1.0f)));
        dd.cone(cone.origin, cone.direction, cone.range, fovDeg, debug::Color{ .r = 1, .g = 1, .b = 0, .a = 0.5f });
        dd.watch("BEAM", std::format("range {:.0f} fov {:.1f}", cone.range, fovDeg));

        // reason from the last throttled tick — the answer to "why didn't an NPC react" (shown every
        // frame; the tick only runs every iNpcDetectionIntervalMs)
        dd.watch("NPC EVENT", reason.empty() ? "no tick yet" : reason);
        // the engine's own perception of the player, per candidate NPC — nothing acts on it, it is here to
        // read while debugging: below 0 they have no idea, 0-20 suspicious/investigating, 20+ actually
        // seeing the player, reaching 50-60 point blank (docs 3.0).
        dd.watch("NPC DETECTION", detectionLevels.empty() ? "no npc in cone" : detectionLevels);
        // how far the last hit NPC was turned away from the player, against the instant-spot gate it feeds
        dd.watch("NPC FACING", std::format("{:.0f} deg off (gate {:.0f})", facingAngle, SPOTTED_FACING_DEGREES));
        drawProbes();

        if (event.timeMs == 0) {
            dd.watch("LAST NPC EVENT", "none");
            return;
        }

        // keep showing the last event for a few ticks so short flashes are inspectable
        const float ageSec = static_cast<float>(nowMillis() - event.timeMs) / 1000.0f;
        dd.watch("LAST NPC EVENT", std::format("{} lvl {} {:.1f} sec ago", event.direct ? "direct" : "lit-spot", event.soundLevel, ageSec));
        if (ageSec <= static_cast<float>(g_config.npcDetectionIntervalMs) / 1000.0f * 4.0f) {
            const auto& eventColor = event.direct ? debug::colors::Red : debug::colors::Orange;
            // distance-scaled so the event marker stays visible however far the beam reaches
            dd.sphere(event.pos, 9.0f, true, eventColor);
            dd.label(std::format("lvl {}", event.soundLevel), event.pos, eventColor);
            // the line to the NPC the event is about: from the beam origin on a direct hit (the beam that lit
            // them), from the lit patch on a lit-spot event (the sight line that made it worth posting)
            dd.line(event.direct ? cone.origin : event.pos, event.npcPos, debug::colors::Magenta);
            dd.point(event.npcPos, 6.0f, true, debug::colors::Magenta);
        }
    }

    /**
     * Draw the last tick's raycasts, colored by what ended them — cyan on a body (the beam reached an
     * actor), red on cover, green when nothing stopped them — each labelled in-world and in the watch table
     * with what it hit: the scene node and, decisively, its collision layer.
     */
    void NpcDetectionHandler::DebugState::drawProbes() const
    {
        auto& dd = debug::dd();
        dd.watch("LOS FILTER", std::format("{:08X}/{}", g_config.npcDetectionLosCollisionFilter, f4vr::getCollisionLayerName(g_config.npcDetectionLosCollisionFilter)));
        if (probes.empty()) {
            dd.watch("LOS RAYS", "none cast last tick");
            return;
        }
        for (const auto& probe : probes) {
            const auto& color = !probe.blocked ? debug::colors::Green : probe.hitActor ? debug::colors::Cyan : debug::colors::Red;
            dd.line(probe.from, probe.blocked ? probe.hitPos : probe.to, color);
            if (probe.blocked) {
                dd.point(probe.hitPos, 7.0f, true, color);
                dd.label(probe.label, probe.hitPos + RE::NiPoint3(0, 0, g_config.debug.flowFlag1), color);
            }
            dd.watch(probe.label, probe.what);
        }
    }
}
