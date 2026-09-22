# Pip-Boy light internals — the Pip-Boy turn-off

How the game shows and hides the Pip-Boy light the mod drives, and why opening the Pip-Boy turned it
off. Implemented in `Utils::updateKeepFlashlightOnInPipboy()`, with the patch site in the framework's
`f4vr/F4VROffsets.h`.

Every address is **Fallout4VR.exe 1.2.72** (image base `0x140000000`). They come from static
disassembly of the binary, with names from the VR address library (`fo4_database.csv` in
`Modding-Reference/F4VR/github-repos/gold/fallout_vr_address_library`) where it has one. Names in
_italics_ have no address-library name and are named here for what the code does.

## 1. How the game shows and hides the light

`PlayerCharacter::TogglePipboyLight` (AddressLib 520007, `0x140f27720`, the framework's
`f4vr::togglePipboyLight`) is three steps: `UIUtils::PlayMenuSound("UIPipBoyLightOn"/"Off")`,
`PlayerCharacter::ShowPipboyLight(!on, false)`, and
`BSInputDeviceManager::SetInputDeviceLightState(2, on)` (the gamepad light bar).

`PlayerCharacter::ShowPipboyLight(bool show, bool skipEffects)` (1304102, `0x140f277b0`):

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
- Without `skipEffects` it also flips the glow meshes of the lamp. Either way it posts a
  `PipboyLightEvent`.

`PipboyManager::wasPipboyLightActive` is at `0x1F5` on VR (flat `0x1E5`). `PipboyManager`'s members
sit 0x10 higher on VR because VR's `Inventory3DManager` is 0x10 larger (a ref-counted pointer and a
byte after `itemBase`); CommonLibF4VR asserts the VR offset.

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
(`0x140f16a70`), a turn-off in a player reset (`0x140f171a0`), and the Pause Menu's settings change
(`0x140b8b100`), which hides and shows the light to apply new values.

**The patch** (`bKeepFlashlightOnInPipboy`, default on) writes `31 C0 90 90 90` (`xor eax, eax`
plus padding) over the call at `0x140c34ab1`. Out of power armor that is exactly what the power-armor
branch already does: `wasPipboyLightActive` stays false, the light isn't hidden on open, and the close
has nothing to restore. `Utils::updateKeepFlashlightOnInPipboy()` checks for the expected bytes before
writing either way, and restores the original call when the option is turned off.

## 3. Verification

Statically: the call site, its bytes and the `wasPipboyLightActive` offset against the disassembly.

In game (VR 1.2.72): tested and works. Re-check this after touching it:

- Out of power armor, the light stays on while the Pip-Boy is open and after it closes. Check that a
  head-mounted light doesn't wash out the Pip-Boy screen.
