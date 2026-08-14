# Weapon Flashlight Requirement

Mount the light on your gun only when the gun actually has a lamp modeled on it — and when it does, put the
beam exactly where that lamp is instead of floating it off the barrel.

For how the light itself works see the [Usage and Configuration Guide](README.md); for install and
requirements see the [main README](../README.md).

## Contents

- [Why this exists](#why-this-exists)
- [How it works](#how-it-works)
- [What changes when a lamp is found](#what-changes-when-a-lamp-is-found)
- [When the weapon has no lamp](#when-the-weapon-has-no-lamp)
- [Making it work with your weapon mods](#making-it-work-with-your-weapon-mods)
- [Configuration](#configuration)
- [Common adjustments](#common-adjustments)

## Why this exists

Weapon mounting is a guess by default. The mod knows where your gun is, so it puts the beam at a tuned
offset near the barrel and that reads fine for most weapons — but the light is coming from nothing. There's
no lamp on the gun; the beam simply starts in mid-air beside it.

Several weapon packs (Tactical Weapon Mods and the merged DOOMBASED weapons pack among them) ship
attachments that actually model a flashlight on the weapon. If you have one of those, the mod can do much better: find the
lamp on the gun and root the beam at *it*. Now the light comes out of a real object, moves with it, and
turning it on lights the lamp itself up.

The flip side is the requirement. Once the mod knows how to find a real lamp, it can also insist on one: no
lamp on the gun, no weapon light. Guns you've fitted with a flashlight get one, guns you haven't don't, and
the weapon light becomes a piece of kit you attach rather than something every weapon has.

It applies the same in and out of power armor — the gun is the gun either way.

## How it works

`iWeaponFlashlightMeshRequired` has three settings, changed on the misc config screen or in the INI:

| Mode | Meaning |
| --- | --- |
| `0` **Disabled** | No restriction. The weapon light always applies, at the generic barrel position. |
| `1` **Enabled** | Always require a modeled flashlight on the equipped weapon. |
| `2` **AutoDetect** *(default)* | Require it only if a supported weapon mod is in your load order. |

**AutoDetect** is the default because the requirement only makes sense if you can actually satisfy it.
Without a weapon pack that models flashlights, *no* gun would ever qualify and the weapon mount would just
stop working. So it checks your load order for the plugins listed in `sWeaponFlashlightAutoDetectPlugins`
(`VRCP_DOOMMerged.esp`, `TacticalMods.esp` by default) and turns the requirement on only if one is present.
Toggling the setting on the misc screen tells you which way it resolved.

**Finding the lamp.** Whenever your equipped weapon changes — drawing, holstering, or swapping — the mod
searches the weapon's 3D model for the first node named in `sWeaponFlashlightMeshNodes` (list order is
priority) and remembers what it found. The node has to be present *and* visible, so a lamp that's modeled in
the mesh but not fitted on your particular gun doesn't count.

The search runs only on that weapon change, not every frame. That keeps it cheap, but it means a lamp whose
model attaches a moment late isn't noticed until the next change — **holster and re-draw** if a gun you know
has a flashlight isn't being recognised.

## What changes when a lamp is found

Three things, all on by default:

- **The beam roots at the lamp.** Instead of the generic barrel offset, the light is positioned relative to
  the actual flashlight node and follows it exactly, including when the weapon animates.
  `bWeaponFlashlightMountBeamToMesh` turns this off; `tWeaponFlashlightMountTransform` fine-tunes the offset
  from the lamp, live, while you watch.
- **The lamp glows.** A glow-only model — the lamp's light effect with no flashlight body — is attached at
  the lamp while the light is on, so the flashlight on your gun visibly emits rather than sitting dark with
  a beam beside it.
- **The move-to-weapon gesture moves onto the lamp.** The offhand-to-primary-hand zone normally sits at your
  primary hand; with a lamp detected, it anchors to the lamp instead, so you reach for the flashlight on the
  gun to move or toggle the light. `bWeaponFlashlightAnchorPrimaryHandSphereToMesh` turns this off. It needs
  beam rooting on as well — with `bWeaponFlashlightMountBeamToMesh` off, the zone stays on your hand.

## When the weapon has no lamp

With the requirement on and no lamp detected:

- **An on-weapon light turns off.** Drawing a lampless gun while the light is on the weapon puts it out.
- **The move-to-weapon gesture goes inert.** Bringing your offhand to your primary hand does nothing, and
  the button passes through to the game as a normal press.
- **The two-handed toggle does nothing.** The dedicated toggle for two-handed grips also needs a real lamp.
- **A two-handed grip falls back to your head.** This is the one case that doesn't just turn off: if your
  light is in your **offhand** and you take a two-handed grip, the offhand is occupied, so rather than lose
  the light it moves to your **head** — and is then subject to the
  [headgear requirement](headgear-restriction.md), which can turn it off in turn if you're bare-headed.
  Releasing the grip returns it to your offhand.

## Making it work with your weapon mods

The node names are the whole mechanism, and node names inside a NIF aren't a standard — they're whatever the
weapon's author called them. `sWeaponFlashlightMeshNodes` ships with a single entry, `AddOnNode`, which
covers the supported packs. For anything else you'll need to add the right name.

To find it, open the weapon's mesh in NifSkope and look for the node the flashlight model hangs from. Add its
name to the list (comma-separated, first match wins), save the INI, then holster and re-draw to force a
re-scan.

The log tells you what happened. At
`%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`:

- `AutoDetect: supported weapon mod '<plugin>' found — weapon-flashlight-mesh requirement on` — AutoDetect
  resolved to on, and which plugin did it.
- `AutoDetect: no supported weapon mod found — weapon-flashlight-mesh requirement off` — nothing matched, so
  the requirement is inert.
- `Found visible weapon-mounted flashlight node: <name>` — a lamp was detected on the equipped weapon, and
  which name matched.
- `Equipped weapon has no flashlight mesh — turning the weapon flashlight off` — the requirement was
  enforced against your current gun.
- `No visible weapon-mounted flashlight node found` — the scan ran and came up empty. Either the gun has no
  flashlight fitted, or its node isn't in your list. This one is logged at debug level, so set
  `iLogLevel = 1` in the `[Debug]` section to see it.

Edits to `sWeaponFlashlightAutoDetectPlugins` are picked up when you save the INI, so you can add a weapon
pack's plugin and see AutoDetect flip without restarting the game.

Note that the gate and the beam rooting are the same feature: detection only runs while the requirement is
on. Setting `iWeaponFlashlightMeshRequired = 0` doesn't keep the rooting and drop the gate — it stops the
mod looking for a lamp at all, and every weapon goes back to the generic barrel position.

## Configuration

The in-game misc screen cycles the mode; everything else is INI-only. All keys live in the
`[ImFl_WeaponMount]` section. See [Advanced Configuration](README.md#advanced-configuration) for the INI
location.

| Setting | Default | Meaning |
| --- | --- | --- |
| `iWeaponFlashlightMeshRequired` | `2` (AutoDetect) | `0` disabled, `1` always require a lamp, `2` require one only if a supported weapon mod is installed. |
| `sWeaponFlashlightAutoDetectPlugins` | `VRCP_DOOMMerged.esp, TacticalMods.esp` | Plugins that switch AutoDetect on. Add your own weapon packs here. |
| `sWeaponFlashlightMeshNodes` | `AddOnNode` | Node names searched for on the weapon, in priority order. |
| `bWeaponFlashlightMountBeamToMesh` | `true` | Root the beam at the detected lamp instead of the generic barrel offset. |
| `bWeaponFlashlightAnchorPrimaryHandSphereToMesh` | `true` | Move the offhand-to-primary-hand gesture zone onto the lamp. Needs beam rooting on. |
| `tWeaponFlashlightMountTransform` | `0,0,0;0,0,0;1` | Light offset from the lamp when rooted there. Live-reloaded, so you can tune it in-game. |
| `sToggleWeaponFlashlightTwoHandedBinding` | `offhand tap trigger` | The two-handed weapon-light toggle. Documented with the other gestures in the [Usage Guide](README.md#toggle-the-weapon-light-two-handed). |

## Common adjustments

| Goal | Change |
| --- | --- |
| Every gun gets a weapon light | `iWeaponFlashlightMeshRequired = 0` |
| Only guns with a fitted flashlight | `iWeaponFlashlightMeshRequired = 1` |
| Support another weapon pack's lamp | Add its node name to `sWeaponFlashlightMeshNodes` |
| Auto-enable for another weapon pack | Add its plugin to `sWeaponFlashlightAutoDetectPlugins` |
| Beam comes out at the wrong spot | Tune `tWeaponFlashlightMountTransform` while the light is on |
| Keep the gesture on your hand, not the gun | `bWeaponFlashlightAnchorPrimaryHandSphereToMesh = false` |
| Keep the gate, drop the beam rooting | `bWeaponFlashlightMountBeamToMesh = false` |
| A gun with a lamp isn't recognised | Holster and re-draw, then check the log for the scan result |
