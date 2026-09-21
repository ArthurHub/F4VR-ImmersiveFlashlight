# Immersive Flashlight misc NIF sprites

Source files for the mod's only remaining NIF-based visual, packed into one `misc` atlas. The config UI
itself is drawn with primitives (its icons live as loose textures in `Textures\ImmersiveFlashlightVR\vrui\`).

- **FRIK menu button** (`btn-flashlight`) — FRIK's external-mod config API only takes a NIF path, so the
  button that opens this mod's config from FRIK's main config menu stays a NIF
  ([src/FlashlightMod.cpp](../../../src/FlashlightMod.cpp)).

The activation / debug sphere visuals are not in this atlas: they are the framework's standalone meshes and
textures, copied unchanged from its `mod-template` into `Meshes\ImmersiveFlashlightVR\spheres\` and
`Textures\ImmersiveFlashlightVR\spheres\` (the texture is set by code at runtime, so nothing in them names the
mod). An `[ImFl_*ActivationSphere]` section's `sSphereStyle` picks their look.

## Pack command

```
python external\F4VR-CommonFramework\nif-tools\vrui_atlas.py pack --name misc --texture-subpath ImmersiveFlashlightVR data\Resources\misc --output data\mod
```

Writes `Textures\ImmersiveFlashlightVR\misc.DDS` and one `Meshes\ImmersiveFlashlightVR\misc\<name>.nif` per
sprite. Renaming a file here means updating the matching path in the source.
