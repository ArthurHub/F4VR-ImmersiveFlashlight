## v1.3

- Flashlight: The hand-held flashlight and a weapon's lamp now show a glow cone that matches the beam's spread, color, and brightness, and follows the beam live as you tune it. Its brightness and width are adjustable in the INI.
- ROCK: Updated to ROCK's modular API. ROCK 0.9 dropped the old API, so the mod had stopped detecting ROCK's weapon grips. Older ROCK versions without the modular API are no longer supported.
- Fixed the in-hand beam not lining up with the flashlight model. The light now comes out of the model's lens.
- Fixed the primary-hand gesture zone reacting to the offhand on a two-handed weapon's foregrip, where it showed its icon and a long-press pulled the light off the weapon. The two-handed toggle handles the light there.
- Fixed a brief stutter the first time a gesture icon or the config UI is shown.

## v1.2

- Pip-Boy: The flashlight now stays on while the Pip-Boy is open, as it already did in power armor. It can be turned off in the INI for the vanilla behavior.
- Config: Redesigned the in-game config UI. The main menu lists the controller button each gesture uses, and the beam tuning screen shows the thumbsticks that tune it.
- Config: The gobo and color buttons show the current gobo texture and beam color with the preset's name, and beam tuning shows the three tuned values on screen, highlighting the one you just changed.
- Config: Beam tuning updates the lit flashlight live instead of flicking it off and on, and each tuning step ticks the controller. INI changes apply to a lit flashlight without the toggle sounds.
- Gestures: The chest grab zone and the primary-hand zone show a small flashlight icon while your hand is inside, replacing the chest's sphere. Each gesture's icon and sphere look (style preset, color, glow, size) is configurable in the INI.

## v1.1.1

Fixed the mod failing to connect to newer FRIK versions that extend the FRIK API resulting in no flashlight in hand.

## v1.1

- ROCK: Added support for [ROCK](https://github.com/brunocatani/ROCK) weapon handling alongside FRIK. A two-handed grip from either mod puts the light on the weapon.
- ROCK: The free primary hand can hold the light while the offhand handles the weapon on its own.
- Fixed a crash when starting a new game or loading a save under certain conditions.
- Fixed flashlight gestures working while some game menus were open (workbench, container, etc.).
- Fixed flashlight gestures working before the Pip-Boy is picked up. The light uses vanilla behavior until you get the Pip-Boy in Vault 111.

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
