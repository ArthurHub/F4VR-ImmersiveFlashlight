# Shared common-UI sprites (`ui-common` atlas)

The framework's shared button sprites, reused by this mod's VR config UI — one PNG per
button. Unlike the mod-specific [`ui-config-main`](../ui-config-main/) atlas, **the source PNGs
do not live in this folder**: they are the framework's shared set in
`external\F4VR-CommonFramework\mod-template\data\resources\common`.

## Pack command

Bin-packs every PNG in the framework's shared `common` folder into a single `ui-common.DDS`
atlas plus one `<button>.nif` per sprite, written into this mod's deployable tree
(`Textures\ImmersiveFlashlightVR\ui-common.DDS` +
`Meshes\ImmersiveFlashlightVR\ui-common\<button>.nif`):

```
python external\F4VR-CommonFramework\nif-tools\vrui_atlas.py pack --name ui-common --texture-subpath ImmersiveFlashlightVR external\F4VR-CommonFramework\mod-template\data\resources\common --output data\mod
```

`--texture-subpath ImmersiveFlashlightVR` is both the in-game texture path baked into every nif
(`Textures\ImmersiveFlashlightVR\ui-common.DDS`) and the subfolder the files are written under —
the atlas to `Textures\ImmersiveFlashlightVR\`, the nifs to `Meshes\ImmersiveFlashlightVR\ui-common\`.
It **must** be the mod name (`ImmersiveFlashlightVR`, from `Version::PROJECT`), because the code
loads these as bare `ui-common\<button>.nif` paths that the framework resolves under
`Data\Meshes\<ModName>\` (see
[F4VRUtils.cpp](../../../external/F4VR-CommonFramework/src/f4vr/F4VRUtils.cpp)) — the same subpath
the [`ui-config-main`](../ui-config-main/) atlas uses. Full options and the reverse (`unpack`) are
in [nif-tools/README.md](../../../external/F4VR-CommonFramework/nif-tools/README.md).
