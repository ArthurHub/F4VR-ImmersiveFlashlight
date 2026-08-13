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
   height picks the actors the beam is touching. How well an NPC already sees the player is deliberately
   **not** part of the filter — that skip was built and then removed; §3.0 records why, and the knowledge
   behind it. In-cone actors then pass a
   hostility filter when
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
     _toward the player_ by **30 % of the NPC→player distance, clamped to [200, 1000] units**. The
     engine makes NPCs investigate the event's location, so the lit NPC
     (and only actors near it — the event radius stays small) turns and moves toward the light source
     instead of searching its own feet; scaling the offset with distance keeps that pull meaningful for
     far NPCs instead of a token shift. Inside the clamp floor (200 units ≈ 2.8 m) that offset would
     reach the player anyway, so the event goes **on the player** instead — the floor doubles as the
     point-blank radius, and §3.1 lands the event there too on a hit that counts as being seen.
     Sound level `iNpcDetectionDirectSoundLevel`
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

### 3.0 The "already spotted me" skip, and why it was removed

Step 2 used to drop in-cone NPCs that had already **seen** the player (`bNpcDetectionSkipSpottedNpcs` /
`iNpcDetectionSpottedLevel`): they know where the player is, so the beam tells them nothing, and the
tick's single event and 3-raycast budget were better spent on someone still unaware. It was built,
measured, shipped, and then cut. What it lost on:

- **The cost ran the wrong way.** The skip asked `RequestDetectionLevel` — a native that _creates_
  actor-knowledge state — for every in-cone NPC every tick, to save at most three cheap raycasts. It was
  the only impure read in the filter chain, and plausibly cost more than it saved.
- **It only bit in a corner.** The search takes the _nearest_ in-cone NPC with LOS, so the skip changed
  the outcome only with 2+ NPCs in the cone where the nearest one had already seen the player.
- **Two keys, near-identical names, unrelated meanings.** `iNpcDetectionSpottedLevel` (engine perception,
  0-60) sat one line from `iNpcDetectionSpottedEventLevel` (scaled beam sound level, 0-150).
- **What removing it costs:** a lit NPC already fighting the player now re-passes §3.1 every tick, so the
  event lands on the player's own position at ~2 Hz through a firefight. `SetActorsDetectionEvent` is a
  world event with a radius, so NPCs the beam never touched can pick it up — accepted, because gunfire in
  that same firefight already posts louder events from the same place.

The read itself is kept as a **debug-only** diagnostic: `findNearestLitNpc()` calls it behind
`bDebugDrawEnabled` to fill the `NPC DETECTION` watch row, so the numbers below stay readable in-game
while a normal session never makes the call. The rest of this section is what measuring it established,
kept because the next feature wanting "does this NPC see the player" on VR needs all of it. Three engine
reads offer to answer that and two of them answer a different question — all measured in-game, not assumed:

- **Combat state is the wrong question.** An NPC walking over to investigate one of this mod's own
  detection events is _already_ in active combat with the player as its `currentCombatTarget`, while
  having no idea where the player is. Skipping on combat therefore drops exactly the NPCs the beam is
  supposed to escalate. (Both reads have their own traps besides — see below — but the semantics are the
  disqualifier.)
- **`Actor::RequestDetectionLevel(Actor* target, DETECTION_PRIORITY)`** is the read, via
  `f4vr::getDetectionLevel()` — AddressLib **943772** → VR `0x140dfb270`, already declared by
  CommonLibF4VR, taking and returning plain scalars. It is the engine's own perception answer, so light,
  distance, facing and sneak are already in it.

  The return is a **continuous level, not a detected/undetected flag**, which is the part the reference
  mods don't tell you. Measured in-game off the `NPC DETECTION` watch row:

  | Level | Meaning |
  | ----- | ------- |
  | `< 0` | Unaware of the player |
  | `0` – `~20` | Aware — suspicious, investigating, has not located the player |
  | `>= ~20` | Has actually seen the player, climbing to roughly **50-60** at point blank |

  The band is the part that matters to anyone reusing this read. `>= 0` is the "detected" boundary
  Heisenberg-Physical-Interactions (`PickpocketHandler`) and Lighthouse Papyrus Extender use, and it
  answers "does it suspect anything at all" — the right question for gating pickpocketing, the wrong one
  here: at `>= 0` the skip dropped every NPC investigating one of this mod's own detection events,
  precisely the ones the beam exists to escalate, which is why its threshold ended up at **20**. That
  measured 0-20 band is also why the channel-splitting `RequestDetectionLevels` below was never needed.
- **It creates state, so the observer needs high process data.** The call runs through
  `AIProcess::CreateActorKnowledge` → the knowledge entry lives in `currentProcess->high`, so
  `getDetectionLevel` reports "not detected" rather than calling when that is null. That side effect is
  the first bullet's cost argument, and the reason the surviving call sits behind the overlay flag.
- **`Actor::RequestDetectionLevels` (plural, 1368522 → VR `0x140dfb3a0`) is deliberately not used.** It
  splits detection into visual and sound channels, which looked like the only way to tell "saw me" from
  "heard me" — but the singular's level band above turned out to answer that on its own. Two crashes came
  out of trying it, both worth recording:
  - It returns through a **hidden result pointer** (MSVC sret), not in EAX. Declared as a by-value return,
    every argument shifts one register: the `DETECTION_PRIORITY` lands in the target-actor slot and the
    callee faults dereferencing it — an access violation at `0x17` (= `kNormal`, 3, plus the field offset
    `0x14`) inside `AIProcess::CreateActorKnowledge`.
  - **The addresslib symbol does not name a return type**, and `DetectionLevels` (4 bytes, per
    libxse-commonlibf4) was only a guess from the plural name. Handing it a 16-byte buffer still crashed —
    in `GetHostileToActor` on the *next* line, on an actor that had passed two virtual calls moments
    earlier, i.e. the callee wrote past the buffer and corrupted the caller's stack frame. `DetectionData`
    is 0x88 bytes and holds `DetectionLevels` at offset 0x80, which would do exactly that.

  Anyone picking this back up needs the real return type first (disassemble at `0x140dfb3a0` and read what
  it writes through `rcx`), not another guess. Note also that Buffout's `ActorIsHostileToActor` fix does
  **not** protect a direct `GetHostileToActor` call — it patches the Papyrus native entry point only.
- **Sign convention** for "detected" is `>= 0`, taken from Heisenberg-Physical-Interactions
  (`PickpocketHandler`: _"negative = hidden, 0+ = detected"_). Lighthouse Papyrus Extender uses `> 0` for
  the singular variant instead, so the two disagree at exactly 0; the overlay prints the raw numbers
  (`NPC DETECTION` watch row) to settle it against real values rather than by argument.

Two VR traps found on the way, recorded because they will bite the next combat-related feature:

- **`Actor::IsInCombat()` (vtable `0xFE`) always returns false on the VR runtime**, even for an enemy
  actively shooting the player. The bundled CommonLibF4VR headers declare that slot as taking no
  arguments; f4sevr's VR SDK declares the same slot as `IsInCombat(UInt64, UInt64)`. The framework wraps
  the native instead — `f4vr::isInActiveCombat()` = `Actor::IsInActiveCombat`, AddressLib **84790** → VR
  `0x140e50350` (status 4), no vtable index involved, and stricter besides: false once the combat group
  has ended.
- **`Actor::currentCombatTarget` (offset `0x380`) is sticky.** It keeps naming the player long after a
  fight is over. (The offset itself is correct on VR — it resolves to a valid handle pointing at exactly
  the player, which a wrong offset would not.)

### 3.1 Instant spotting

A detection event placed near an NPC only makes it _investigate a location_. Putting a bright beam in a
raider's face from four meters away should not make them wander over to look at the floor — they've
**seen** you. So `tryEscalateToSpotted()` marks a direct hit as a spotting when all of:

- the direct event's **scaled** sound level reached `iNpcDetectionSpottedEventLevel` (default 100). Since
  the level already scales with beam strength at the target (§3 step 4), thresholding on it is really a
  "bright and close" test, tuned per location by that location's radius/fade. With the default base
  level of 100 the reachable range is [25, 150], so 100 ≈ full-strength beam on them, and anything above
  150 disables the escalation,
- the NPC is **hostile** to the player. Re-checked here even when the candidate filter already did it,
  because that filter is optional (`bNpcDetectionOnlyHostileNpcs`) and escalating on a
  neutral over a flashlight would be wrong,
- the NPC is **facing** the player: `f4vr::getActorFacingAngleTo()` within `SPOTTED_FACING_DEGREES`
  (hardcoded 90 — a half-angle to either side, so the whole forward hemisphere).
  Facing is the actor's own heading (`data.angle.z`), measured horizontally — **not**
  `Actor::GetEyeVector()`, which was tried first and returns unusable values on the VR runtime: it fills
  its `origin` out-param with what is plainly a direction vector (`-0.9, 0, 0` against world-scale
  coordinates). Heading is also the better signal regardless — it's what the AI turns toward a target,
  while head look-at is cosmetic on top — and being a plain member read it cannot fault. The overlay's
  `NPC FACING` row prints the measured angle against the gate.

**What escalation actually means here.** The obvious reading — "put them into combat" — turned out to be
mostly beside the point, and cost four attempts to learn. An NPC that has been hunting the player is
*already* in active combat with them (`activeCombat true`, `currentCombatTarget` = `0x14`) while having no
idea where they are; it is searching. Starting combat on such an NPC is a no-op by definition, which is
exactly what the instrumented log showed on every tick:

```
startCombat: actor 0004B2BA activeCombat true -> true, combatTarget 00000014
```

So the escalation **is** placing the tick's detection event on the player's own position instead of offset
near the NPC. That tells a searching NPC where the player actually is, and vanilla detection closes the loop
from there. No combat call is involved: an NPC that isn't already hunting the player is brought into combat
by the detection event itself, so forcing it adds nothing in either state.

### 3.1.1 Forcing combat — not used here, but mapped

No combat call is made by this feature, for the reasons above. The three engine entry points were mapped
and **all three verified working on VR 1.2.72**, so they stay in the framework behind
`f4vr::startCombat(actor, target, method)` / `f4vr::StartCombatMethod` for any future caller. Recorded
because "make an NPC attack X" is a recurring need and this took four rounds to pin down.

The precondition that matters: **all three do nothing to an actor already in combat with the target.**
That is a no-op, not a failure, and it is what made the first three rounds look like broken calls. Verify
against an actor that is *not* already fighting, and watch for `activeCombat false -> true`.

| Method | Call | AddressLib → VR | Timing |
| ------ | ---- | --------------- | ------ |
| `EnterCombat` (default) | `AIProcess::EnterCombat(self, target, null)` | 1514649 → `0x140e8f700` | immediate |
| `ActorStartCombat` | `Actor::StartCombat(target, commandingActor)` | 765218 → `0x140e4fe00` | immediate |
| `TaskQueue` | `TaskQueueInterface::QueueActorStartCombat(actor, target, commandAlly)` | 179777 → `0x140daa620` | **deferred** — combat is not yet set when the call returns; it lands a frame or more later |

`EnterCombat` is the default because it is what the engine's own Papyrus `SendAssaultAlarm` calls right
after `Actor::AttackAlarm` — deliberately without that `AttackAlarm`, which files a `kAttack` crime
(crime-faction PC-enemy, bounty, witness-guard propagation): right for "the player shot me", wrong for
"I noticed a flashlight".

Provenance notes:

- **`AIProcess::EnterCombat`** — address and parameter list from Heisenberg-Physical-Interactions, which
  RE-verified them by decompiling `Fallout4VR.exe`. Called on the NPC's own `currentProcess`; combat lives
  in HIGH process data, so an actor without one cannot enter it.
- **`Actor::StartCombat`** — the only one of the three whose address the VR database lists at status 2. Its
  parameter list is pinned by the Papyrus binding, which `Fallout4.pdb` names
  `GameScript::mem_Actor_StartCombat(IVirtualMachine*, uint, Actor* self, Actor* target, bool commandAlly)`
  — matching `Actor.StartCombat(Actor akTarget, bool abCommandAlly)`, so the member takes `(target,
commandingActor)` with the second null for anyone aggroing on its own account. Calling that binding
  directly would be safer still (the framework already calls Papyrus natives as `(vm, stackID, actor, …)`)
  but its address `0x14138B8A0` has **no row in the VR address database at all**, and the nearest mapped
  neighbour is 0x500 bytes away — interpolation, not a lookup.
- **`TaskQueueInterface::QueueActorStartCombat`** — the function is mapped at status 4, but its queue
  singleton is AddressLib **7491**, listed at status 2 under an auto-generated name (`DAT_145ac64f0`). That
  made it the shakiest chain on paper; in practice it works, it is simply deferred, which is why an
  immediately-following state read still shows `activeCombat false`.

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
at most 4 raycasts (3 LOS + 1 lit-spot), one detection event write, and — only on a direct hit — one
facing angle plus one more hostility query. Nothing runs per frame; nothing allocates beyond the
query's scrap array. The per-NPC detection-level query is debug-only (§3.0) and costs nothing with the
overlay off.

## 6. Tuning cheat-sheet

| Goal                          | Change                                               |
| ----------------------------- | ---------------------------------------------------- |
| Disable entirely              | `bNpcDetectionEnabled = false`                       |
| Only matter while sneaking    | `bNpcDetectionOnlyWhenSneaking = true`               |
| No instant spotting           | `iNpcDetectionSpottedEventLevel` above 150           |
| Spotted only point-blank      | `iNpcDetectionSpottedEventLevel` up toward 150        |
| Neutral NPCs react too        | `bNpcDetectionOnlyHostileNpcs = false`               |
| Snappier / lazier reactions   | `iNpcDetectionIntervalMs` down / up                  |
| Beam edge alerts too          | `fNpcDetectionFovMult` up toward 1.0                 |
| Long-range beam discipline    | `fNpcDetectionMaxRange` up (game units, 100 ≈ 1.4 m) |
| Softer / harder direct alert  | `iNpcDetectionDirectSoundLevel` (0 disables direct)  |
| No "they saw the light patch" | `iNpcDetectionLitSpotSoundLevel = 0`                 |
| LOS wrong (walls / open air)  | `bDebugDrawEnabled = true`, then read §4             |
| Beam stops on thin air        | add its layer to `PASS_THROUGH_LAYERS` (code)        |
| Beam passes through walls     | `sNpcDetectionLosCollisionFilter` (another layer)    |
