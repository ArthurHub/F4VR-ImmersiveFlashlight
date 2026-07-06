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
   `ProcessLists::GetActorsWithinRangeOfPoint`, filtered: not the player, not dead, optionally not
   companions/teammates. A point-in-cone test (dot product against the cone's cosine) at chest
   height picks the actors the beam is touching.
3. **Line of sight** — nearest-first, up to 3 candidates per tick get a physics raycast
   (`bhkPickData` + `CombatUtilities::CalculateProjectileLOS`, the same primitive the framework's
   `isMovementSafe()` uses) from the beam origin to the chest point. The target point sits inside
   the NPC's own collision capsule, so a clear path still reports a "hit" just short of the target;
   only a hit meaningfully short of it (> ~60 units, capsule radius + margin) counts as a wall.
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

## 5. Cost

Per tick (default 2/s, only while the light is on and no config-UI preview is active): one engine
range query, a dot product per nearby actor, at most 4 raycasts (3 LOS + 1 lit-spot), one detection
event write. Nothing runs per frame; nothing allocates beyond the query's scrap array.

## 6. Tuning cheat-sheet

| Goal                          | Change                                               |
| ----------------------------- | ---------------------------------------------------- |
| Disable entirely              | `bNpcDetectionEnabled = false`                       |
| Only matter while sneaking    | `bNpcDetectionOnlyWhenSneaking = true`               |
| Companions react too          | `bNpcDetectionIgnoreCompanions = false`              |
| Snappier / lazier reactions   | `iNpcDetectionIntervalMs` down / up                  |
| Beam edge alerts too          | `fNpcDetectionFovMult` up toward 1.0                 |
| Long-range beam discipline    | `fNpcDetectionMaxRange` up (game units, 100 ≈ 1.4 m) |
| Softer / harder direct alert  | `iNpcDetectionDirectSoundLevel` (0 disables direct)  |
| No "they saw the light patch" | `iNpcDetectionLitSpotSoundLevel = 0`                 |
