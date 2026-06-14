# Immersive Flashlight config-UI sprites

Source PNGs for the in-game VR configuration UI. Each image is one sprite in the
`ui-config-main` atlas:

- **Location toggles** (`btn-flashlight-on-head`, `btn-flashlight-on-pa-head`,
  `btn-flashlight-in-hand`, `btn-flashlight-on-weapon`) — pick which mount the beam settings apply to.
- **Beam controls** (`btn-beam-tuning`, `btn-switch-gobo`, `btn-switch-color`,
  `btn-flashlight-shadows`) — tune intensity/radius/FOV, cycle gobo and color presets, toggle shadows.
- **Messages** (`msg-main`, `msg-beam-tuning`) — the help panels shown below the buttons.
- **Title** (`title`) — the config panel header.
- **FRIK menu icon** (`btn-flashlight`) — the button registered in FRIK's main config menu to open this UI.

These feed `FlashlightConfigMode::createMainConfigUI()` in
[src/FlashlightConfigMode.cpp](../../../src/FlashlightConfigMode.cpp); the FRIK menu icon is
registered in [src/ImmersiveFlashlight.cpp](../../../src/ImmersiveFlashlight.cpp). Edit the art
here, then re-run the pack command below. (Shared buttons like Save/Reset/Exit live in the
separate `ui-common` atlas.)

## Pack command

Bin-packs every PNG in this folder into a single `ui-config-main.DDS` atlas plus one
`<sprite>.nif` per image, written into the deployable mod tree
(`Textures\ImmersiveFlashlightVR\ui-config-main.DDS` +
`Meshes\ImmersiveFlashlightVR\ui-config-main\<sprite>.nif`):

```
python external\F4VR-CommonFramework\nif-tools\vrui_atlas.py pack --name ui-config-main --texture-subpath ImmersiveFlashlightVR data/Resources/ui-config-main --output data\mod
```

Each PNG's file name becomes its `.nif` name — the same name passed to `UIButton` / `UIWidget`
in code (e.g. `btn-flashlight-on-head.png` → `ImmersiveFlashlightVR\ui-config-main\btn-flashlight-on-head.nif`).
So renaming a PNG here means updating the matching string in the source. Full options and the
reverse (`unpack`) are in
[nif-tools/README.md](../../../external/F4VR-CommonFramework/nif-tools/README.md).
