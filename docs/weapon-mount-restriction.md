# Weapon Flashlight Requirement

The weapon light goes only on guns that carry a modeled flashlight, and the beam comes from that lamp instead of floating beside the barrel. It applies the same in and out of power armor.

How the light works: [Usage and Configuration Guide](README.md). Settings and the config file: [Settings](README.md#settings).

## Contents

- [Modes](#modes)
- [What changes when a lamp is found](#what-changes-when-a-lamp-is-found)
- [When the weapon has no lamp](#when-the-weapon-has-no-lamp)
- [Making it work with your weapon mods](#making-it-work-with-your-weapon-mods)
- [Settings](#settings)

## Modes

**Require a flashlight on the weapon**, on the General page (`iWeaponFlashlightMeshRequired`):

| Option | Meaning |
| --- | --- |
| **Not required** (`0`) | Every weapon gets the light, at a general offset near the barrel. |
| **Required** (`1`) | Only a weapon with a modeled flashlight. |
| **Required with a weapon mod** (`2`, default) | Required only while a supported weapon mod is installed. |

The default makes sense because without a pack that models flashlights, no gun could qualify. Its plugins are listed in `aWeaponFlashlightAutoDetectPlugins`, by default `VRCP_DOOMMerged.esp` and `TacticalMods.esp`. The General page says whether one was found.

The mod finds the lamp by looking through the weapon's model for the first node named in `aWeaponFlashlightMeshNodes` (the list order is the priority). The node has to be visible, so a lamp that isn't fitted on your gun doesn't count. It looks only when the equipped weapon changes, so if a lamp is missed, **holster and draw the weapon again**.

The requirement and the lamp detection are one feature. With **Not required**, the mod doesn't look for a lamp at all, and every gun uses the general offset.

## What changes when a lamp is found

These are all on by default, under Weapon mount on the Advanced page:

- **The beam comes from the lamp** and follows it as the weapon moves. Turn this off with **Beam from the weapon's flashlight** (`bWeaponFlashlightMountBeamToMesh`). **Beam on the flashlight** (`tWeaponFlashlightMountTransform`) fine-tunes the offset from the lamp, live.
- **The lamp glows** while the light is on, matching the weapon beam. See [Beam](README.md#beam).
- **The hand-or-weapon gesture moves to the lamp**, so you reach for the flashlight on the gun. Turn this off with **Gesture at the weapon's flashlight** (`bWeaponFlashlightAnchorPrimaryHandSphereToMesh`). It also needs the beam to come from the lamp.

## When the weapon has no lamp

With the requirement on:

- **The light goes elsewhere instead of the weapon.** Drawing or switching to such a gun with the light on your weapon or in your primary hand moves it to your head, or to your offhand if the [headgear requirement](headgear-restriction.md) rules the head out. A two-handed grip with the light in your offhand does the same, since the grip occupies that hand. Putting the gun away, or letting go of the grip, brings it back to the hand.
- **With no place left, the light turns off.** That's both hands on the gun and no eligible headgear. Turning it on there shows a message saying why.
- **Without the move, the light turns off.** With **Move the light when its place isn't allowed** (General page) off, the light turns off instead of going elsewhere, and turning it on with such a gun shows a message saying why.
- **The hand-or-weapon gesture won't put it on the weapon**, and the two-handed toggle does nothing.

## Making it work with your weapon mods

Node names are whatever the weapon's author called them. The default `["AddOnNode"]` covers the supported packs. For another pack:

1. Open the weapon's mesh in NifSkope and find the node the flashlight hangs from.
2. Add its name to `aWeaponFlashlightMeshNodes` (e.g. `["AddOnNode", "YourNode"]`), and the pack's plugin to `aWeaponFlashlightAutoDetectPlugins`.
3. Save the config file, then holster and draw the weapon again.

The log (`%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`) tells you what happened:

- `AutoDetect: supported weapon mod '<plugin>' found` or `no supported weapon mod found`: whether the requirement is on.
- `Found visible weapon-mounted flashlight node: <name>`: a lamp was found on the weapon.
- `Flashlight runtime location: OnWeapon -> <location>`: the weapon has no lamp, so the light moved elsewhere.
- `No location available for the flashlight (at <location>) — turning it off`: there was no place left for the light.
- `No visible weapon-mounted flashlight node found`: no lamp was found. It is logged at debug level, so set `iLogLevel = 1` in `[Debug]` to see it.

## Settings

All in the `[ImFl_WeaponMount]` section:

| Setting | Default | Meaning |
| --- | --- | --- |
| `iWeaponFlashlightMeshRequired` | `2` | `0` not required, `1` required, `2` required with a weapon mod. |
| `aWeaponFlashlightAutoDetectPlugins` | `["VRCP_DOOMMerged.esp", "TacticalMods.esp"]` | Plugins that turn the requirement on. |
| `aWeaponFlashlightMeshNodes` | `["AddOnNode"]` | Lamp node names, in priority order. |
| `bWeaponFlashlightMountBeamToMesh` | `true` | The beam comes from the lamp. |
| `bWeaponFlashlightAnchorPrimaryHandSphereToMesh` | `true` | The hand-or-weapon gesture is at the lamp. Needs the one above. |
| `tWeaponFlashlightMountTransform` | `{ pos = [0.0, 0.0, 0.0], scale = 1.0 }` | The light's offset from the lamp. |
| `kToggleWeaponFlashlightTwoHandedBinding` | offhand tap trigger | The [two-handed toggle](README.md#two-handed-toggle). |
