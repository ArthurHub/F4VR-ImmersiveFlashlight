# FAQ and Troubleshooting

Common questions and fixes. For how the light works and the full list of settings, see the [Usage and Configuration Guide](README.md); for install and requirements see the [main README](../README.md).

## The flashlight won't turn on, or my old flashlight button does nothing

The vanilla Pip-Boy light toggle is **disabled by default** so only this mod's gestures control the light. Turn the light on by grabbing the model off your chest, or with the head / hand gestures — see [Moving the Light](README.md#moving-the-light-gestures). If you'd rather keep the vanilla button, turn off the vanilla-toggle disable on the misc config screen, or set `bDisableVanillaFlashlightToggle = false` in the INI.

## How do I move the light between hand, head, and weapon?

All location changes are physical VR gestures (grab from your body, offhand-to-head, offhand-to-primary-hand, two-handed toggle). The full reference and the configurable [default bindings](README.md#default-control-bindings) are in the Usage Guide. Any gesture can be rebound or disabled (`none`) in the INI.

## The light won't go on my head

Out of power armor the head lamp is gated by a **headgear requirement**, which ships set to **Immersive** — you must be wearing a light-capable helmet or hard hat (not a soft hat). Either wear eligible headgear, or change `iFlashlightHeadgearRequirement` on the misc config screen to **Any headgear** or **None**. In power armor the helmet lamp always works regardless of this setting. See [Restrictions](README.md#restrictions).

## The hand light turns off when I draw a melee weapon

That's intentional — a drawn melee or unarmed weapon occupies your primary hand, so the light can't sit there. Sheathe the weapon, or move the light to your offhand or head.

## The weapon-mounted light won't turn on

The **weapon-flashlight requirement** ships set to **AutoDetect**: when a supported weapon mod (e.g. Tactical Weapon Mods) is installed, the weapon light only mounts on guns that actually carry a modeled flashlight. Equip a weapon with a flashlight attachment, or set `iWeaponFlashlightMeshRequired` to **Disabled** on the misc config screen so the weapon light always applies. See [Restrictions](README.md#restrictions).

## The flashlight shadows look bad or blocky

The mod adds shadows for immersion, but they only render well when the game's **Shadow Quality** is set to **HIGH** — anything lower looks broken. Some performance mods, like [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961), lower shadow quality while the game is running and break the look. If you prefer no flashlight shadows, disable them on the misc config screen.

## Why do I get a VR FPS Stabilizer warning?

The mod detects [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961) because it can degrade flashlight shadows, and shows a notification every 5 minutes while the light is on. The warning does not appear if flashlight shadows are disabled. If you've handled it another way, silence the warning with `bWarnAboutFPSStabilizerMod = false` in the INI.

## Does it conflict with other flashlight mods?

Run only one flashlight replacement at a time — other mods that drive the Pip-Boy light will conflict. This mod also requires **FRIK v77+**; older FRIK versions lack the config UI and flashlight integration.

## Where are the settings, INI file, and logs?

Most options are on the in-game beam and misc config screens (open the FRIK configuration UI, then select Immersive Flashlight). Everything else lives in the INI and is live-reloaded into the running game when you save:

- INI: `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR\ImmersiveFlashlightVR.ini`
- Logs: `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`

The full settings reference is in the [Usage and Configuration Guide](README.md#advanced-configuration).
