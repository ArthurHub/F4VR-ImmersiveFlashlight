# FAQ and Troubleshooting

Common questions and fixes. How the light works: [Usage and Configuration Guide](README.md). Install and requirements: [main README](../README.md). Click a question to expand it.

## Contents

- [Getting Started](#getting-started)
- [Head and Weapon Light](#head-and-weapon-light)
- [Visuals](#visuals)
- [NPC Light Detection](#npc-light-detection)
- [Compatibility](#compatibility)

## Getting Started

<details>
  <summary>Where are the settings?</summary>
&#10240;

In [Mods Config FO4 VR](https://www.nexusmods.com/games/fallout4/mods/110945): press in both thumbsticks and hold them, then pick Immersive Flashlight VR. Everything is also in the config file, which reloads as you save it:

- Config: `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR\ImmersiveFlashlightVR.toml`
- Log: `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`

See [Settings](README.md#settings).

</details>

<details>
  <summary>The gestures do nothing, and there's no flashlight on my belt</summary>
&#10240;

The mod waits until you've picked up the **Pip-Boy**. Before that the game only has the vanilla wrist light, which the mod can't move. Once the Pip-Boy is on your wrist, or you're in power armor, everything switches on.

The gestures also pause while a menu that takes the controllers is open (workbench, container, terminal, dialogue). If the belt flashlight is missing with the Pip-Boy on, check **Grab flashlight from the body** on the General page.

</details>

<details>
  <summary>The flashlight won't turn on, or my old flashlight button does nothing</summary>
&#10240;

The vanilla long-press of the offhand trigger toggles the light by default. If it does nothing, **Disable the game's flashlight toggle** is on (General page). You can always turn the light on with a gesture: grab it off your belt, or use the head or hand gesture. See [Gestures](README.md#gestures).

If it turns on but not where you expect, a restriction may be blocking that location. See [Head and Weapon Light](#head-and-weapon-light).

</details>

<details>
  <summary>My beam tuning changes are gone</summary>
&#10240;

The in-world tuning hands its changes to the Mods Config menu, which shows them as pending once you exit the tuning. Press **Save** there to keep them. Closing the menu without saving discards them.

</details>

## Head and Weapon Light

<details>
  <summary>The light won't go on my head</summary>
&#10240;

Out of power armor, the head light needs headgear. By default it takes a light-capable helmet or hard hat, not a soft hat. Wear one, or set **Require headgear for flashlight on head** (General page) to a looser option. In power armor the helmet lamp always works. See [Headgear Requirement](headgear-restriction.md).

</details>

<details>
  <summary>My helmet should count as light-capable but doesn't</summary>
&#10240;

Set `aDumpDataOnce = ["headgear"]` in the config file's `[Debug]` section and save. The log then lists every head-slot armor in your load order, as allowed or blocked. Add the helmet to `aHeadLightAllowList` as `"localFormID|plugin"`, or its mod's helmet keyword to `aHeadLightKeywords`. See [Choosing what counts](headgear-restriction.md#choosing-what-counts).

</details>

<details>
  <summary>The weapon light won't turn on</summary>
&#10240;

With a supported weapon mod installed (Tactical Weapon Mods, VRCP DOOM), the weapon light only goes on guns that carry a modeled flashlight. Fit a flashlight attachment, or set **Require a flashlight on the weapon** (General page) to **Not required**. See [Weapon Flashlight Requirement](weapon-mount-restriction.md).

</details>

<details>
  <summary>My gun has a flashlight attachment but the mod doesn't see it</summary>
&#10240;

The weapon is checked only when it changes, so **holster and draw it again**. If it's still missed, the lamp hangs from a node the mod doesn't know: find its name in NifSkope and add it to `aWeaponFlashlightMeshNodes`. The log records each check. See [Making it work with your weapon mods](weapon-mount-restriction.md#making-it-work-with-your-weapon-mods).

</details>

<details>
  <summary>The beam on my weapon comes out of the wrong place</summary>
&#10240;

On a gun with no detected lamp, the beam sits at a general offset near the barrel. With a lamp detected it comes from the lamp, and **Beam on the flashlight** (Advanced page, Weapon mount) adjusts it live.

</details>

<details>
  <summary>The hand light turns off when I draw a melee weapon</summary>
&#10240;

That's intended: the melee weapon occupies your primary hand. Put the weapon away, or move the light to your offhand or head.

</details>

<details>
  <summary>The light jumps to my head when I grip my weapon two-handed</summary>
&#10240;

A two-handed grip occupies your offhand, so a light held there normally moves onto the weapon. If the weapon-flashlight requirement is on and the gun has no lamp, it goes to your head instead. Bare-headed, the headgear requirement then turns it off. Letting go of the grip brings it back. See [When the weapon has no lamp](weapon-mount-restriction.md#when-the-weapon-has-no-lamp).

</details>

## Visuals

<details>
  <summary>The flashlight shadows look broken or blocky</summary>
&#10240;

They only render well with the game's **Shadow Quality** on **HIGH**. The General page warns when it's lower. Performance mods such as [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961) lower it while you play. If you'd rather have none, set **Shadows** to off on the General page.

</details>

<details>
  <summary>How do I hide the icon near the flashlight on my belt?</summary>
&#10240;

It marks the grab zone, and shows only while a hand is inside it. On the Gestures page, set **Body grab**'s **Show icon** to Never, or make it smaller with **Icon size**. The grab works the same, and the haptic tick still marks the zone. If a sphere shows too, turn off **Show all activation spheres**.

</details>

<details>
  <summary>I don't see the flashlight model in my hand</summary>
&#10240;

The model is posed on your fingers through FRIK's hand-pose API, which needs **FRIK v78** or later. On older FRIK the model stays hidden, and everything else still works. It can also be turned off: see **Show the model** on the Advanced page.

</details>

## NPC Light Detection

<details>
  <summary>Enemies notice me as soon as I turn the flashlight on</summary>
&#10240;

That's NPC light detection. The game ignores flashlights for stealth, and this mod makes the beam count. A lit flashlight makes you a little visible, and the beam on someone makes you much more so. Turn it off with **Enemies detect player light** on the General page. See [NPC Light Detection](npc-detection.md).

</details>

<details>
  <summary>The flashlight makes stealth too hard</summary>
&#10240;

Tune it on the Advanced page (Enemy light detection) rather than turning it off:
- **Brighten curve** up keeps you dim until the beam is close.
- **Reach** down shortens how far the beam gives you away.
- **Lit while on** down lowers the cost of just carrying the light.
- **Only while sneaking** limits the alerts to sneaking.

See [Configuration](npc-detection.md#configuration).

</details>

<details>
  <summary>Enemies find me when the beam isn't pointing at them</summary>
&#10240;

Three causes:
- Having the light on makes you a little visible by itself.
- While the beam is on someone, every NPC who can see you sees you lit, not only that one.
- The bright patch the beam paints can be noticed by an NPC near it who is looking at it. **Where it lands** turns that off.

</details>

<details>
  <summary>Do I still need Flashlight Stealth Fix?</summary>
&#10240;

No, and don't run both: [Flashlight Stealth Fix](https://www.nexusmods.com/fallout4/mods/76586) solves the same problem, so the two stack. This mod's version depends on where you aim and checks line of sight, but it only knows its own flashlight. Uninstall the other mod, or turn off **Enemies detect player light** here. See the [comparison](npc-detection.md#compared-to-flashlight-stealth-fix).

</details>

## Compatibility

<details>
  <summary>Does it conflict with other flashlight mods?</summary>
&#10240;

Run one flashlight replacement at a time: other mods that drive the Pip-Boy light conflict with it.

</details>

<details>
  <summary>Does it work with ROCK?</summary>
&#10240;

Yes, with **ROCK v0.9 or newer**. The light follows how ROCK has you hold the weapon: on the weapon in a two-handed grip, in your free primary hand with a pistol fired from the offhand, and on the weapon for a rifle the offhand carries. See [Weapons in the offhand (ROCK)](README.md#weapons-in-the-offhand-rock).

Known limitation: while ROCK fires from your left hand, the mod's primary-hand haptics are felt in the offhand.

</details>

<details>
  <summary>Why do I get a VR FPS Stabilizer warning?</summary>
&#10240;

[VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961) lowers the shadow quality, which breaks the flashlight's shadows. The mod reminds you every 5 minutes while the light is on with shadows. Turn the reminder off with **Warn about VR FPS Stabilizer** on the General page.

</details>
