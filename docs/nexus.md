# Short description

A highly versatile and configurable Pip-Boy flashlight replacement mod that allows the player to hold the flashlight in hand, attach it to the head, or mount it on a weapon, with separate per-mode settings for beam intensity, distance, spread, color, and gobo.

# Full description

[size=3]A VR-first replacement for the Pip-Boy flashlight supporting in-hand, head-mounted, and weapon-mounted use. Each mode is independently configurable, providing precise control over beam intensity, distance, spread, color, and gobo. Location switching is physical and immersive, with sensible, context-aware restrictions. All configuration is performed in-game and applied live, with no reloads, restarts, or re-installs required.[/size]
[size=3]
Explore the wasteland at night with a powerful, long-range flashlight for visibility at a distance, then switch to a soft, wide head-mounted beam for close-quarters exploration and looting.[/size]

[size=4][b]Usage[/b][/size]
[list]
[\*][size=3]Toggle flashlight: Long-press the off-hand trigger (default F4VR behavior).[/size][/*]
[\*][size=3]Switch flashlight location: Bring your hand close to your head or your other hand until haptic feedback is felt, then press grab.[/size][/*]
[\*][size=3]Configure settings: Open via FRIK configuration UI (hold both thumbsticks for 2 seconds).[/size][/*]
[\*][size=3][b]Important: [/b]Set game "Shadows Quality" to "HIGH" to prevent corrupted shadows ([url=https://github.com/ArthurHub/F4VR-ImmersiveFlashlight/wiki/FAQ-%5C-Troubleshoot#shadows-problems]details and how to disable shadows[/url]﻿).[/size][/*]
[/list]

[size=4][b]Features[/b][/size]
[size=3]Flashlight Modes:[/size]
[list]
[\*][size=3]In-hand flashlight: Held in the player’s left or right hand, behaving like a handheld flashlight directed by hand movement.[/size][/*]
[\*][size=3]Head-mounted flashlight (cap lamp): Mounted on the player’s head and following head orientation.[/size][/*]
[\*][size=3]Weapon-mounted flashlight: Attached to non-melee weapons and aligned with weapon direction, including two-handed grip support.[/size][/*]
[/list]
[size=3]Beam Customization:[/size]
[list]
[\*][size=3]Live, in-game tuning via a simple UI.[/size][/*]
[\*][size=3]Per-mode configuration (+ separate for Power Armor head) allowing each flashlight type to behave differently:[/size]
[list]
[\*][size=3]Intensity – brightness of the beam.[/size][/*]
[\*][size=3]Distance – effective illumination range.[/size][/*]
[\*][size=3]Spread – beam field of view.[/size][/*]
[\*][size=3]Color – RGB beam color.[/size][/*]
[\*][size=3]Gobo – beam texture / pattern.[/size][/*]
[/list]
[/*]
[/list]
[size=3]Immersion & Behavior:[/size]
[list]
[\*][size=3]Physical switching between head, hand, and weapon locations.[/size][/*]
[\*][size=3]Automatic switching:[/size]
[list]
[\*][size=3]Primary hand to weapon-mounted when non-melee weapon is equipped.[/size][/*]
[\*][size=3]Primary hand to offhand switches automatically when using melee weapons.[/size][/*]
[/list]
[/*]
[/list]

[size=4][b]Planned Features[/b][/size]
[list]
[\*][size=3]Visible physical flashlight model in the player’s hand.[/size][/*]
[\*][size=3]Restrict head-mounted mode to specific headgear (helmets).[/size][/*]
[\*][size=3]Restrict weapon-mounted mode to weapons with flashlight attachments (integration with other mods).[/size][/*]
[/list]

[size=4][b]Advanced Configuration[/b][/size]
Most configuration is stored in "ImmersiveFlashlightVR.ini" file. 
Much more than can be configured via in-game configuration interface.
Any changes made in the INI file will be live loaded into the running game to experience the effects.
Location: 
[code]%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR[/code]

[b][size=4]Installation[/size][/b]
[size=3]Requirements[/size]
[list]
[\*][url=https://f4se.silverlock.org/][size=3]F4SEVR[/size][/url][/*]
[\*][url=https://www.nexusmods.com/fallout4/mods/64879][size=3]VR Address Library[/size][/url][/*]
[\*][url=https://www.nexusmods.com/fallout4/mods/53464][size=3]FRIK v77+[/size][/url][/*]
[/list]
[size=3]Steps[/size]
[list]
[\*][size=3]Install via your mod manager, or extract directly into the Data folder.[/size][/*]
[\*][size=3]No ESP file; load order does not matter.[/size][/*]
[/list]
[size=3]Incompatibilities[/size]
[list]
[\*][size=3][url=https://www.nexusmods.com/fallout4/mods/65961]VR FPS Stabilizer[/url]﻿ may cause shadows issues (probably due to lowering shadow quality)[/size][/*]
[\*][size=3]FRIK v76 and below: no config UI and must manually disable the FRIK flashlight in FRIK.ini (bRemoveFlashlight).[/size][/*]
[\*][size=3]Other flashlight mods: TBD.[/size][/*]
[/list]

[center][size=4][b]Showcase[/b][/size][/center]
[center][youtube]dTvVdf9Y9jY[/youtube][/center]
[center][size=4][b]Configuration[/b][/size][/center]
[center][youtube]GkOjVrN6_4g[/youtube][/center]

[size=4][b]Mod Recommendations[/b][/size]
[list]
[\*][size=3][url=https://www.nexusmods.com/fallout4/mods/94647]Comfort Swim VR[/url] — Improves Fallout 4 VR swimming mechanics.[/size][/*]
[\*][size=3][url=https://www.nexusmods.com/fallout4/mods/51224]Virtual Holsters[/url] — Realistic body-based holstering.[/size][/*]
[/list]

[size=4][b]Source Code[/b][/size]
[size=3]GitHub: [url=https://github.com/ArthurHub/F4VR-ImmersiveFlashlight]ArthurHub/F4VR-ImmersiveFlashlight[/url][/size]

[size=4][b]Support[/b][/size]
If you like my work, consider helping out.
[url=https://patreon.com/theartofdev][img]https://camo.githubusercontent.com/932ed7c82e6cf6c653059af7f533f1aba02ce4f3da0238f3a95da5815985ee2f/68747470733a2f2f7468656172746f666465762e776f726470726573732e636f6d2f77702d636f6e74656e742f75706c6f6164732f323032352f30362f6265636f6d655f615f706174726f6e5f627574746f6e2e706e67[/img][/url]
[url=https://ko-fi.com/G2G61G72WH][img]https://camo.githubusercontent.com/201ef269611db7eb6b5d08e9f756ab8980df3014b64492770bdf13a6ed924641/68747470733a2f2f6b6f2d66692e636f6d2f696d672f676974687562627574746f6e5f736d2e737667[/img][/url]

[size=4][b]Credits / Thanks[/b][/size]
This mod is built on the work of the Fallout VR modding community and public code.
[list]
[*]Ryan, rsm, McKenzie, alandtse, and other [i][url=https://github.com/alandtse/CommonLibVR/tree/vr]CommonLibVR[/url][/i] contributors.[/*]
[*]RollingRock, alandtse, shizof, and CylonSurfer for open-source mods such as FRIK and Virtual Holsters.[/*]
[*]CylonSurfer for the original flashlight head/hand switching implementation in FRIK, which served as inspiration.[/*]
[*]Existing flashlight mods, including [i][url=https://www.nexusmods.com/fallout4/mods/10840]Pip-Boy Flashlight[/url][/i], for design inspiration.[/*]
[/list]
