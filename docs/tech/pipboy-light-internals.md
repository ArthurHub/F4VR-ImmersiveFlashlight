# Pip-Boy light internals — the Pip-Boy turn-off and the in-place refresh

How the game builds, removes and uses the Pip-Boy light the mod drives; why opening the Pip-Boy turned
it off; and how the mod changes beam values on the light that is already on instead of turning it off
and on. Implemented in `src/LiveLight.{h,cpp}` (refresh), `Utils::updateKeepFlashlightOnInPipboy()`
(Pip-Boy patch) and `FlashlightState::refreshLightValues()`, with the engine entry points in the
framework's `f4vr/F4VROffsets.h`.

Every address is **Fallout4VR.exe 1.2.72** (image base `0x140000000`). They come from static
disassembly of the binary, with names from the VR address library (`fo4_database.csv` in
`Modding-Reference/F4VR/github-repos/gold/fallout_vr_address_library`) where it has one. Names in
_italics_ have no address-library name and are named here for what the code does.

## 1. How the game builds the light

`PlayerCharacter::TogglePipboyLight` (AddressLib 520007, `0x140f27720`, the framework's
`f4vr::togglePipboyLight`) is three steps: `UIUtils::PlayMenuSound("UIPipBoyLightOn"/"Off")`,
`PlayerCharacter::ShowPipboyLight(!on, false)`, and
`BSInputDeviceManager::SetInputDeviceLightState(2, on)` (the gamepad light bar).

`PlayerCharacter::ShowPipboyLight(bool show, bool skipGlowEffects)` (1304102, `0x140f277b0`), which
plays no sound (the toggle's sound is the wrapper's):

- **Form and node.** When a worn item carries the power-armor light keyword (`0x1401f0480`), the
  form is the power-armor light (`0x1401f06c0`; the mod writes its values into `0xB48A0`) and the
  parent is `PlayerNodes::HeadLightParentNode` (`+0x808`). Otherwise it is the default Pip-Boy light
  (`BGSDefaultObjectManager +0xBD0`, colored from the Pip-Boy UI color) under
  `PipboyLightParentNode` (`+0x7C0`). The VR check picking those nodes (`0x140fc19a0`) is `mov al, 1`.
- **Show** calls `TESObjectLIGH::GenDynamic(form, nullptr, node, …, &pipboyLight, …)`
  (`0x140306a50`), which returns the `NiLight` (kept in `niPipboyLight`) and writes the `BSLight`
  (`pipboyLight`). A light that is already on is left alone.
- **Hide** calls `ShadowSceneNode::RemoveLight(pipboyLight)` (`0x1427ea7e0`), detaches the NiLight
  and clears both pointers.
- Without `skipGlowEffects` it also flips the glow meshes of the lamp. Either way it posts a
  `PipboyLightEvent`.

`GenDynamic` makes a new `NiPointLight` named `"<form> PtLight"` and copies the form onto it: diffuse
= color bytes / 255 (negated for `kNegative`), radius = `data.radius` (0.1 when 0) in all three
components, fade = `fade`, attenuation from `attenConstant/Scalar/Exponent`. Everything else goes into
a parameter block for the _ShadowSceneNode light factory_ (`0x1427e9cd0`), which picks the class:

| Light flags (bool4 = false)            | Class                                                              | Size  |
| -------------------------------------- | ------------------------------------------------------------------ | ----- |
| `kSpotShadow`, FOV below a threshold   | `BSShadowFrustumLight` (vtable `0x1430beed8`)                      | 0x200 |
| `kHemiShadow` / `kOmniShadow`          | `BSShadowParabolicLight` (single / dual)                           | 0x1F0 |
| none of those (`kNonShadowSpot`, …)    | `BSLight` (vtable `0x1430b8a40`)                                    | 0x190 |

For a spot (type 6) the factory then sets `cos(FOV / 2)`, builds the light-volume cone with
_`BSLight::SetShape`_, and for a shadow light sets the shadow camera frustum with
_`BSLight::SetCameraFrustum`_ (both in §3). The mod's flags always make a spot: shadows on is
`kSpotShadow` (a `BSShadowFrustumLight`), shadows off is `kNonShadowSpot` (a `BSLight`).

**The game has no refresh.** The values reach a light only while it is built. Even the game's own
settings change (`0x140b8b100`, the Pause Menu handler) refreshes the light with `ShowPipboyLight(false)`
then `ShowPipboyLight(true)`, the same off/on the mod used to do.

### What is read live

Everything below is read on every frame (or every call) from the live objects, so writing it takes
effect on the next frame.

- `BSLight::GetLuminanceAtPoint` (`0x14286ea60`) and the renderer read the NiLight diffuse, fade,
  radius and attenuation, and the spot falloff from `cos(FOV / 2)` (`BSLight +0xA4`) and the falloff
  exponent (`+0xA0`).
- `ShadowSceneNode::UpdateQueuedLight` (`0x1427eb2c0`) tests objects against the NiLight's position
  and radius to rebuild a light's lit-object list, and empties it while the fade is below 0.05.
- The light pass at `0x142846d60` (which also runs `UpdateQueuedLight`) reads the gobo slot (`+0x158`),
  with the light's flag bytes, to pick its shader permutation.
- The `BSShadowFrustumLight` per-frame update (vtable slot 15, `0x142912d00`) re-applies the shadow
  camera's near (`+0x1F0`) and far (`+0x1F4`, or the NiLight radius when 0), but keeps the frustum's
  left/right/top/bottom, which hold the FOV.

What is **not** refreshed after the light is built:

- the light-volume cone (`+0x148`), whose width comes from the FOV and whose height is the radius;
- the shadow camera's FOV;
- the gobo projection camera (`+0x150`, made by `0x14286fc30` on the first frame a light without
  shadows has a gobo): its frustum is set once, from `cos(FOV / 2)` and the radius.

### Offsets

| Object                   | Offset  | Member                                                                |
| ------------------------ | ------- | --------------------------------------------------------------------- |
| `PlayerCharacter`        | `0x10A8`| `NiPointer<BSLight> pipboyLight` (CommonLibF4VR flat `0xC38` + 0x470)  |
|                          | `0x10B0`| `NiPointer<NiLight> niPipboyLight`                                     |
| `NiLight`                | `0x16C` | diffuse `NiColor`                                                     |
|                          | `0x178` | radius `NiPoint3`                                                     |
|                          | `0x184` | fade                                                                  |
|                          | `0x190` | bound copied from the light-volume cone                               |
|                          | `0x1B0` | attenuation constant / scalar / exponent                              |
| `BSLight`                | `0xA0`  | falloff exponent (at least 1)                                         |
|                          | `0xA4`  | `cos(FOV / 2)`                                                        |
|                          | `0xB8`  | `NiPointer<NiLight>`                                                  |
|                          | `0x148` | `NiPointer<BSGeometry>` light-volume cone                             |
|                          | `0x150` | `NiPointer<NiCamera>` gobo projection camera (no shadows)             |
|                          | `0x158` | `NiPointer<NiTexture>` gobo                                            |
|                          | `0x172` | dynamic (per-frame near/far on a shadow light)                        |
|                          | `0x180` | type (6 = spot)                                                       |
| `BSShadowFrustumLight`   | `0x198` | shadow render data, its shadow `NiCamera*` at `+0x40`                 |
|                          | `0x1F0` | shadow near                                                           |
|                          | `0x1F4` | shadow far                                                            |
| `PipboyManager`          | `0x1F5` | `wasPipboyLightActive` (flat `0x1E5`)                                  |

CommonLibF4VR now carries these: `NiLight`, `NiPointLight` and `BSLight` are defined with their VR
layouts, and `PlayerCharacter` / `PipboyManager` assert the offsets above. `PipboyManager`'s members
sit 0x10 higher on VR because VR's `Inventory3DManager` is 0x10 larger (a ref-counted pointer and a
byte after `itemBase`). The `PlayerCharacter` offsets are also `static_assert`ed in `LiveLight.cpp`.

## 2. Opening the Pip-Boy turns the light off

`PipboyManager::InitPipboy` (`0x140c34780`, called from `PipboyManager::OnPipboyOpenAnim`) ends with:

```
140c34a9d  call PowerArmor::QActorInPowerArmor
140c34aa2  test al, al
140c34aa4  je   140c34aaa
140c34aa6  xor  eax, eax                           ; in power armor: the light stays on
140c34aa8  jmp  140c34ab6
140c34aaa  mov  rcx, [PlayerCharacter singleton]
140c34ab1  call PlayerCharacter::IsPipboyLightOn   ; E8 DA 2C 2F 00
140c34ab6  mov  [rsi+1F5h], al                     ; wasPipboyLightActive
140c34abc  test al, al
140c34abe  je   140c34ad1
           ...  PlayerCharacter::ShowPipboyLight(false, true)
```

The Pip-Boy close (`0x140c337a0`, called from `PipboyManager::PlayPipboyCloseAnim` among others) shows
the light again with `ShowPipboyLight(true, false)` when `wasPipboyLightActive` is set, then clears
it. Nothing else on the open path touches the light. The other `ShowPipboyLight` callers are
elsewhere: two player-state changes that turn a lit light off with the toggle's sound (`0x140f00680`,
next to power-armor race code, and `0x140f01020`), a restore of a light that couldn't be attached yet
(`0x140f16a70`), a turn-off in a player reset (`0x140f171a0`), and the Pause Menu refresh above.

**The patch** (`bKeepFlashlightOnInPipboy`, default on) writes `31 C0 90 90 90` (`xor eax, eax`
plus padding) over the call at `0x140c34ab1`. Out of power armor that is exactly what the power-armor
branch already does: `wasPipboyLightActive` stays false, the light isn't hidden on open, and the close
has nothing to restore. `Utils::updateKeepFlashlightOnInPipboy()` checks for the expected bytes before
writing either way, and restores the original call when the option is turned off.

## 3. Refreshing the light in place

`FlashlightState::refreshLightValues(mode)` writes the config onto the form (`setLightValues()`) and
stops there when no value changed or the light is off. An on light is re-created by default (see
"When it re-creates" below). Location changes and INI hot-reload take that path: it only runs engine
code, and a light that moves hides the swap. A location change also plays the light-on sound the
vanilla toggle plays (`UIUtils::PlayMenuSound("UIPipBoyLightOn")`, 1227993, `0x14133d7d0`); a
hot-reload stays silent. Only the beam screen's tuning passes
`LightRefreshMode::InPlace`, because the in-place refresh swaps objects the renderer is using. It
confines that risk to someone standing in the config UI. `LiveLight::refresh(form)` then repeats the
build-time copy on the existing objects:

1. **NiLight**: diffuse, radius (all three components) and fade, exactly as `GenDynamic` converts them.
2. **Spot cone**: `BSLight +0xA4 = cos(FOV / 2)`.
3. **Light volume**: _`BSLight::SetShape`_ (`0x14286f9b0`) with the factory's arguments:
   `width = sqrt(2 * tan²(half FOV)) * 1.22077` (half FOV clamped to [1°, 160°]),
   `baseRadius = width * radius`, `height = radius`, `1, 1, 1`. It returns early for the type the light
   already has, so the type (`+0x180`) is cleared first. The cone is generated under
   `BSGraphics::Renderer::TryLock` (`TryEnterCriticalSection`); when the lock is busy the old cone
   stays, and the refresh is retried on the next frame, up to 30 times. Without the rebuild, a wider
   or longer beam is cut off at the old cone's edge.
4. **Gobo**: the texture from `Utils::loadGoboTexture()` (the same cached object `GenDynamic` gets from
   `BSShaderManager::GetTexture`), with `NiTexture::flags` bit `0x40` cleared as `GenDynamic` does.
5. **Cameras**, with _`BSLight::SetCameraFrustum`_ (`0x14286f180`): on a shadow light the shadow camera
   (FOV, near = `nearDistance`) plus `+0x1F0` / `+0x1F4` for the per-frame update. On a light without
   shadows, the gobo projection camera when it exists (near 1, as `0x14286fc30` makes it).

The replaced cone and gobo aren't released on the spot. The renderer may still hold the pointer the
light had, and for how long isn't known, so `LiveLight` keeps a reference for 60 frames. Everything
runs on the game thread, including hot-reload, which `Flashlight` applies on the next frame instead of
from the file watcher's thread.

**When it re-creates.** Every change outside beam tuning. While tuning, a change of the light flags
(shadows on/off, or any other bit) needs a different kind of light, so it is built anew. The same
goes for a light that isn't a spot or isn't the class its flags ask for.
`LiveLight::recreate()` calls `ShowPipboyLight(false, true)` and `ShowPipboyLight(true, true)`. That
is the old double toggle without the two menu sounds, the light-bar change and the glow-mesh flip.
It also drops a pending cone retry, since the new light is built from the form as it is now.

## 4. Verification

Statically: every offset and signature above against the disassembly; the `PlayerCharacter` offsets
at compile time; the patch site bytes; the factory constants (1.22077, 1/255, π/180), bit for bit.

In game (VR 1.2.72): both features were tested and work. The light stays on through the Pip-Boy, and
beam changes apply to the light that is on. Re-check these after touching either:

- Out of power armor, the light stays on while the Pip-Boy is open and after it closes. Check that a
  head-mounted light doesn't wash out the Pip-Boy screen.
- In beam config, intensity, color, radius, FOV and gobo change without a blink, with shadows on and
  off. Widening the FOV or the radius doesn't leave a hard edge where the old beam ended.
- Toggling shadows on the misc screen re-creates the light without the toggle sound.
- **Not tested in game yet:** moving the light between locations with different beam values
  re-creates it with the light-on sound only; saving an INI beam change re-creates it silently. An INI
  save that changes no beam value, or a move between locations with the same values, leaves the light
  alone.
- The log has no `Renderer kept refusing to rebuild the flashlight light volume`.
