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

    // Half-angle off the NPC's own heading within which the player counts as being in front of it, for the
    // instant-spot escalation — 90 is the whole forward hemisphere, which is as loose as "looking at you"
    // can be without becoming "facing away from you".
    constexpr float SPOTTED_FACING_DEGREES = 90.0f;

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
        if (g_config.debug.drawEnabled) {
            drawDebugOverlay();
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
        _debugReason.clear();
        _debugProbes.clear();
        BeamCone cone;
        if (!getBeamCone(cone)) {
            _debugReason = "no beam cone";
            return;
        }

        if (runNpcDirectDetection(cone)) {
            return;
        }

        runLitSpotDetection(cone);
    }

    /**
     * Direct path: when enabled and an NPC is lit in the cone (with line of sight), post a detection event and
     * return true; otherwise false, so the caller falls back to the lit-spot path. The event sits between the
     * NPC and the player — the engine's investigate-the-location behavior then turns them toward the light
     * instead of their own feet — or on the player at point blank / on a hit counting as being seen.
     */
    bool NpcDetectionHandler::runNpcDirectDetection(const BeamCone& cone)
    {
        if (!g_config.npcDetectionDirectEnabled) {
            _debugReason = "direct off";
            return false;
        }

        const auto npc = findNearestLitNpc(cone);
        if (!npc) {
            if (_debugConeCount > 0) {
                _debugReason = std::format("{} in cone, no LOS", _debugConeCount);
            } else if (_debugFriendlyCount > 0) {
                _debugReason = std::format("in cone but ignored: {} friendly", _debugFriendlyCount);
            } else {
                _debugReason = "no npc in cone";
            }
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
        logger::sampleDebug(3000,
            "NpcDetector: NPC {:08X} lit at beam-dist {:.0f}, event ({:.0f},{:.0f},{:.0f}) {}",
            npc->formID,
            beamDist,
            eventPos.x,
            eventPos.y,
            eventPos.z,
            onPlayer ? (spotted ? "ON the player (spotted)" : "ON the player (point blank)") : std::format("offset {:.0f} toward player", offset));
        postDetectionEvent(eventPos, soundLevel);
        _debugEvent =
            DebugEventState{ .eventPos = eventPos, .npcPos = npcPos + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET), .soundLevel = soundLevel, .direct = true, .timeMs = nowMillis() };
        _debugReason = std::format("hit npc {:08X}, direct lvl {}{}, ({} in cone, {} friendly)",
            npc->formID,
            soundLevel,
            spotted ? " -> SPOTTED (event on player)" : "",
            _debugConeCount,
            _debugFriendlyCount);
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
        _debugFacingAngle = facingAngle;
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
     * center terminates on world geometry, so NPCs next to that bright patch investigate it. A no-op when the
     * lit-spot path is disabled or its sound level is zeroed, or when the beam ends in open air (nothing to
     * notice).
     */
    void NpcDetectionHandler::runLitSpotDetection(const BeamCone& cone)
    {
        // preserve the direct-path miss reason (if any) and append the lit-spot outcome to it
        const std::string directMiss = _debugReason.empty() ? "" : _debugReason + " - ";
        if (!g_config.npcDetectionLitSpotEnabled || g_config.npcDetectionLitSpotSoundLevel <= 0) {
            _debugReason = directMiss + "lit-spot off";
            return;
        }

        RE::NiPoint3 spot;
        if (!getBeamTerminationSpot(cone, spot)) {
            _debugReason = directMiss + "no beam termination spot found";
            return;
        }

        const float spotDist = MatrixUtils::vec3Len(spot - cone.origin);
        const int soundLevel = scaleSoundLevelByBeamStrength(g_config.npcDetectionLitSpotSoundLevel, spotDist);
        postDetectionEvent(spot, soundLevel);
        _debugEvent = DebugEventState{ .eventPos = spot, .soundLevel = soundLevel, .direct = false, .timeMs = nowMillis() };
        _debugReason = std::format("lit spot, lvl {}", soundLevel);
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
     * Teammates are always skipped, and non-hostile actors too when bNpcDetectionOnlyHostileNpcs is on, so
     * the raycast budget isn't spent on a crowd that wouldn't act on the beam anyway.
     */
    RE::Actor* NpcDetectionHandler::findNearestLitNpc(const BeamCone& cone)
    {
        _debugConeCount = 0;
        _debugFriendlyCount = 0;
        _debugDetectionLevels.clear();
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
            // Companions/teammates already know where the player is - no point investigating the beam.
            if (RE::IsPlayerTeammate(*npc)) {
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
            // The only engine call in the filter, so it runs last: just the handful of actors the beam
            // actually touches are worth asking about.
            if (g_config.npcDetectionOnlyHostileNpcs && !npc->GetHostileToActor(player)) {
                _debugFriendlyCount++;
                continue;
            }
            if (g_config.debug.drawEnabled) {
                // Diagnostic only - the beam deliberately does not act on how well the NPC already sees the
                // player (docs 3.0). Read behind the overlay flag because the native creates actor-knowledge
                // state, which a normal play session shouldn't be paying for a number nothing reads.
                _debugDetectionLevels += std::format("{}{:08X} lvl {}{}",
                    _debugDetectionLevels.empty() ? "" : ", ",
                    npc->formID,
                    f4vr::getDetectionLevel(npc, player),
                    f4vr::isInActiveCombat(npc) ? " COMBAT" : "");
            }
            inCone.emplace_back(dist, npc);
        }
        _debugConeCount = static_cast<int>(inCone.size());

        // Physics raycasts per tick spent finding a visible in-cone NPC (nearest first).
        constexpr int MAX_LOS_CHECKS_PER_TICK = 3;

        std::ranges::sort(inCone, {}, &std::pair<float, RE::Actor*>::first);
        int losChecks = 0;
        for (const auto& npc : inCone | std::views::values) {
            if (losChecks++ >= MAX_LOS_CHECKS_PER_TICK) {
                break;
            }
            const auto label = g_config.debug.drawEnabled ? std::format("los {:08X}", npc->formID) : std::string();
            if (isLineOfSightClear(cone.origin, npc->data.location + RE::NiPoint3(0, 0, TARGET_HEIGHT_OFFSET), label)) {
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
        // The probe feeds the debug overlay and nothing else, so with the overlay off none of its strings
        // are formatted and nothing is retained — only probe.blocked / probe.hitPos, which the callers read.
        const bool recording = g_config.debug.drawEnabled;
        if (recording) {
            probe.label = label;
            probe.from = from;
            probe.to = to;
        }
        std::string through; // what the ray went through on its way, reported alongside what stopped it
        const auto record = [&]<typename... Args>(const std::format_string<Args...> fmt, Args&&... args) {
            if (recording) {
                probe.what = std::format(fmt, std::forward<Args>(args)...);
                if (!through.empty()) {
                    probe.what += std::format(" (through {})", through);
                }
                _debugProbes.push_back(probe);
            }
            return probe.blocked;
        };

        const auto player = f4vr::getPlayer();
        const auto cell = player ? player->parentCell : nullptr;
        const auto world = cell ? cell->GetbhkWorld() : nullptr;
        if (!world) {
            return record("no physics world");
        }
        const float rayLen = MatrixUtils::vec3Len(to - from);
        if (rayLen < 1.0f) {
            return record("degenerate ray");
        }
        const auto direction = (to - from) * (1.0f / rayLen);

        constexpr int MAX_PASS_THROUGH_HITS = 2;

        auto start = from;
        for (int pass = 0; pass <= MAX_PASS_THROUGH_HITS; pass++) {
            RE::bhkPickData pickData;
            pickData.SetStartEnd(start, to);
            pickData.collisionFilter.filter = g_config.npcDetectionLosCollisionFilter;

            if (!world->PickObject(pickData) || !pickData.HasHit()) {
                return record("clear over {:.0f}", rayLen);
            }

            // hit distance along the ORIGINAL ray, so hitPos stays comparable across retries
            const float hitDist = MatrixUtils::vec3Len(start - from) + pickData.GetHitFraction() * MatrixUtils::vec3Len(to - start);
            const auto hitFilter = pickData.result.hitBodyInfo.m_shapeCollisionFilterInfo.storage;
            const auto node = pickData.GetNiAVObject();
            const auto hit = recording ? std::format("{} [{}]", node && !node->name.empty() ? node->name.c_str() : "?", f4vr::getCollisionLayerName(hitFilter)) : std::string();

            if (f4vr::isCollisionLayerInMask(hitFilter, PASS_THROUGH_LAYERS)) {
                constexpr float PASS_THROUGH_STEP = 2.0f;
                if (pass >= MAX_PASS_THROUGH_HITS) {
                    return record("pass-through limit");
                }
                if (recording) {
                    through += (through.empty() ? "" : ", ") + hit;
                }
                if (hitDist + PASS_THROUGH_STEP >= rayLen) {
                    return record("nothing beyond"); // the volume sat at the very end of the ray
                }
                start = from + direction * (hitDist + PASS_THROUGH_STEP);
                continue;
            }

            probe.blocked = true;
            probe.hitActor = f4vr::isCollisionLayerInMask(hitFilter, ACTOR_LAYERS);
            probe.hitPos = from + direction * hitDist;
            return record("{} at {:.0f}", hit, hitDist);
        }
        return record("pass-through limit");
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
    void NpcDetectionHandler::drawDebugOverlay()
    {
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
        dd.watch("NPC EVENT", _debugReason.empty() ? "no tick yet" : _debugReason);
        // the engine's own perception of the player, per candidate NPC — nothing acts on it, it is here to
        // read while debugging: below 0 they have no idea, 0-20 suspicious/investigating, 20+ actually
        // seeing the player, reaching 50-60 point blank (docs 3.0).
        dd.watch("NPC DETECTION", _debugDetectionLevels.empty() ? "no npc in cone" : _debugDetectionLevels);
        // how far the last hit NPC was turned away from the player, against the instant-spot gate it feeds
        dd.watch("NPC FACING", std::format("{:.0f} deg off (gate {:.0f})", _debugFacingAngle, SPOTTED_FACING_DEGREES));
        drawProbesDebug();

        if (_debugEvent.timeMs == 0) {
            dd.watch("LAST NPC EVENT", "none");
            return;
        }

        // keep showing the last event for a few ticks so short flashes are inspectable
        const float ageSec = static_cast<float>(nowMillis() - _debugEvent.timeMs) / 1000.0f;
        dd.watch("LAST NPC EVENT", std::format("{} lvl {} {:.1f} sec ago", _debugEvent.direct ? "direct" : "lit-spot", _debugEvent.soundLevel, ageSec));
        if (ageSec <= static_cast<float>(g_config.npcDetectionIntervalMs) / 1000.0f * 4.0f) {
            const auto& eventColor = _debugEvent.direct ? debug::colors::Red : debug::colors::Orange;
            // distance-scaled so the event marker stays visible however far the beam reaches
            dd.sphere(_debugEvent.eventPos, 9.0f, true, eventColor);
            dd.label(std::format("lvl {}", _debugEvent.soundLevel), _debugEvent.eventPos, eventColor);
            if (_debugEvent.direct) {
                dd.line(cone.origin, _debugEvent.npcPos, debug::colors::Magenta);
                dd.point(_debugEvent.npcPos, 6.0f, true, debug::colors::Magenta);
            }
        }
    }

    /**
     * Draw the last tick's raycasts, colored by what ended them — cyan on a body (the beam reached an
     * actor), red on cover, green when nothing stopped them — each labelled in-world and in the watch table
     * with what it hit: the scene node and, decisively, its collision layer.
     */
    void NpcDetectionHandler::drawProbesDebug()
    {
        auto& dd = debug::dd();
        dd.watch("LOS FILTER", std::format("{:08X}/{}", g_config.npcDetectionLosCollisionFilter, f4vr::getCollisionLayerName(g_config.npcDetectionLosCollisionFilter)));
        if (_debugProbes.empty()) {
            dd.watch("LOS RAYS", "none cast last tick");
            return;
        }
        for (const auto& probe : _debugProbes) {
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
