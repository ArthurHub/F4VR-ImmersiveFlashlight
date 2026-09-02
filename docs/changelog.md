## v1.0

- Flashlight: Added a visible flashlight model held in your hand, with forward (thumb-up) and overhand (ice-pick) grips auto-detected from how you tilt the controller, or locked to one.
- Flashlight: Added a beam-less model stowed on your chest that you physically grab into your hand, and put back to turn the light off. The flashlight location is now tracked separately in and out of power armor.
- Gestures: Bring your offhand near your head to toggle the light as a cap lamp, or near your primary hand (or the flashlight on your gun) to move it to the hand or weapon; long-press either to pull the light back to your offhand.
- Gestures: Every gesture's binding, zone, haptics, and optional visible tuning sphere is configurable, and any gesture can be disabled.
- NPC Detection: NPCs now notice the flashlight beam, which vanilla ignores entirely for stealth. Carrying a lit flashlight makes you somewhat visible, putting the beam on someone makes you much more so, and a bright beam in a hostile's face at close range means they've seen you.
- Restrictions: Added an optional headgear requirement that gates the head-mounted light behind worn headgear, either any headgear or only light-capable helmets.
- Restrictions: Added an optional weapon-flashlight requirement that gates the weapon-mounted light behind a weapon carrying a modeled flashlight, rooting the beam and its glow at that lamp. Auto-detects supported weapon mods such as Tactical Weapon Mods.
- Config: Split the in-game UI into a main menu plus beam and misc screens, with buttons to open the INI or the online docs. The on-weapon beam can now be tuned without a weapon in hand.
- Config: Added an option to disable the vanilla flashlight toggle so only the mod's gestures control the light, and reorganized the INI into a common section plus per-feature advanced sections.

## v0.9.1

- Power armor: Added separate configuration for the power-armor helmet lamp.
- Power armor: Fixed the vanilla bug where the flashlight turned off when entering or exiting power armor.
- Power armor: Fixed a stale flashlight location when opening the config after entering or exiting power armor.
- Added a toggle for flashlight shadows.
- Added a warning when VR FPS Stabilizer is installed, as it can degrade flashlight shadows.

## v0.9

- Fixed the gobo textures being loaded from the wrong folder.
- Improved gobo texture loading with error handling and file-extension filtering.
- Documented how to disable shadows in the INI.

## v0.8

- Initial release.
- Flashlight in either hand, on the head, or mounted on a weapon, with automatic switching when a weapon is drawn.
- Independent beam settings per location: intensity, distance, spread, color, and gobo texture.
- In-game VR configuration UI, opened from the FRIK config menu and applied live.
- Gobo (beam pattern) texture support, loaded from a folder.
- INI changes are live-loaded into the running game.
