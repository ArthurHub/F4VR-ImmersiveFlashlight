# NPC Light Detection — design & implementation notes

How the mod makes NPCs actually notice the player's flashlight beam, natively (no ESP, no perk,
no Papyrus), directionally (only what the beam touches matters), and LOS-gated (no pings through
walls). Implemented in `src/NpcDetectionHandler.{h,cpp}`, configured by the `NpcDetection*` INI keys.

Reference material: `Modding-Reference/F4VR/knowledge-base/flashlight_npc_detection_mechanics.md`.

## 1. The vanilla problem

Fallout 4 (and FO4 VR) NPC visual detection asks one question about the player: _"how lit is the
target?"_ — the lighting solver's `LightAmount` scaled by the `kModDetectionLight` perk entry point.
An **omnidirectional** light (vanilla Pip-Boy lamp) illuminates the wearer, so it raises detection a
little. A **spot light** — everything this mod produces — projects a cone _away_ from the wearer, so
the wearer's own `LightAmount` barely moves. Result: you can put a beam on a raider's chest from the
dark and the engine considers you unlit and undetected. The NPC never checks whether _it_ is inside
someone's beam; that test simply doesn't exist in the engine.

## 2. Prior art and why it isn't enough

**Flashlight Stealth Fix** (Papyrus + ESP + BakaFramework) solves this with a perk that sets the
player's `kModDetectionLight` multiplier to **1500** while the flashlight is on, plus an
omnidirectional `Actor.CreateDetectionEvent(player, 100)` ping every 3 seconds. It works, but:

- **Not directional.** The 1500x multiplier applies no matter where the beam points; the 3-second
  ping radiates in all directions. Enemies turn around when your beam is aimed at the floor behind
  you.
- **Through walls.** `CreateDetectionEvent` is a noise-style event — NPCs in radius react with no
  line-of-sight test.
- **Coarse cadence.** A 3-second Papyrus timer means up to 3 "free" seconds of beam-in-the-face.
- **Dependencies.** Needs an ESP slot, BakaFramework, and MCM.

## 3. Design

This mod owns the flashlight, which removes every hard part of the reference brief: no
flashlight-on detection is needed (`Utils::isFlashlightOn()`), and the exact beam origin, direction,
cone angle, and reach are already known per frame — the game light node that the mod itself
positions (`Flashlight::adjustFlashlightTransformToHandOrHead()`), and the active per-location beam
config (`FlashlightState::flashlightFov` / `flashlightRadius`). The beam used for detection is the
beam the player _sees_, including the weapon-lamp rooting.

Every detection tick (`iNpcDetectionIntervalMs`, default 500 ms) while the light is on:

1. **Beam cone** — origin/direction from the light node's world transform; cone half-angle =
   `activeBeamFov / 2 * fNpcDetectionFovMult` (the visual cone's dim outer edge shouldn't alert, so
   the default multiplier 0.75 uses the bright core); range = `min(activeBeamRadius,
fNpcDetectionMaxRange)` (beam radii reach 7000 units ≈ 90 m; alerting at that distance is
   unreasonable, so an independent cap, default 2000 ≈ 25 m). The cap gates only _eligibility_ —
   per-location beam strength still differentiates how hard the ping lands (step 4).
2. **Candidates** — all loaded actors within range of the beam origin via the engine's own
   `ProcessLists::GetActorsWithinRangeOfPoint`, filtered: not the player, not dead, not
   companions/teammates (they already know where the player is). A point-in-cone test (dot product against the cone's cosine) at chest
   height picks the actors the beam is touching. In-cone actors then pass a hostility filter when
   `bNpcDetectionOnlyHostileNpcs` is on (default): `Actor::GetHostileToActor(player)`, the engine's own
   faction/relationship + combat-state hostility query (VR `RelocationID(1148686, 2229968)`). It runs
   **last** in the filter chain — it's a native call, so only the handful of actors the beam actually
   touches are worth asking about, and it keeps step 3's small raycast budget aimed at real threats
   instead of a crowd of neutrals standing between the player and the enemy behind them. The cost is
   that conditionally-hostile NPCs (a guard who'd only turn on you after catching you, trespass cases)
   stop reacting; turn the flag off to get the vanilla-style everyone-notices behavior back.
3. **Line of sight** — nearest-first, up to 3 candidates per tick get a physics raycast from the beam
   origin to the chest point (see §4 for which engine query, and for the debugging surface around it).
   The target point sits inside the NPC's own collision capsule, so a clear path still reports a "hit"
   just short of the target; only a hit meaningfully short of it (> ~60 units, capsule radius +
   margin) counts as a wall.
4. **One detection event** per tick, placed by what the beam touched:
   - **Direct hit** (an NPC is in the cone with LOS): the event is placed near the lit NPC, offset
     _toward the player_ by **25 % of the NPC→player distance, clamped to [150, 1000] units** and
     never past the player. The engine makes NPCs investigate the event's location, so the lit NPC
     (and only actors near it — the event radius stays small) turns and moves toward the light source
     instead of searching its own feet; scaling the offset with distance keeps that pull meaningful for
     far NPCs instead of a token shift. Sound level `iNpcDetectionDirectSoundLevel`
     (default 100, the Papyrus mod's default) scaled by **beam strength at the target**: linear
     falloff over the location's _full visual radius_ (not the range cap) times its _fade_
     (intensity), clamped to [0.25, 1.5]. This keeps per-location beam character meaningful even
     though the eligibility range is capped identically for all locations — e.g. at 20 m the
     7000-radius / 1.4-fade hand torch pings at ~full strength while the 2000-radius / 1.1-fade
     head lamp is already down at the floor.
   - **Lit spot** (nobody in the cone): the beam center is raycast to where it terminates, and a
     weaker event (`iNpcDetectionLitSpotSoundLevel`, default 40) is placed on the lit surface. An
     NPC standing next to the bright patch on the wall investigates the patch — which faces them
     toward the beam, and the next ticks escalate via the direct path once they step into the cone.
     This also covers the light pool at the player's own feet giving them away at close quarters.

The event is `AIProcess::SetActorsDetectionEvent(player->currentProcess, player, location, level,
nullptr)` — the native behind Papyrus `CreateDetectionEvent`, owned by the player so investigation
and blame attach to the player. Events decay engine-side; turning the light off simply stops posting
(no cleanup needed).

### What was deliberately not done

- **`kModDetectionLight` perk / `HandleEntryPoint` hook** (the brief's "Strategy B"). A runtime perk
  needs an ESP or fragile runtime form construction; a function-entry detour on the variadic
  `BGSEntryPoint::HandleEntryPoint` (VR `0x54b7e0`) is the highest-risk pattern in the mod's
  toolbox, and the hook still can't tell _which observer_ is asking — making it directional requires
  hooking the per-pair detection update, which has no published VR address. The detection-event
  plane achieves the directional behavior with zero hooks. If a future version wants the _visual_
  detection channel (stealth-meter pressure without the investigate behavior), that hook is the
  entry point to research.
- **NPC facing test.** The engine treats detection events as noise, so a beam on an NPC's back
  alerts them. That's accepted as realistic-enough (light on your back scatters onto surfaces in
  front of you) and much cheaper than a facing + peripheral-vision model.
- **In-game config UI toggle.** `MiscScreen` toggles need a bespoke button NIF; until that asset
  exists the feature is INI-only (hot-reload applies changes live).

### Beam axis derivation

The light node's beam-forward axis is local **+X**, determined empirically by drawing a marker out
along each candidate light-local axis (transformed to world by the codebase convention
`world.rotate.Transpose() * localAxis`, see `FlashlightState::refreshGripStyle()`) and taking the one
whose line tracked the visible beam. `getBeamCone()` uses `BEAM_LOCAL_AXIS = (1, 0, 0)`.

One bug masked this and is worth recording: `Flashlight::adjustFlashlightTransformToHandOrHead()` sets
the light node's `local` each frame but doesn't propagate it to `world` (the engine does that later, at
render). So reading `lightNode->world` in the same frame returned the **head-light-parent** transform
(light at the HMD, following head look), not the hand. `getBeamCone()` now calls
`f4vr::updateTransforms(lightNode)` before reading its world. An earlier `−Z` guess (reverse-engineered
from the weapon mount's `Euler(90, 0, −90)`) happened to point "down" only because of this stale head
transform.

## 4. The line-of-sight raycast

Both the per-NPC LOS check and the lit-spot probe are the same ray — beam origin → target, cast by
`NpcDetectionHandler::castRay()` through `bhkWorld::PickObject(bhkPickData&)` on the physics world of
the player's cell (`player->parentCell->GetbhkWorld()`), the same call ROCK uses for VR world ray
clipping. Start/end go in as **game-space** points (`bhkPickData::SetStartEnd` converts to Havok's ~1/70
scale internally), and the call happens on the game thread from `onFrameUpdate`, which is what
`PickObject`'s internal world locking expects.

The **collision filter** is `sNpcDetectionLosCollisionFilter`, default `02420028` — the game's own
item-picker filter, which other VR physics mods use for "where does this ray hit the world". Layers are
`RE::COL_LAYER`, the low 7 bits of the filter word.

> The player's own filter (`Actor::GetCollisionFilter()`) was the original choice, on the reasoning that
> its collision **group** makes the ray ignore the player's body. It also makes the ray look like a
> character controller, which is what broke it — see below.

> The combat AI's `CombatUtilities::CalculateProjectileLOS(actor, projectile, pickData)` was evaluated
> as an alternative and rejected: it re-derives the physics query from a `BGSProjectile` form, so its
> answer depends on which projectile it is handed — by default whatever sorts first in the load order,
> i.e. a function of the user's mod list. `PickObject` is the same raycast without that input.

### Passing through what can't block light

A raycast stops on the first body it meets, and plenty of those are things a light beam goes straight
through. The one that actually broke this feature is **`kActorZone`**: invisible AI trigger volumes
that exist to catch *character controllers* — which is exactly what the ray looked like while it
carried the player's collision filter. The beam terminated on a trigger box in the middle of a room, so
the lit spot landed in mid-air and NPC LOS read as blocked by nothing.

The item-picker filter doesn't collide with those, which is the primary fix. Behind it, `castRay()`
also retries: when a hit's layer is in `PASS_THROUGH_LAYERS` (actor zones, triggers, portals, acoustic
spaces, invisible walls, stair helpers, camera/door detection volumes, gas and cloud traps, and the
transparent/glass layers — a `constexpr` mask built from `RE::COL_LAYER` enumerators, not configurable),
it resumes the cast 2 units past that surface and keeps going, up to 4 times. Hit distance is
accumulated back onto the **original** ray, so `hitPos` stays comparable. Running out of retries is
reported as open air, not as a wall — a stack of five volumes is a room full of triggers, not cover.
Pass-throughs are named in the probe, so if the filter ever starts catching something new, it's visible
rather than silent.

### Debugging what the beam hit

The diagnostic that solved this was one string: the hit's **collision layer**. A beam dying in mid-air
is inexplicable until the label reads `actorZone`. So each cast records a `RayProbe` — the ray, the hit
point, and `what` ("`<node>` [`<layer>`] at `<distance>`", or why nothing stopped it) — and with the
framework's `bDebugDrawEnabled` on, `drawProbesDebug()` draws the ray **cyan** (it reached a body — the
hit is on one of `ACTOR_LAYERS`, which for the LOS test is the good outcome), **red** (cover stopped it)
or **green** (nothing stopped it), marks and labels the hit point in-world, and puts `what` in the watch
table (channel `NPC-DETECTION`) keyed by which ray it was. Anything the ray passed through is named in
the same string, as is `no physics world` (cell transition — LOS then fails open).

The probes are the overlay's alone: with `bDebugDrawEnabled` off, `castRay()` skips every label and hit
description and records nothing, setting only the `blocked` / `hitPos` its callers read.

## 5. Cost

Per tick (default 2/s, only while the light is on and no config-UI preview is active): one engine
range query, a dot product per nearby actor, one hostility query per _in-cone_ actor (usually 0-2),
at most 4 raycasts (3 LOS + 1 lit-spot), one detection event write. Nothing runs per frame; nothing
allocates beyond the query's scrap array.

## 6. Tuning cheat-sheet

| Goal                          | Change                                               |
| ----------------------------- | ---------------------------------------------------- |
| Disable entirely              | `bNpcDetectionEnabled = false`                       |
| Only matter while sneaking    | `bNpcDetectionOnlyWhenSneaking = true`               |
| Neutral NPCs react too        | `bNpcDetectionOnlyHostileNpcs = false`               |
| Snappier / lazier reactions   | `iNpcDetectionIntervalMs` down / up                  |
| Beam edge alerts too          | `fNpcDetectionFovMult` up toward 1.0                 |
| Long-range beam discipline    | `fNpcDetectionMaxRange` up (game units, 100 ≈ 1.4 m) |
| Softer / harder direct alert  | `iNpcDetectionDirectSoundLevel` (0 disables direct)  |
| No "they saw the light patch" | `iNpcDetectionLitSpotSoundLevel = 0`                 |
| LOS wrong (walls / open air)  | `bDebugDrawEnabled = true`, then read §4             |
| Beam stops on thin air        | add its layer to `PASS_THROUGH_LAYERS` (code)        |
| Beam passes through walls     | `sNpcDetectionLosCollisionFilter` (another layer)    |
