# Usage and Configuration Guide

A VR-first replacement for the Pip-Boy flashlight supporting in-hand, head-mounted, and weapon-mounted use. Each mode is independently configurable for beam intensity, distance, spread, color, and gobo. Moving the light between locations is physical and gesture-driven, with sensible, context-aware restrictions, and all configuration is performed in-game and applied live — no reloads, restarts, or re-installs.

Explore the wasteland at night with a powerful, long-range flashlight to see into the distance, then switch to a soft, wide head-mounted beam for close-quarters exploration and looting.

For installation, requirements, and credits see the [main README](../README.md). For shadows, the VR FPS Stabilizer warning, and other troubleshooting see the [FAQ](faq.md).

## Contents

- [Flashlight Locations](#flashlight-locations)
- [Moving the Light (Gestures)](#moving-the-light-gestures)
- [Default Control Bindings](#default-control-bindings)
- [Grip Styles](#grip-styles)
- [Power Armor](#power-armor)
- [In-Game Configuration](#in-game-configuration)
- [Beam Settings](#beam-settings)
- [Restrictions](#restrictions)
- [NPC Light Detection](#npc-light-detection)
- [Vanilla Flashlight Toggle](#vanilla-flashlight-toggle)
- [Advanced Configuration](#advanced-configuration)
- [Mod Recommendations](#mod-recommendations)

## Flashlight Locations

The light can sit in one of several locations, each with its own beam settings:

- **In hand** — held in your offhand or primary hand, pointed wherever you point the controller, like a handheld flashlight. A visible flashlight model appears in the hand.
- **On head (cap lamp)** — mounted on your head and following where you look, ideal for hands-free close-quarters work.
- **On weapon** — mounted on a drawn non-melee weapon and aligned with the barrel, including two-handed grips. When a supported weapon mod provides a modeled flashlight, the beam roots at that lamp.
- **Power-armor head** — the helmet lamp while in power armor, tuned separately (and brighter/longer-range by default).

The mod tracks your chosen location separately in and out of power armor, so switching the light in power armor doesn't change your out-of-PA choice.

### Automatic switching

When the light is set to a hand, the runtime adapts to what that hand is doing:

- **Primary hand, empty** → light sits in the hand.
- **Primary hand, ranged weapon drawn** → light moves onto the weapon.
- **Primary hand, melee/unarmed drawn** → the hand is occupied, so the light turns off (it can't sit there).
- **Offhand, two-handed grip** → the offhand is on the foregrip, so the light routes onto the weapon. If the weapon-flashlight requirement is on and the weapon has no modeled lamp, it falls back to your head instead of being lost; releasing the grip returns it to the offhand.

## Moving the Light (Gestures)

Locations are changed physically, with haptic feedback when your hand enters an interaction zone. These gestures are the primary way to move and toggle the light — the in-game config UI is only for tuning. All bindings are configurable (see below); set any to `none` to disable that gesture.

### Grab from / stow on your body

A beam-less flashlight model sits on your chest whenever the light is off, head-mounted, or weapon-mounted.

- **Grab**: reach a hand into the model's grab zone and fire the grab binding to take the light into that hand — the light turns on in that hand.
- **Stow**: reach the same hand back into the zone and fire again to put the model back — the light turns off.
- A hand holding a **drawn weapon** can't grab or stow (in practice, your primary hand while holding the gun).

### Put the light on your head

- Bring your **offhand** near your head and **tap** to put the light on your head; tap again while it's on your head to turn it off.
- **Long-press** near your head to pull a head-mounted light back to your offhand.

### Move the light to your hand or weapon

- Bring your **offhand** near your **primary hand** and **tap** to move the light:
  - Light on offhand + primary hand empty → moves to the **primary hand**.
  - Light on offhand + ranged weapon drawn → moves **onto the weapon**.
  - Light on offhand + melee/unarmed → nothing happens (the hand is occupied).
  - Light **on weapon** → toggles **off**.
  - Light **on primary hand** → returns to the **offhand**.
  - Light **off** → turns on at the primary hand (empty) or on the weapon (ranged weapon drawn).
- **Long-press** to pull an on-weapon light back to your offhand.
- When the weapon-flashlight requirement has detected a modeled lamp, this zone anchors to the **gun's lamp**, so you reach it by bringing your offhand to the flashlight on the gun.

### Toggle the weapon light two-handed

When you grip a weapon **two-handed** your offhand is on the foregrip and can't reach the primary-hand zone, so there's a dedicated toggle:

- While gripping the weapon two-handed, **tap** to toggle the weapon-mounted light on/off. It doesn't change your configured location, so releasing the grip returns the light to wherever your config routes it.

## Default Control Bindings

All bindings use the format `<hand> <type> <button> [duration] [suppress] [+modifier]` (e.g. `offhand tap trigger`, `primary longpress A`, `none`). The three proximity gestures are grouped into per-gesture INI **sections** (each with a `sPrimaryBinding` / `sSecondaryBinding` plus its zone and haptics); the full guide is in the [framework input-binding & activation-sphere docs](https://github.com/ArthurHub/F4VR-CommonFramework/blob/main/docs/input-binding.md).

| Gesture | INI section → key | Default |
| --- | --- | --- |
| Grab/stow with offhand | `[ImFl_BodyActivationSphere]` → `sPrimaryBinding` | `offhand tap trigger` |
| Grab/stow with primary hand | `[ImFl_BodyActivationSphere]` → `sSecondaryBinding` | `primary tap trigger` |
| Put light on head / toggle off | `[ImFl_HeadActivationSphere]` → `sPrimaryBinding` | `offhand tap trigger` |
| Pull head light to offhand | `[ImFl_HeadActivationSphere]` → `sSecondaryBinding` | `offhand longpress trigger` |
| Move light to primary hand / weapon | `[ImFl_PrimaryHandActivationSphere]` → `sPrimaryBinding` | `offhand tap trigger` |
| Pull weapon light to offhand | `[ImFl_PrimaryHandActivationSphere]` → `sSecondaryBinding` | `offhand longpress trigger` |
| Toggle weapon light (two-handed) | `sToggleWeaponFlashlightTwoHandedBinding` | `offhand tap trigger` |

The same `offhand tap trigger` is shared by several gestures on purpose — they apply in mutually exclusive situations (near your head vs. near your hand vs. gripping two-handed), and only the relevant one fires while suppressing the button from the game.

## Grip Styles

Hand-held modes have two grips, each with its own light angle, model pose, and hand pose:

- **Forward** — thumb-up grip, controller top facing up.
- **Overhand** — fist / ice-pick grip, controller flipped around its barrel so the top faces down while the beam still points forward.

`iFlashlightGripMode` selects how the grip is chosen: `0` = Auto (detected from controller tilt, with hysteresis so it doesn't flip-flop), `1` = Forward only, `2` = Overhand only. In Auto mode, `fFlashlightGripOverhandTiltDegrees` sets the tilt at which it switches to Overhand and `fFlashlightGripHysteresisDegrees` the band for switching back.

## Power Armor

Power armor swaps the skeleton and poses the hands differently, so most spatial settings have a power-armor variant (`...PA` suffix) — light transforms, mesh transforms, hand poses, and the body/grab zones. PA variants fall back to their non-PA value when omitted. Beam values (intensity/distance/spread/color/gobo) are shared in and out of power armor, except the head, which has a dedicated brighter, longer-range PA-head profile. The head-mounted light in power armor uses the helmet lamp and is never gated by the headgear requirement.

## In-Game Configuration

Open the FRIK configuration UI (hold both thumbsticks for ~2 seconds) and select Immersive Flashlight. The UI is split into three screens:

- **Main menu** — opens the beam and misc screens, opens the INI file for advanced editing, opens the wiki, or exits.
- **Beam screen** — location toggles, live beam tuning, gobo and color presets, and Save / Reset. Unsaved changes are discarded when you close it.
- **Misc screen** — global toggles: beam shadows, show all activation spheres, the stowed body model, the headgear requirement, the weapon-flashlight requirement, and the vanilla-toggle disable. Each is saved immediately and applied live.

Everything is applied while you watch, so you can tune the beam against the actual scene.

## Beam Settings

Each location stores its own beam profile:

| Setting | Meaning |
| --- | --- |
| **Intensity** (`fade`) | Brightness of the beam. |
| **Distance** (`radius`) | How far the light makes objects visible. |
| **Spread** (`FOV`) | How wide or narrow the cone is. |
| **Color** (RGB) | Beam color. |
| **Gobo** | Beam texture / pattern projected by the light. |

The defaults aim for a wide, soft head lamp for tight spaces; a far-reaching, tighter hand flashlight for walking in the dark; and a tighter, "tactical" weapon beam. Tune them per mode in the beam screen or the INI.

## Restrictions

Optional gameplay gates, configured on the misc screen or in the INI. When a requirement isn't met the corresponding light is turned off and its activation gesture goes inert until the requirement is met again.

### Headgear requirement (head light)

`iFlashlightHeadgearRequirement` gates the head-mounted light **out of power armor** (in power armor the helmet lamp always applies):

- `0` **None** — no restriction.
- `1` **Any headgear** — any item worn in the head slot is enough.
- `2` **Immersive** — only "light-capable" headgear (helmets / hard hats, not soft hats), decided by a keyword set plus allow/deny lists you can extend in the INI.

### Weapon-flashlight requirement (weapon light)

`iWeaponFlashlightMeshRequired` requires the equipped weapon to actually carry a modeled flashlight before the light may mount on it, and roots the beam at that lamp:

- `0` **Disabled** — no restriction; the weapon light always applies.
- `1` **Enabled** — always require a modeled flashlight mesh on the weapon.
- `2` **AutoDetect** — require it only when a supported weapon mod (e.g. Tactical Weapon Mods) is in your load order.

When a lamp is detected, the beam roots at it (`bWeaponFlashlightMountBeamToMesh`) and the move-to-weapon gesture's zone anchors to it (`bWeaponFlashlightAnchorPrimaryHandSphereToMesh`).

## NPC Light Detection

Vanilla ignores flashlights for stealth entirely — the beam points away from you, so the game never counts it against you. This mod makes it count, and makes it **directional**: carrying a lit flashlight makes you somewhat visible, putting the beam on someone makes you much more so, and a bright beam in a hostile's face at close range means they've seen you. Line of sight is checked, so nothing is alerted through a wall.

It's on by default and configured in the INI (`[ImFl_NpcDetection]`). See **[NPC Light Detection](npc-detection.md)** for the mechanics, the full settings reference, and how it compares to Flashlight Stealth Fix — don't run both.

## Vanilla Flashlight Toggle

`bDisableVanillaFlashlightToggle` (default **on**) disables the game's built-in global Pip-Boy light toggle so only this mod's gestures control the light. Turn it off if you want the vanilla long-press toggle back alongside the gestures.

## Advanced Configuration

Many settings beyond the in-game UI live in the INI, and any change is live-loaded into the running game so you can tune and immediately see the effect.

Location: `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR\ImmersiveFlashlightVR.ini`

Logs are written to `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`.

Set `bShowAllActivationSpheres = true` to render every grab / head / primary-hand activation zone at its exact size and position while tuning (or set a single gesture's `sShowSphere` to `always` / `wheninside` in its section). See the [FAQ](faq.md) for shadow quality and the VR FPS Stabilizer warning.

## Mod Recommendations

- [Comfort Swim VR](https://www.nexusmods.com/fallout4/mods/94647) — improves Fallout 4 VR swimming mechanics.
- [Virtual Holsters](https://www.nexusmods.com/fallout4/mods/51224) — realistic body-based holstering.
