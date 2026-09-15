# Immersive Flashlight misc NIF sprites

Source files for the mod's only remaining NIF-based visuals, packed into one `misc` atlas. The config UI
itself is drawn with primitives (its icons live as loose textures in `Textures\ImmersiveFlashlightVR\vrui\`).

- **FRIK menu button** (`btn-flashlight`) — FRIK's external-mod config API only takes a NIF path, so the
  button that opens this mod's config from FRIK's main config menu stays a NIF
  ([src/FlashlightMod.cpp](../../../src/FlashlightMod.cpp)).
- **Activation / debug spheres** (`activation-sphere@<color>-<strength>.nif`, `debug-sphere.nif`, and their
  textures `activation-sphere.png`, `debug-sphere.png`, `debug-sphere@strong.png`) — the 3D zone visuals of
  the body, head and primary-hand activation spheres. Copied from the framework's
  `mod-template/data/resources/common`, except `debug-sphere.png`, halved to 256x128: at 512 wide it cannot
  fit a 512 atlas with the packer's 2px padding, which doubles the atlas to 1024. The default mesh is `misc\activation-sphere@white-medium.nif`
  ([src/Config.cpp](../../../src/Config.cpp)); an `[ImFl_*ActivationSphere]` section's `sSphereNif` picks
  another.

Meshes and textures pair by name, exactly or up to a `@` suffix, so each sphere `.nif` keeps its geometry and
has its UVs remapped into the atlas. See the packer's
[custom mesh override](../../../external/F4VR-CommonFramework/nif-tools/README.md#custom-mesh-override).

## Pack command

```
python external\F4VR-CommonFramework\nif-tools\vrui_atlas.py pack --name misc --texture-subpath ImmersiveFlashlightVR data\Resources\misc --output data\mod
```

Writes `Textures\ImmersiveFlashlightVR\misc.DDS` and one `Meshes\ImmersiveFlashlightVR\misc\<name>.nif` per
sprite or mesh. Renaming a file here means updating the matching path in the source.
