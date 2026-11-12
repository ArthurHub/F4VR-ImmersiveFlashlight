# Immersive Flashlight VR

[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg?style=flat-square)](https://github.com/RichardLitt/standard-readme)
[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg?style=flat-square)](LICENSE)

> A Fallout 4 VR F4SE plugin that replaces the Pip-Boy flashlight with a light you hold in either hand, wear on your head, or mount on your weapon.

The flashlight is a physical object: grab it off your belt, bring it to your head, or move it onto your gun. Each mount has its own beam, every setting applies live, and NPCs notice where you point it. It is an F4SE plugin (`ImmersiveFlashlightVR.dll`) built on the [F4VR Common Framework](external/F4VR-CommonFramework).

## Table of Contents

- [Features](#features)
- [Install](#install)
- [Usage](#usage)
- [Development](#development)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [Support](#support)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Features

- **Five mounts:** either hand, the head, the power-armor helmet, and the weapon. A drawn ranged weapon takes the light onto the gun, and a two-handed grip moves it there from the offhand.
- **A beam per mount:** intensity, distance, spread, color and gobo, with a glow cone at the lens that matches the beam.
- **Gestures:** a flashlight on your belt to grab and put back, the offhand to your head for a cap lamp, and the offhand to your primary hand for the hand or the weapon.
- **Optional restrictions:** the head light can need a helmet, and the weapon light a gun with a modeled flashlight.
- **NPC light detection:** the beam makes you visible and draws NPCs, depending on where you aim.
- **In-game settings** in Mods Config FO4 VR, with an in-world beam tuning panel. All of it applies live.

## Install

### Requirements

- [F4SE VR](https://f4se.silverlock.org/)
- [VR Address Library for F4SEVR Plugins](https://www.nexusmods.com/fallout4/mods/64879)
- [FRIK v77+](https://www.nexusmods.com/fallout4/mods/53464) (v78+ for the flashlight model in your hand)
- [Mods Config FO4 VR](https://www.nexusmods.com/games/fallout4/mods/110945) for the in-game settings. Without it, the settings are only in the config file.

Install the [Nexus Mods](https://www.nexusmods.com/fallout4/mods/99342) package with your mod manager, or extract it into the game's `Data` folder. There is no ESP, so load order doesn't matter.

### Compatibility

- [ROCK](https://github.com/brunocatani/ROCK) v1.0+ is supported: the light follows its two-handed grips and offhand weapons. Older ROCK versions are not.
- Run only one flashlight replacement at a time.
- Don't run [Flashlight Stealth Fix](https://www.nexusmods.com/fallout4/mods/76586) together with NPC light detection.
- [VR FPS Stabilizer](https://www.nexusmods.com/fallout4/mods/65961) lowers the shadow quality, which breaks the beam's shadows.

## Usage

Reach to the flashlight on your belt and tap the trigger to take it. Bring your offhand to your head to wear it, or to your primary hand to move it there or onto your weapon. Open Mods Config FO4 VR (hold both thumbsticks pressed in) for the settings. Set the game's Shadow Quality to **HIGH**.

- [Usage and Configuration Guide](docs/README.md): gestures, bindings, settings and restrictions
- [FAQ and Troubleshooting](docs/faq.md)
- [Changelog](docs/changelog.md)

## Development

```sh
git clone https://github.com/ArthurHub/F4VR-ImmersiveFlashlight.git
cd F4VR-ImmersiveFlashlight
git submodule update --init --recursive
cmake --preset default
cmake --build --preset release
```

It needs Visual Studio 2022 or 2026 (x64, C++ desktop workload), CMake 4.2+ and `VCPKG_ROOT`. Prerequisites, packaging and copying to the game are in [docs/development.md](docs/development.md).

## Maintainers

- [@ArthurHub](https://github.com/ArthurHub)

## Contributing

PRs accepted. For larger changes, please open an issue first to discuss what you would like to change.

Code style is enforced by clang-format for C++ and Prettier for JSON through pre-commit: run `pre-commit install` once after cloning. The architecture is in [CLAUDE.md](CLAUDE.md) and [docs/tech/architecture.md](docs/tech/architecture.md).

## Support

If you like my work, consider helping out.

[![become a patron](https://theartofdev.wordpress.com/wp-content/uploads/2025/06/become_a_patron_button.png)](https://patreon.com/theartofdev)

## Acknowledgements

This mod is built on the work of the Fallout VR modding community and public code.

- Ryan, rsm, McKenzie, alandtse, devartificial, and the other [CommonLibVR](https://github.com/alandtse/CommonLibVR/tree/vr) contributors.
- RollingRock, alandtse, shizof, and CylonSurfer for open-source mods such as FRIK and Virtual Holsters.
- CylonSurfer for the original flashlight inspiration in FRIK, and help with Blender.
- Existing flashlight mods, including [Pip-Boy Flashlight](https://www.nexusmods.com/fallout4/mods/10840), for design inspiration and flashlight asset.
- [Flashlight Stealth Fix](https://www.nexusmods.com/fallout4/mods/76586) for inspiration on handling NPCs detection.
- DevArtificial for ray tracing RE and many features adopted from [ROCK](https://github.com/brunocatani/ROCK).

## License

[GPL-3.0](LICENSE) © 2025 Arthur T
