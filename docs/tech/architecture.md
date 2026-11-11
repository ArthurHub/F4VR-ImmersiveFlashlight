# Architecture details

How each feature behaves, the rules behind it, and the traps in it. [CLAUDE.md](../../CLAUDE.md) has the overview and the component map; the NPC detection and the live light have their own docs ([npc-light-detection.md](npc-light-detection.md), [pipboy-light-internals.md](pipboy-light-internals.md)).

## Location model

- `FlashlightLocation` is where the light is at runtime: `OnHead`, `OnPAHead`, `InOffhand`, `InPrimaryHand`, `OnWeapon`. `FlashlightConfigLocation` is what the player chose: `OnHead`, `InOffhand`, `InPrimaryHand`. Each location has its own beam values (fade, radius, FOV, color, gobo, plus the head's tilt).
- The choice is kept per power-armor state (`iFlashlightLocation` / `iFlashlightLocationInPA`). `FlashlightState::getActiveFlashlightConfigLocation()` picks it by `f4vr::isInPowerArmor()`, and `switchFlashlightConfigLocation()` saves only the current state's. The per-frame `refreshFlashlightLocation()` moves the light on a PA transition.
- `resolveFlashlightLocation()` resolves the runtime location in two steps. The config is never rewritten by it, so the light returns to the chosen place once that is available again.
  - **Requested:** `getRequestedFlashlightLocation()` maps the choice onto the game state (the bullets below).
  - **Fallback:** with `bRestrictionFallback` on, when `isLocationAvailable()` rules the requested location out, the first available of head, `InOffhand`, `InPrimaryHand`, `OnWeapon` is taken. The head needs `isHeadFlashlightAllowed()` (`OnPAHead` always passes), a hand must be free of the weapon, and the weapon needs `RestrictionHandler::canWeaponCarryFlashlight()`.
  - **Nowhere:** with none available (with `bRestrictionFallback` off, as soon as the requested location is ruled out), `FlashlightState::hasAvailableLocation()` turns false and the light keeps its place, so it doesn't move just to be turned off. `RestrictionHandler::enforceRestrictions()` then turns it off.
  - **`InPrimaryHand`:** an empty hand is `InPrimaryHand`, a drawn weapon is `OnWeapon`. With the weapon-flashlight requirement off that includes melee, at the barrel offset. `f4vr::isMeleeWeaponDrawn()` is true only while melee is drawn.
  - **`InOffhand`:** `InOffhand`, but a two-handed grip (`WeaponGripHandler::isTwoHandedGripActive()`, FRIK or ROCK) routes it to `OnWeapon`.
  - **Weapon in the offhand (ROCK):** carried one-handed there, the primary hand is free, so either hand config resolves to `InPrimaryHand` (checked before the hand branches). Two-handed takes the normal routing.
  - **Part carry (ROCK):** the offhand holds a weapon it can't fire (`isWeaponCarriedByOffhand()`). `InOffhand` then takes the two-handed routing. `InPrimaryHand` stays `OnWeapon`, unless the light was put into the free hand during the carry: `switchFlashlightConfigLocation(InPrimaryHand)` sets `FlashlightState::_heldInFreePrimaryHand`, which resolves to `InPrimaryHand` until the carry ends.
- While the offhand holds the weapon (`isOffhandHoldingWeapon()`), the gestures change hands: the body grab feeds only the primary binding (while `isPrimaryHandFreeOfWeapon()`), the free primary hand takes over the head gesture, and the primary-hand sphere and the two-handed toggle are inert.
- `FlashlightState::setFlashlightRuntimeLocationOverride()` pins a location while the beam tuning previews it. `isRuntimeLocationOverrideActive()` makes the restrictions and the NPC detection stand aside.

## Grip style

- The hand-held locations have a `FlashlightGripStyle`: `Forward` (thumb up) or `Overhand` (ice-pick, controller flipped around its barrel). `FlashlightState::refreshGripStyle()` picks it each frame from `iFlashlightGripMode`. `Auto` measures the wand's +Z against world up with hysteresis (`fFlashlightGripOverhandTiltDegrees`, `fFlashlightGripHysteresisDegrees`), and the other two modes lock the style.
- Controller-direction movement follows the offhand controller's nose, which an overhand grip can point backward. While the light is on in the offhand (and no gesture-blocking menu is open), `Flashlight::alignWandMovementToBeam()` passes the beam's +X to the framework's `f4vr::WandMovement::alignTo()` each frame. That inverts the offhand stick the game reads while the controller points more than 90° from the beam, whatever the grip style. Not calling it ends the correction a frame later. `bFlipReversedWandMovement` turns it off.
- Each style has its own light transform (`tFlashlightIn{Offhand,PrimaryHand}Transform[Overhand]`, wand-relative, used only while the model isn't shown), mesh transform (`tFlashlightMeshTransform[Overhand]`) and FRIK hand pose (`hFlashlightHandPose[Overhand]`). The light transform is resolved each frame. `FlashlightMesh` re-applies the mesh transform and the pose on a style change, with no reattach.
- The mesh transforms and the poses have a PA variant (`...PA`, defaulting to the non-PA value), as gauntlets are sized differently. The light transform and the beam values have none. The skeleton swaps on a PA transition, so `Flashlight::handlePowerArmorTransition()` invalidates the meshes to reattach with the PA values.

## In-hand model and beam glow

- `FlashlightMesh` hangs the model under the wand node. With it shown, the light is rooted at the model's lens (`Flashlight_beam_attach`, +X out of the lamp, via `getLightAnchorNode()`), so it leaves the model along the glow. The lens disc (`Flashlight_lens_FX`) is tinted with the beam color.
- `BeamGlowMesh` is the visible cone, shared with the weapon lamp. A node transform can't change a cone's angle, so the angle comes from 15 mesh variants (`flashlight-beam-<N>deg.nif`, N = 20..90 in steps of 5), picked from the beam FOV less `fBeamGlowFovOffset`. Each variant is cloned lazily, its materials made private before the first attach, and swapped in place. Color and brightness (`fBeamGlowBaseIntensity + fade x fBeamGlowIntensity`, hidden at <= 0) are written live on the material. Settings are in `[ImFl_BeamGlow]`.
- `OnWeaponBeamMesh` attaches a glow as a sibling of the weapon's lamp node, with `tWeaponFlashlightMountTransform`, while the light is on at `OnWeapon` and a lamp was detected. It isn't mirrored for handedness.

## Body stow and grab

- `BodyFlashlightMesh` shows a beam-less clone (`Flashlight_lamp_FX` hidden) at the chest bone. It is parented under the VR primary-hand UI attach node and placed each frame (see the scene-graph rule in CLAUDE.md). It shows except while the light is held in a hand: off, on the head and on the weapon all leave it stowed.
- `tFlashlightBodyTransform` (+ `PA`) and the grab zone are authored right-handed. `Flashlight::mirrorZoneIfNeeded()` mirrors them (Z translate + heading/roll) for a left-handed player before they reach the sphere, which doesn't mirror. The zone is measured from the stowed model: only the model's translate shifts it.
- `Flashlight::checkBodyGrab()`: in the zone, the hand's binding (owner `ImFl_BodyGrab`) returns the light (off) when it's already in that hand, or grabs it to that hand from any other state. A hand holding a drawn weapon can do neither. `BODY_GRAB_COOLDOWN_MS` stops a grab-then-return.

## Activation spheres

- Three `f4vr::WandActivationSphere` zones (framework, see its [f4vr README](../../external/F4VR-CommonFramework/src/f4vr/README.md#activation-spheres)) are configured by `Config::bodyActivation` / `headActivation` / `primaryHandActivation`. Each comes from its `[ImFl_*ActivationSphere]` section via `ConfigBase::loadWandActivationConfig`:
  - the zone: `tZone`, plus an optional `tZonePA`
  - the bindings: `kPrimaryBinding` / `kSecondaryBinding`, with `suppress` in the binding
  - the haptics: `sEntryHaptic`, `sPrimaryHaptic` / `sSecondaryHaptic`
  - the two visuals: an icon (`sShowIcon`, `sIcon`, `cIconColor`, `fIconSize`) and a sphere (`sShowSphere`, `sSphereStyle` + overrides, `sSphereOrientation`, `fSphereScale`, which is visual only)
- `bShowAllActivationSpheres` forces every sphere visual on, at its true zone size.
- A binding with no action in the current state is fed as `DisabledBinding`, so its button is suppressed, and its entry haptic plays, only while the gesture is available. Two bindings on one sphere are told apart by `InputBinding::operator==` against the fed value.
- **Gating:** every gesture is inert while a game menu blocks it (`Utils::findOpenGestureBlockingMenu()`, flag-based, with Pip-Boy / holotape / scope / VATS allowed) or while there is no flashlight yet (`RestrictionHandler::isFlashlightAvailable()`: Pip-Boy worn or in PA). Before the Pip-Boy is worn the engine shows the vanilla wrist glow the mod can't drive, so the body model hides and the restrictions stand aside. A found Pip-Boy stays latched until `RestrictionHandler::invalidate()`.

### Head activation

- `checkHeadActivation()`: the offhand near the HMD (`tZone` off `hmdNode`, not mirrored, no PA variant).
  - `kPrimaryBinding` (default tap trigger): on to the head from off, switch to the head from a hand, or off when already on the head. It switches the location before turning the light on, so the head beam values are on the light as it comes on.
  - `kSecondaryBinding` (default long-press trigger): pulls a head-mounted light to the hand that fired it, and is fed only while the light is on the head.
- With the offhand holding the weapon alone, both bindings move to the free primary hand (`onOtherHand()`, through `WeaponGripHandler::adjustBindingForInputRemap()`).
- The headgear requirement gates the tap's put-on-head, while a head light can still be turned off.

### Primary-hand activation

- `checkPrimaryHandActivation()`: the offhand near the primary hand, with offhand bindings tested against a primary-hand zone. It turns the light on only onto the weapon, never into an empty hand.
- **Tap:**
  - offhand: to the primary hand, or onto a drawn ranged weapon; inert with melee
  - on the weapon: off
  - in the primary hand: back to the offhand
  - on the head, with a ranged weapon drawn: onto the weapon
  - off: on at the weapon, only when `isWeaponFlashlightAllowed()`
- Moving to the primary hand and moving onto the weapon are the same call, `switchFlashlightConfigLocation(InPrimaryHand)`. The location model resolves which it is.
- **Long-press:** pulls the on-weapon light to the offhand.
- With `bWeaponFlashlightAnchorPrimaryHandSphereToMesh` and a detected lamp (which also needs `bWeaponFlashlightMountBeamToMesh`), the zone is anchored to the lamp's parent node, with the mount offset, and only `tZone`'s scale is read.
- It is inert while the offhand holds or supports the weapon, as a lamp-anchored zone would swallow the offhand's button on the foregrip.

### Two-handed weapon toggle

- `checkWeaponFlashlightToggle()` covers the two-handed hold, where the offhand can't reach the primary-hand sphere. It is zone-less: it reads `kToggleWeaponFlashlightTwoHandedBinding` (default offhand tap trigger) directly, and toggles the weapon light on/off without switching the location.
- It is active only during a two-handed grip with the offhand not firing, at `OnWeapon`, with `isWeaponFlashlightAllowed()`, and while no sphere claims the same input this frame (`isSuppressing(binding)`).
- It suppresses its button under `ImFl_WeaponToggle`, issuing suppress/release only on edges (`_weaponToggleSuppressedBinding`). `onGameSessionLoaded()` clears that, as the framework resets all suppression before it.

## Restrictions

`RestrictionHandler` holds the gameplay gates, which the location resolve reads to move the light elsewhere (see [Location model](#location-model)). `enforceRestrictions()` runs after `refreshFlashlightLocation()` and turns the light off only when no location is available: bare-headed (by the requirement) with both hands on a weapon that can't carry it, or with `bRestrictionFallback` off, its own location ruled out. `getNoLocationMessage()` names what ruled it out. The player hears one sound either way, told apart by `_lightWasOn` (the previous frame). A light that loses its place while on turns off with the vanilla off sound and no message, as the player just caused it. A light just turned on there (the vanilla toggle, since the gestures don't turn it on into nowhere) already played the on sound. It goes off silently (`Utils::turnFlashlightOff(false)`, `ShowPipboyLight(false)` without the toggle's sound), and a HUD message says why, at most every 3 s. It stands aside during the beam-tuning preview.

### Headgear (head, out of PA only)

- `iFlashlightHeadgearRequirement`: `None`, `AnyHeadGear` (any slot-30 `ARMO`, `biped->object[0]`), or `Immersive` (light-capable headgear).
- The `Immersive` rule is: worn and not denied, and (allowed or has a keyword). `aHeadLightKeywords` holds editor IDs, matched against `BGSKeyword::formEditorID`. `aHeadLightAllowList` / `aHeadLightDenyList` hold `localFormID|plugin` entries, resolved by `LookupForm`, which skips absent plugins.
- `resolveForms()` resolves the lists once, at game load and on reload. In PA the head is always allowed.

### Weapon flashlight mesh

- `iWeaponFlashlightMeshRequired`: `Disabled`, `Enabled`, or `AutoDetect` (on when a plugin of `aWeaponFlashlightAutoDetectPlugins` is loaded). It is resolved to a bool in `resolveForms()`, read through `isWeaponFlashlightMeshRequired()`, and applies in and out of PA.
- **Detection:** `onFrameUpdate()` walks the weapon 3D for the first node named in `aWeaponFlashlightMeshNodes` (NIF node names, default `AddOnNode`), only when `f4vr::EquippedWeaponHandler` reports a weapon change. There is no rescan, so a lamp that attaches a frame late waits for the next change. It returns whether the weapon changed, so `Flashlight` can invalidate `OnWeaponBeamMesh`.
- **Invalidation:** `invalidate()` drops the cached node and re-resolves. It is called at game load, session load, a PA transition and a config reload.
- **Beam rooting:** with `bWeaponFlashlightMountBeamToMesh`, `getOnWeaponFlashlightMeshNode()` returns the lamp node, and the light is rooted there with `tWeaponFlashlightMountTransform` instead of the generic barrel offset.
- **Weapon change:** the lamp scan runs after the frame's location refresh, so `RestrictionHandler::onFrameUpdate()` re-resolves the location on a weapon change before enforcing. Otherwise a light on a weapon swapped for one without a lamp would be resolved against the previous weapon and turned off instead of falling back.

## Light switches

- **`bDisableVanillaFlashlightToggle`:** `Utils::updateVanillaFlashlightToggleDisabled()` sets `fPipboyLightDelay:Controls` to 99, caching the original once and restoring it when turned off. It is idempotent, and applied at session load and on reload.
- **`bKeepFlashlightOnInPipboy`:** `Utils::updateKeepFlashlightOnInPipboy()` patches the `IsPipboyLightOn()` call in `PipboyManager::InitPipboy` to `xor eax, eax`, as power armor already does. The bytes are checked before each write. See [pipboy-light-internals.md](pipboy-light-internals.md) §2.
- **Light refresh:** `FlashlightState::refreshLightValues(mode)` writes the beam values onto the light form, which is all an off light needs, and does nothing when nothing changed. An on light is re-created (`LiveLight::recreate()`, the engine's hide/show without its sounds), with `RecreateWithSound` for a location change made with a gesture (`switchFlashlightConfigLocation()`). Automatic location changes (a grip, a drawn weapon, a restriction fallback) are silent, as they follow what the player does anyway. `InPlace` (beam tuning) refreshes the live light (`LiveLight::refresh()`), and falls back to re-creating it when the light flags changed. See [pipboy-light-internals.md](pipboy-light-internals.md) §3.

## NPC light detection

`NpcDetectionHandler` makes NPCs notice the beam, which vanilla ignores. On a throttled tick it posts a detection event near the nearest NPC in the cone with line of sight, or on the lit spot when some NPC faces it. A bright hit on a hostile who faces the player puts the event on the player (instant spotting). Separately, every frame, it raises the player's light level. The design, the VR addresses, the traps and the tuning are in [npc-light-detection.md](npc-light-detection.md).

## Mods Config FO4 VR

- The config spec, `data/mod/ModsConfigFO4/Config/ImmersiveFlashlightVR/spec.json`, edits the TOML live. Its `live` settings are previewed by the framework's `ConfigPreviews` as session overrides of the config, which reload the values (see `external/F4VR-CommonFramework/src/modsconfig/README.md`).
- `FlashlightMod::registerModsConfigEntry()` adds:
  - the `shadowMapResolution` state, which the Shadows warning reads
  - the `openBeamConfig` function, which opens `BeamScreen` and minimizes the menu; the menu is restored as the screen closes
- `BeamScreen` saves nothing itself. Each tuned value, and Reset's defaults, goes to the menu with `setPendingValue` by setting id (`<key>:<section>`, from `Config::getFlashlightValueSetting()`), so the menu's Save writes them and its Discard takes them back.
- The screen sets a value in the config before it hands it over, and refreshes the light itself. The value comes back as a preview marked `handedOver`, which `ConfigPreviews` records as an override with no reload. A refused value is not kept: the next load of the config drops it, and the player is told once.
- Button handlers fire mid-`UIManager` iteration, so Exit only sets `_closeRequested`, and `onFrameUpdate()` closes.

## Config hot-reload

- Every load of the typed values ends in `Config::loadConfigInternal()`, which raises `_valuesReloaded`. That covers the file watcher, a session override set or cleared (Mods Config previews, devbench `set` / `clear`) and `reload()`.
- `Flashlight::onFrameUpdate()` consumes it and applies the values on the game thread (`applyConfigChange()`): restriction caches, the vanilla-toggle and Pip-Boy patches, the location and the light values.
- The constructor consumes it once. The mod's own saves (`Config::setFlashlightLocation`) don't reload.

## ROCK integration (`WeaponGripHandler`)

- It answers which hand holds the weapon, so nothing else names FRIK or ROCK:
  - `isTwoHandedGripActive()`: FRIK or ROCK
  - `isWeaponInOffhand()`: ROCK's firing hand is the offhand
  - `isWeaponCarriedByOffhand()`: WeaponParts `gripKind == PartCarry`
  - and the derived `isOffhandHoldingWeapon()` / `isPrimaryHandFreeOfWeapon()`
- **API:** `initialize()` discovers ROCK, registers an owner, and binds Weapon, WeaponParts and Input as Read. Interfaces are negotiated forward-compatibly by `queryRockInterface()` (major 1, minimum minor 0, the table extent up to the last slot called), so ROCK appending calls doesn't break it. The headers in `src/api/ROCK/` are copied as-is from `brunocatani/RPS_SDK` and excluded from clang-format.
- **Threading:** it samples once per frame on the game thread, where ROCK answers.
- **Weapon pose:** ROCK solves the two-handed weapon pose after this mod's frame. The handler forwards ROCK's `AfterRock` phase callback, and `Flashlight::onWeaponTransformFinalized()` re-applies the on-weapon light then.
- **Trigger remap:** while the physical left hand fires, ROCK presents its trigger on the right controller. A `VRControllers.setControllerStateAdjuster()` restores both triggers from ROCK's raw input. `adjustBindingForInputRemap()` clears `suppress` on trigger bindings then, as ROCK already hides the free hand's trigger.
- **Known limitation:** during left-hand firing ROCK retargets this mod's right-controller haptics to the left controller. It is to be fixed in ROCK.

## devbench tool

The framework registers the `immersiveflashlightvr` tool with its generic actions (`health`, `state`, `config` / `set` / `clear` / `overrides`, `perf`). [src/devbench/FlashlightDevBench.cpp](../../src/devbench/FlashlightDevBench.cpp) adds:

- **State:** `FlashlightDevState`, captured after each frame while armed. Every flag and label also emits a `<group>.<name>` event once a new value holds for 3 frames.
- **Events, always on:** `gesture` (from each gesture callback) and `restriction.lightOff`.
- **Actions:** `light` (`on|off|toggle`, `location`), `gesture`, `zones`, `ui` (`open|close`) and `restrictions` (what is allowed, and why).
- **`gesture`** fires `bodyGrab` (`hand`), `head` / `primaryHand` (`press`: `tap|long`) or `weaponToggle` through `Flashlight::simulateGesture()`. That runs the bindings the frame would feed (`get*Bindings()`, `isWeaponToggleActive()`) and the gesture's own handler, so only the zone and the button press are skipped. An inert gesture answers `fired: false` with the reason.
- **Node names, for the generic `nodes` action:** `ImmersiveFlashlight`, `ImmersiveFlashlightBody`, `ImmersiveFlashlightWeaponBeam`.

Inside `namespace ImFl`, `devbench::` is the mod's, and the framework's is `f4cf::devbench::`.

## Perf sites

- Each per-frame component opens an `F4CF_PERF_FUNCTION()` / `F4CF_PERF_SCOPE()` at the top of its frame function. Rarer work that can hitch a frame has its own site: mesh attach, beam-glow variant load, lamp scan, `LiveLight`, `refreshLightValues`, the NPC tick and its raycasts, and the debug draw.
- `onWeaponTransformFinalized` runs in ROCK's callback, so it is a root of its own.
- Recording is off until devbench `perf` or `perf_reset` in `[Debug] aDumpDataOnce`. The `tracy` preset makes each site a Tracy zone. See `external/F4VR-CommonFramework/src/perf/README.md`.
