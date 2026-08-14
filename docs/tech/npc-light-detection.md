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
2. **Candidates** — all loaded actors within reach of the beam via the engine's own
   `ProcessLists::GetActorsWithinRangeOfPoint`, filtered: not the player, not dead, not
   companions/teammates (they already know where the player is). Gathered **once per tick** and shared by
   both events in step 4, since both are questions about the same set of actors. The query radius is the
   cone range **plus** the lit-spot witness range (§3.2): the lit patch can sit a full cone range out and
   its witness is that much further from the beam origin again, so a cone-sized query would miss exactly
   the actors the lit-spot path looks for. A point-in-cone test (dot product against the cone's cosine) at chest
   height then tags the actors the beam is touching. How well an NPC already sees the player is deliberately
   **not** part of the filter — that skip was built and then removed; §3.0 records why, and the knowledge
   behind it. Candidates then pass a
   hostility filter — the in-cone ones here, the lit-spot witnesses in §3.2 — when
   `bNpcDetectionOnlyHostileNpcs` is on (default): `Actor::GetHostileToActor(player)`, the engine's own
   faction/relationship + combat-state hostility query (VR `RelocationID(1148686, 2229968)`). It is
   deliberately **not** part of the shared gather pass and runs **last** in each path's own filter chain —
   it's a native call, so only the handful of actors the beam actually
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
     weaker event (`iNpcDetectionLitSpotSoundLevel`, default 40) is placed on the lit surface — but
     only when some NPC could actually have **seen** that patch (§3.2). An
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

### 3.2 The lit spot needs a witness

A detection event is a **sound**. `SetActorsDetectionEvent` posts onto the AI's hearing channel: the engine
attenuates it by distance and nothing else — no facing test, no line of sight, no wall occlusion. Every
actor in range processes it.

That is fine for the direct path, whose recipient is by construction an NPC the beam is shining at with
clear LOS. It was **not** fine for the lit spot, which posted unconditionally wherever the beam terminated:
walking a corridor with the light on broadcast a ~2 Hz noise off the floor, so NPCs behind walls and NPCs
facing the other way came to investigate a light patch they could not possibly have seen. The floor was
making a sound.

So the lit-spot event is now gated on a **witness** — the nearest NPC that could plausibly have noticed the
patch:

1. **within `LIT_SPOT_WITNESS_RANGE_MULT` × `fNpcDetectionMaxRange` of the spot** (0.5, a code constant): a
   bright patch on a wall is a far weaker cue than the beam in your eyes, so it carries a fraction of the
   distance. Not configurable — the gate exists to make the event plausible, and
   `iNpcDetectionLitSpotSoundLevel` remains the knob for how much the event is worth.
2. **facing it**, `SPOTTED_FACING_DEGREES` (90°, the same forward-hemisphere half-angle §3.1 uses on the
   player) via `f4vr::isActorFacing`.
3. **hostile**, if `bNpcDetectionOnlyHostileNpcs` is on — inherited from the shared filter rather than
   hardcoded, so turning that flag off still lets a settler notice the patch on his wall.
4. **with line of sight to the spot**, up to 2 raycasts nearest-first. Facing alone doesn't answer the wall
   case: an NPC can face a wall with the lit patch on the far side of it. The ray runs **spot → NPC**, not
   the reverse, so it starts on the lit surface instead of inside the NPC's own collision capsule (which it
   would hit immediately) — and the target point then sits inside that capsule exactly as the direct path's
   rays expect, so `LOS_TARGET_CAPSULE_MARGIN` reads the same here. It starts `LIT_SPOT_LOS_LIFT` (60 units)
   **above** that surface: on uneven ground — a slope, rubble, a doorstep — the ground the beam is painting
   shadows its own ray and every witness reads as blocked by nothing. Only the sight-line test is lifted;
   the event still goes on the spot itself.

Order matters: 1 and 2 are arithmetic, 3 is the native call, 4 is the raycast, and each stage is only asked
of what survived the cheaper one. When no candidate is anywhere near the beam the termination raycast is
skipped outright, which is the common case of walking a lit corridor alone — the path then costs nothing.

**What this does not fix:** the posted event is still omnidirectional. Once a witness earns it, everyone in
range hears it, wall or no wall. The gate changes the event's *meaning* — "somebody saw your light" rather
than "the floor made a noise" — and their reaction alerting others through a wall is the engine's own
behavior, which is fine. Making the event itself directional isn't possible through this native.

The overlay draws the witness: a magenta line from the lit patch to the NPC that earned the event (the
direct path's line runs from the beam origin instead — there, the beam is what matters), plus the
`spot los <formID>` probe ray in `LOS RAYS`, and an `NPC EVENT` reason that separates "nobody nearby",
"nobody looking at it", and "N looking, no LOS".

### 3.3 Reaching the visual channel — what was measured

Everything above reaches NPCs through the **sound/event** channel: `SetActorsDetectionEvent` makes them
_investigate a location_. It never touches the **visual** channel — the engine's own "how lit is the target"
maths — which is the channel that is actually broken for spot lights (§1), and the one the Papyrus mod's perk
goes after. Reviewing that mod turned up four engine surfaces that reach it, all mapped on VR 1.2.72:

| Symbol                                                                                          | VR            | AddressLib        | Verdict                                                     |
| ----------------------------------------------------------------------------------------------- | ------------- | ----------------- | ----------------------------------------------------------- |
| `AiFormulas::CalculateDetectionFormula(Actor& observer, Actor& target, DetectionFormulaData&)` | `0x14064a890` | 1267425, status 4 | the per-pair update — **both** actors in scope; needs a hook |
| `AiFormulas::CalculateDetectionEventFormula(DetectionFormulaData&)`                             | `0x14064a690` | 1467776, status 4 | scores the events this feature already posts                |
| `AIProcess::SetDetectionModifier(float)`                                                        | `0x140e910d0` | none — raw offset | **tried, measured INERT — see below**                        |
| `AIProcess::ModDetectionModifierTimer(void)`                                                   | `0x140e99a50` | none — raw offset | decays that modifier; moot                                   |

#### `SetDetectionModifier` is inert on VR

It looked like the prize: a plain native, no hook, callable exactly the way the tick already calls
`SetActorsDetectionEvent`. It was implemented behind a debug probe, called in-game with the beam on a
hostile — and moved `f4vr::getDetectionLevel(npc, player)` not at all. That null result is **structural**, not
a tuning failure, established by disassembling the VR binary rather than by guessing:

- The function is exactly what its name says. Its body reads `mov rax,[rcx+0x10]` (`AIProcess::high`), writes
  a constant to `+0x48C` (arming the timer) and the float argument to `+0x488`, and returns. So the address is
  right, the call works, and the value lands.
- An xref over the whole `.text` finds **three** instructions touching `+0x488`: that write, a read in the
  getter at `0x140e91100`, and a read in `AIProcess::SaveGame`. Nothing else.
- And that getter has **zero callers anywhere in the binary** — the check that matters, because a field read
  through an accessor is invisible to a field-only scan.

So `detectionModifier` is written, persisted into the save, and never consulted by the detection code. The
probe and its wrapper were removed again; this section is what remains of them.

#### The VR struct offsets, now confirmed

Read straight out of the instruction stream, which retires the "no VR-verified `HighProcessData` layout
exists" caveat this section used to carry — they match flat-FO4 (`libxse-commonlibf4`) exactly:

| Field                            | Offset  |
| -------------------------------- | ------- |
| `AIProcess::high`                | `+0x10` |
| `HighProcessData::detectionModifier`      | `+0x488` |
| `HighProcessData::detectionModifierTimer` | `+0x48C` |
| `HighProcessData::lightLevel`             | `+0x490` |

#### `lightLevel` is the live input

The neighbouring field is the interesting one — the actor's cached illumination, i.e. the exact quantity §1
says a spot light fails to raise. Unlike `detectionModifier` it is genuinely wired in: its getter
(`0x140e9a3a0`) is called from

- **`GatherDetectionFormulaData`** (`0x140e1c820`, addrlib 421115) — the function that fills the
  `DetectionFormulaData` handed to `CalculateDetectionFormula`,
- `Actor::CalculateNormalizedLightLevel`, and `Actor::CalculateDarkEnoughForTorch` (the NPC "it's dark, equip
  a torch" check),

and it has a setter at `0x140e9a3c0` whose only caller is the engine's own lighting refresh (which also stamps
`lightLevelTimeStamp` at `+0x49C`).

That makes writing the **player's** `lightLevel` the hookless route into the visual channel: the engine's own
detection maths would then see a lit player, with sneak, perks and difficulty all interacting normally —
which is what the Papyrus mod buys with its 1500x perk, except this mod can switch it on only while the beam
is actually on someone. Two caveats: the lighting refresh **overwrites** the field on its own schedule, so the
write has to be re-applied (per frame, not per tick), and the value is the player's own — so like the perk it
is **not per-observer**, which the event path already is. Genuinely per-observer visual detection still means
hooking `CalculateDetectionFormula`.

#### Measured, then shipped

The route works, confirmed in-game before it was built on:

- With the probe reading only, the player's light level sits at **20-40** in an ordinary dim interior and
  climbs to **~150** standing under a street light — so §1's premise holds (a spot light does not raise its
  wearer's illumination) and the field is on an absolute, physically meaningful scale.
- Forcing **200** read back 200 every frame: the write holds, the lighting refresh does not immediately
  reclaim it. At that value enemies located the player from over **1500 units** away — far too much, and the
  reason the shipped ceiling is well below it.

So the beam now feeds the **visual** channel as well, gated by `bNpcDetectionLightLevelEnabled` (default on)
and driven off the direct path only — the same hit that posts the direct event.

**The value.** A flat baseline for merely having the light on, with the beam's own contribution riding on
top of it:

```
strength01 = clamp((beamStrength - STRENGTH_FLOOR) / (STRENGTH_CAP - STRENGTH_FLOOR), 0, 1)
beam       = lerp(fNpcDetectionLightLevelMin, fNpcDetectionLightLevelMax, pow(strength01, fNpcDetectionLightLevelCurve))
applied    = max(fNpcDetectionLightLevelBaseline, beam-after-hold-and-decay)
```

The **baseline** (default 40) applies whenever the light is on, beam on somebody or not — carrying a light
source gives you away by itself. It sits at the top of the vanilla-dim band (20-40), so it clears the "must
exceed the engine's own value" test in a genuinely dark interior while doing nothing in a room with any
light of its own. It should stay under `Min`, or the weak end of the beam ramp becomes indistinguishable
from just holding the flashlight.

It is applied **above** the `bNpcDetectionOnlyWhenSneaking` gate, unlike everything else here: that option
is about whether the beam _posts events_, whereas being lit is a property of holding the light at all, and
suppressing it while standing would mean the flashlight is free to carry as long as you never crouch. With
ticks gated off the beam's own contribution just decays away and the flat baseline remains.

The **beam ramp** is 70 → 220 by default (curve 1.0), tuned in play so that catching someone in the beam
gets them onto you quickly rather than merely making you gradually more noticeable.

The first version normalized against `iNpcDetectionSpottedEventLevel` instead, so that the ceiling was reached
exactly when a hit counted as *spotting* (§3.1). That reads well and played badly. Since the scaled sound
level is `iNpcDetectionDirectSoundLevel × strength` and the spotted threshold defaults to the same 100, the
whole expression collapsed to `clamp(strength, 0, 1)` — and strength reaches 1.0 whenever
`falloff × fade >= 1`, which for the in-hand beam (radius 7000, fade 1.4) is **2000 units out**. Everything
nearer than that pinned to max, and at a 4000-unit `fNpcDetectionMaxRange` even the far edge still sat at 0.6.
The player was near-maximally lit across the entire detection range.

Normalizing over the full `[STRENGTH_FLOOR, STRENGTH_CAP]` band removes that saturation and restores a
gradient across the whole range, which is what made the min/max pair meaningful enough to tune by feel. The
curve exponent then shapes it: 1.0 is a straight line, higher pulls the bright end in toward the player.
Note the floor lifts the whole curve, so raising `Min` trades long-range consequence against how close the
beam has to be before the player is brightly lit.

**Hold, then decay.** Hits only arrive on the throttled tick, so the peak is held at full for one
`iNpcDetectionIntervalMs` and only then fades linearly over `fNpcDetectionLightLevelDecayMs` (default 1000).
Without the hold the value would decay to zero and snap back every tick — a sawtooth that reads in-game as
the beam flickering in and out of effectiveness. Rises are immediate; only leaving fades.

**Two rules that keep the write honest**, both non-obvious enough to be worth stating:

1. **Never write below what the engine reports.** A weak beam computing 60 while the player stands under a
   street light at 150 would, written naively, make them _harder_ to see than doing nothing — the feature
   would silently protect the player in exactly the moments they are most exposed. So a write happens only
   when it exceeds the engine's own value.
2. **The engine's value is only observable through disagreement.** While writing every frame, reading the
   field just returns our own number — a skipped "sample frame" does not help, because nothing reclaims the
   field in that frame either. The reliable signal is a reading that _differs_ from what we last wrote: that
   is the engine having refreshed, and therefore the only genuine sample of the player's true illumination.
   Writing in bursts (only while the beam is actually on someone) is what keeps that sample fresh.

**Handing it back.** Stopping the write is not enough — nothing guarantees the refresh reclaims the field
promptly, and a player left holding a forced value with the flashlight off would stay lit indefinitely. So
every exit (light off, feature disabled, config-UI preview, sneak gate, or the beam simply justifying less
light than the engine already reports) restores the last engine-produced value explicitly.

**What it is not.** The value is the *player's own* illumination, so once raised, **every** NPC sees a lit
player — not only the one the beam is on. That is the same structural limit the Papyrus mod's perk has, and
it is the reason the ceiling is tuned to "as exposed as standing under a lamp" rather than to what a beam in
the face would physically justify. It is also why the lit-spot path deliberately does **not** contribute:
lighting a wall should not blanket-expose the player to a room. Genuinely per-observer visual detection still
means hooking `CalculateDetectionFormula`.

**Debug.** The overlay's `PLAYER LIGHT` row shows the engine's last reading, what the beam is adding, and
whether that is actually being written — which is also how the vanilla scale above was measured in the first
place, by watching the engine's own value while walking under different lighting.


### What was deliberately not done

- **`kModDetectionLight` perk / `HandleEntryPoint` hook** (the brief's "Strategy B"). A runtime perk
  needs an ESP or fragile runtime form construction; a function-entry detour on the variadic
  `BGSEntryPoint::HandleEntryPoint` (VR `0x54b7e0`) is the highest-risk pattern in the mod's
  toolbox, and the hook still can't tell _which observer_ is asking. The detection-event plane
  achieves the directional behavior with zero hooks.

  ~~Making it directional requires hooking the per-pair detection update, which has no published VR
  address.~~ **Corrected:** it does have one —
  `AiFormulas::CalculateDetectionFormula(Actor& observer, Actor& target, DetectionFormulaData&)`,
  AddressLib 1267425 → VR `0x14064a890`, **status 4**, with *both* actors in scope. That removes the
  specific objection above: a detour there needs no cone-set cache and no guess at who is asking, and
  it is a plain non-variadic signature. It remains a hook, so it stays unbuilt — but if a future
  version wants the _visual_ detection channel (stealth-meter pressure without the investigate
  behavior), that is now the entry point, and §3.3 covers the cheaper hookless candidate to try first.
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

Per tick (default 2/s, only while the light is on and no config-UI preview is active): **one** engine
range query shared by both paths (radius widened per step 2, so a few more actors reach the cheap
filters), a dot product per nearby actor, one hostility query per _in-cone_ actor (usually 0-2), at most
6 raycasts (3 direct LOS + 1 lit-spot termination + 2 witness LOS), one detection event write, and —
only on a direct hit — one facing angle plus one more hostility query. The lit-spot path adds its own
arithmetic pass over the same candidates plus a facing angle and a hostility query per actor near the
spot, and skips its raycasts entirely when nobody is nearby (§3.2). Nothing runs per frame; nothing
allocates beyond the query's scrap array and the shared candidate vector. The per-NPC detection-level
query is debug-only (§3.0) and costs nothing with the overlay off.

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
| Lit patch noticed from afar   | `LIT_SPOT_WITNESS_RANGE_MULT` (code, §3.2)           |
| Beam makes you visible too soon | `fNpcDetectionLightLevelCurve` up (§3.3)              |
| Not visible enough up close   | `fNpcDetectionLightLevelMax` up (§3.3)               |
| Light on at all should cost more | `fNpcDetectionLightLevelBaseline` up (§3.3)       |
| LOS wrong (walls / open air)  | `bDebugDrawEnabled = true`, then read §4             |
| Beam stops on thin air        | add its layer to `PASS_THROUGH_LAYERS` (code)        |
| Beam passes through walls     | `sNpcDetectionLosCollisionFilter` (another layer)    |
