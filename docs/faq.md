# FAQ and Troubleshooting

Common questions and fixes. For how the light works and the full list of settings, see the [Usage and Configuration Guide](README.md); for install and requirements see the [main README](../README.md); for what changed in each release see the [Changelog](changelog.md).

## Contents

- [The flashlight won't turn on, or my old flashlight button does nothing](#the-flashlight-wont-turn-on-or-my-old-flashlight-button-does-nothing)
- [How do I move the light between hand, head, and weapon?](#how-do-i-move-the-light-between-hand-head-and-weapon)
- [The light won't go on my head](#the-light-wont-go-on-my-head)
- [The weapon-mounted light won't turn on](#the-weapon-mounted-light-wont-turn-on)
- [Enemies notice me as soon as I turn the flashlight on](#enemies-notice-me-as-soon-as-i-turn-the-flashlight-on)
- [I don't see the flashlight model in my hand](#i-dont-see-the-flashlight-model-in-my-hand)
- [The flashlight shadows look bad or blocky](#the-flashlight-shadows-look-bad-or-blocky)
- [Where are the settings, INI file, and logs?](#where-are-the-settings-ini-file-and-logs)
- [The hand light turns off when I draw a melee weapon](#the-hand-light-turns-off-when-i-draw-a-melee-weapon)
- [The light jumps to my head when I grip my weapon two-handed](#the-light-jumps-to-my-head-when-i-grip-my-weapon-two-handed)
- [The flashlight makes stealth too hard](#the-flashlight-makes-stealth-too-hard)
- [Enemies find me even when the beam isn't pointing at them](#enemies-find-me-even-when-the-beam-isnt-pointing-at-them)
- [Do I still need Flashlight Stealth Fix?](#do-i-still-need-flashlight-stealth-fix)
- [Does it conflict with other flashlight mods?](#does-it-conflict-with-other-flashlight-mods)
- [Why do I get a VR FPS Stabilizer warning?](#why-do-i-get-a-vr-fps-stabilizer-warning)
- [My helmet should count as light-capable but doesn't](#my-helmet-should-count-as-light-capable-but-doesnt)
- [My gun has a flashlight attachment but the mod doesn't see it](#my-gun-has-a-flashlight-attachment-but-the-mod-doesnt-see-it)
- [The beam on my weapon comes out of the wrong place](#the-beam-on-my-weapon-comes-out-of-the-wrong-place)

## The flashlight won't turn on, or my old flashlight button does nothing

The vanilla long-press of the off-hand trigger still toggles the light **by default**, alongside this mod's gestures. If that button does nothing, the vanilla-toggle disable has been switched on — turn it back off on the misc config screen, or set `bDisableVanillaFlashlightToggle = false` in the INI. Either way you can always turn the light on by grabbing the model off your chest, or with the head / hand gestures — see [Moving the Light](README.md#moving-the-light-gestures).

If the light turns on but not where you expect it, a restriction may be blocking that location — see the head and weapon entries below.

## How do I move the light between hand, head, and weapon?

All location changes are physical VR gestures (grab from your body, offhand-to-head, offhand-to-primary-hand, two-handed toggle). The full reference and the configurable [default bindings](README.md#default-control-bindings) are in the Usage Guide. Any gesture can be rebound or disabled (`none`) in the INI.

## The light won't go on my head

Out of power armor the head lamp is gated by a **headgear requirement**, which ships set to **Immersive** — you must be wearing a light-capable helmet or hard hat (not a soft hat). Either wear eligible headgear, or change `iFlashlightHeadgearRequirement` on the misc config screen to **Any headgear** or **None**. In power armor the helmet lamp always works regardless of this setting. See [Headgear Requirement](headgear-restriction.md).

## The weapon-mounted light won't turn on

The **weapon-flashlight requirement** ships set to **AutoDetect**: when a supported weapon mod (e.g. Tactical Weapon Mods) is installed, the weapon light only mounts on guns that actually carry a modeled flashlight. Equip a weapon with a flashlight attachment, or set `iWeaponFlashlightMeshRequired` to **Disabled** on the misc config screen so the weapon light always applies. See [Weapon Flashlight Requirement](weapon-mount-restriction.md).

## Enemies notice me as soon as I turn the flashlight on

That's NPC light detection. Vanilla ignores flashlights for stealth completely — you can put a beam on a raider's chest from the dark and they won't react — and this mod makes the beam count. Carrying a lit flashlight makes you somewhat visible on its own, and putting the beam on someone makes you a lot more visible; catch a hostile full in the face at close range and they've simply seen you. Everything about it is tunable, and `bNpcDetectionEnabled = false` in the INI turns it off entirely. See [NPC Light Detection](npc-detection.md).

## I don't see the flashlight model in my hand

The in-hand model is posed onto your fingers through FRIK's hand-pose API, which arrived in **FRIK v78** — on older FRIK the model stays hidden rather than floating unposed in your hand. Update FRIK to v78 or later to see it. Everything else works regardless: the light itself, all the gestures, and the flashlight model stowed on your chest.

## The flashlight shadows look bad or blocky

The mod adds shadows for immersion, but they only render well when the game's **Shadow Quality** is set to **HIGH** — anything lower looks broken. Some performance mods, like [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961), lower shadow quality while the game is running and break the look. If you prefer no flashlight shadows, disable them on the misc config screen.

## Where are the settings, INI file, and logs?

Most options are on the in-game beam and misc config screens (open the FRIK configuration UI, then select Immersive Flashlight). Everything else lives in the INI and is live-reloaded into the running game when you save:

- INI: `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR\ImmersiveFlashlightVR.ini`
- Logs: `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`

The full settings reference is in the [Usage and Configuration Guide](README.md#advanced-configuration).

## The hand light turns off when I draw a melee weapon

That's intentional — a drawn melee or unarmed weapon occupies your primary hand, so the light can't sit there. Sheathe the weapon, or move the light to your offhand or head.

## The light jumps to my head when I grip my weapon two-handed

A two-handed grip occupies your offhand, so a light held there has to go somewhere. Normally it moves onto the weapon; if the weapon-flashlight requirement is on and your gun has no lamp, it goes to your **head** instead of being lost — and if you're not wearing eligible headgear, the headgear requirement then turns it off. Releasing the grip brings it back to your offhand. See [When the weapon has no lamp](weapon-mount-restriction.md#when-the-weapon-has-no-lamp).

## The flashlight makes stealth too hard

Tune it rather than turning it off. `fNpcDetectionLightLevelCurve` up keeps you dim until the beam is genuinely close; `fNpcDetectionMaxRange` down shortens how far the beam can give you away; `fNpcDetectionLightLevelBaseline` down reduces the cost of merely carrying the light; and `bNpcDetectionOnlyWhenSneaking = true` limits the alerts to when you're actually sneaking. The full list is in [NPC Light Detection](npc-detection.md#configuration).

## Enemies find me even when the beam isn't pointing at them

Three things can cause that. Simply having the light on makes you visible by a fixed amount (`fNpcDetectionLightLevelBaseline`) — you're carrying a light source. While the beam *is* on somebody, that extra visibility is your own illumination, so every NPC who can see you benefits from it, not only the one being lit. And the bright patch your beam paints on a wall or floor can be noticed by anyone standing near it who can actually see it (set `iNpcDetectionLitSpotSoundLevel = 0` to stop that). See [NPC Light Detection](npc-detection.md).

## Do I still need Flashlight Stealth Fix?

No, and you shouldn't run both — [Flashlight Stealth Fix](https://www.nexusmods.com/fallout4/mods/76586) solves the same vanilla problem, so the two stack into roughly double the penalty. This mod's version is directional (pointing the beam away from someone costs you far less than pointing it at them) and checks line of sight, neither of which the Papyrus version can do; on the other hand it only applies to this mod's flashlight. Either uninstall Flashlight Stealth Fix or set `bNpcDetectionEnabled = false` here. See the [comparison](npc-detection.md#compared-to-flashlight-stealth-fix).

## Does it conflict with other flashlight mods?

Run only one flashlight replacement at a time — other mods that drive the Pip-Boy light will conflict. This mod also requires **FRIK v77+**; older FRIK versions lack the config UI and flashlight integration.

## Why do I get a VR FPS Stabilizer warning?

The mod detects [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961) because it can degrade flashlight shadows, and shows a notification every 5 minutes while the light is on. The warning does not appear if flashlight shadows are disabled. If you've handled it another way, silence the warning with `bWarnAboutFPSStabilizerMod = false` in the INI.

## My helmet should count as light-capable but doesn't

The Immersive rule qualifies headgear by keyword, plus an allow list and a deny list you can extend. To see exactly what passes in your own load order, set `sDumpDataOnceNames = headgear` in the INI's `[Debug]` section and save — the mod writes every head-slot armor into the log, split into allowed and blocked. Then add the FormID of what you want to `sHeadLightAllowList` (as `localFormID|plugin`), or its mod's helmet keyword to `sHeadLightKeywords`. Changes apply live. See [Choosing what counts](headgear-restriction.md#choosing-what-counts).

## My gun has a flashlight attachment but the mod doesn't see it

The weapon is scanned only when it changes, so **holster and re-draw** first — that's usually all it takes. If it still isn't recognised, the mod is looking for node names it doesn't know: open the weapon in NifSkope, find the node the lamp hangs from, and add its name to `sWeaponFlashlightMeshNodes`. The log records the scan result either way. See [Making it work with your weapon mods](weapon-mount-restriction.md#making-it-work-with-your-weapon-mods).

## The beam on my weapon comes out of the wrong place

When no modeled flashlight is detected the beam sits at a generic offset near the barrel — it's a guess that suits most guns. When a lamp *is* detected the beam roots at that lamp instead, and `tWeaponFlashlightMountTransform` tunes the offset from it live while you watch. See [What changes when a lamp is found](weapon-mount-restriction.md#what-changes-when-a-lamp-is-found).
