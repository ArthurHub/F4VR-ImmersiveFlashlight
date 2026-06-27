## Advanced Configuration
Most configuration is stored in `ImmersiveFlashlightVR.ini` file. Much more than can be configured via in-game configuration interface.
Any changes made in the INI file will be live loaded into the running game to experience the effects.
Location: `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR`

## Shadows Problems
By default, the mod adds shadows to the flashlight for better immersion.
For the shadows to work properly the "Shadow Quality" setting must be set to "HIGH". Otherwise, they look really bad.

Some performance mods like [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961)﻿ may cause issues as they lower the shadow quality while the game is running.

If you prefer not to have flashlight shadows, you can disable them in game via the config menu.

## VR FPS Stabilizer Warning

The mod detects if [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961) is running to warn of potential shadow issue and shows notification every 5 minutes when the flashlight is on. The warning will not show if flashlight shadows are disabled.

If you solved it in another way and want to disable the warning you can do so in `ImmersiveFlashlightVR.ini` by setting:
`bWarnAboutFPSStabilizerMod = false`
