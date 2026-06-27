A VR-first replacement for the Pip-Boy flashlight supporting in-hand, head-mounted, and weapon-mounted use. Each mode is independently configurable, providing precise control over beam intensity, distance, spread, color, and gobo. Location switching is physical and immersive, with sensible, context-aware restrictions. All configuration is performed in-game and applied live, with no reloads, restarts, or re-installs required.

Explore the wasteland at night with a powerful, long-range flashlight for visibility at a distance, then switch to a soft, wide head-mounted beam for close-quarters exploration and looting.


## Usage

* **Toggle flashlight**: Long-press the off-hand trigger (default F4VR behavior).
* **Switch flashlight location**: Bring your hand close to your head or your other hand until haptic feedback is felt, then press grab.
* **Configure settings**: Open via FRIK configuration UI (hold both thumbsticks for 2 seconds).



## Features

### Flashlight Modes

* **In-hand flashlight**: Held in the player’s left or right hand, behaving like a handheld flashlight directed by hand movement.
* **Head-mounted flashlight (cap lamp)**: Mounted on the player’s head and following head orientation.
* **Weapon-mounted flashlight**: Attached to non-melee weapons and aligned with weapon direction, including two-handed grip support.

### Beam Customization
* Live, in-game tuning via a simple UI.
* Per-mode configuration allowing each flashlight type to behave differently:

  * **Intensity** – brightness of the beam.
  * **Distance** – effective illumination range.
  * **Spread** – beam field of view.
  * **Color** – RGB beam color.
  * **Gobo** – beam texture / pattern.

### Immersion & Behavior
* Physical switching between head, hand, and weapon locations.
* Automatic switching:
  * Primary hand to weapon-mounted when non-melee weapon is equipped.
  * Primary hand to offhand switches automatically when using melee weapons.


## Planned Features

* Visible physical flashlight model in the player’s hand.
* Restrict head-mounted mode to specific headgear (helmets).
* Restrict weapon-mounted mode to weapons with flashlight attachments (integration with other mods).




## Installation

### Requirements
* [F4SEVR](https://f4se.silverlock.org/)
* [VR Address Library](https://www.nexusmods.com/fallout4/mods/64879)
* [FRIK v77+](https://www.nexusmods.com/fallout4/mods/53464)

### Steps
* Install via your mod manager, or extract directly into the `Data` folder.
* No ESP file; load order does not matter.


## Incompatibilities
* **FRIK v76 and below**: no config UI and must manually disable the FRIK flashlight in `FRIK.ini` (`bRemoveFlashlight`).
* Other flashlight mods: TBD.


## Mod Recommendations
* [Comfort Swim VR](https://www.nexusmods.com/fallout4/mods/94647) — Improves Fallout 4 VR swimming mechanics.
* [Virtual Holsters](https://www.nexusmods.com/fallout4/mods/51224) — Realistic body-based holstering.


## Source Code
GitHub: [ArthurHub/F4VR-ImmersiveFlashlight](https://github.com/ArthurHub/F4VR-ImmersiveFlashlight)


## Support

If you like my work, consider helping out.

[![become a patron](https://theartofdev.wordpress.com/wp-content/uploads/2025/06/become_a_patron_button.png)](https://patreon.com/theartofdev)

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/G2G61G72WH)

## Credits / Thanks

This mod is built on the work of the Fallout VR modding community and public code.
* Ryan, rsm, McKenzie, alandtse, and other *[CommonLibVR](https://github.com/alandtse/CommonLibVR/tree/vr)* contributors.
* RollingRock, alandtse, shizof, and CylonSurfer for open-source mods such as FRIK and Virtual Holsters.
* CylonSurfer for the original flashlight head/hand switching implementation in FRIK, which served as inspiration.
* Existing flashlight mods, including *[Pip-Boy Flashlight](https://www.nexusmods.com/fallout4/mods/10840)*, for design inspiration.
