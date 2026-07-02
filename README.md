# Immersive Flashlight VR

[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg?style=flat-square)](https://github.com/RichardLitt/standard-readme)
[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg?style=flat-square)](LICENSE)

> A Fallout 4 VR F4SE plugin that replaces the Pip-Boy flashlight with a versatile, fully configurable light you can hold in hand, mount on your head, or attach to your weapon.

Immersive Flashlight VR turns the flat Pip-Boy light into a physical object you interact with in VR. Carry it in either hand, clip it to your head like a cap lamp, or mount it on your gun, and tune the beam independently for each mode. Location switching is physical and gesture-driven, with sensible, context-aware restrictions, and every setting is configured in-game and applied live. It runs as an F4SE plugin DLL (`ImmersiveFlashlightVR.dll`) and uses [F4VR Common Framework](external/F4VR-CommonFramework) for plugin lifecycle, VR controller input, config hot-reload, VR UI widgets, and shared game utilities.

## Table of Contents

- [Background](#background)
- [Install](#install)
- [Usage](#usage)
- [Development](#development)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [Support](#support)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Background

Fallout 4 VR ships with a single Pip-Boy flashlight that is awkward to control and fixed in behavior. Immersive Flashlight VR replaces it with a light you physically move between your hand, head, and weapon, with separate beam settings per mode so you can explore the wasteland at night with a powerful long-range beam, then switch to a soft, wide head lamp for close-quarters looting.

What it provides:

- **Five mounting positions** - in either hand, on the head, on the power-armor helmet, and weapon-mounted, with automatic, context-aware switching (a drawn ranged weapon routes the light onto the gun; melee frees the hand).
- **Per-mode beam tuning** - independent intensity, distance (radius), spread (FOV), RGB color, and gobo texture for every location.
- **Physical interaction** - a grabbable flashlight model stowed on your body, a bring-offhand-to-head gesture for the head lamp, and an offhand-to-primary-hand gesture to move the light to your hand or weapon.
- **Optional restrictions** - gate the head lamp behind worn headgear (any, or specific light-capable helmets), and gate the weapon light behind a weapon that actually carries a modeled flashlight, rooting the beam at that mesh.
- **In-game VR configuration** - a multi-screen VR UI for beam tuning, location toggles, color/gobo presets, and global options, all applied live with no restart.

## Install

### Requirements

- [F4SE VR](https://f4se.silverlock.org/)
- [VR Address Library for F4SEVR Plugins](https://www.nexusmods.com/fallout4/mods/64879)
- [FRIK v77+](https://www.nexusmods.com/fallout4/mods/53464)

### Recommended install

Download from [Nexus Mods](https://www.nexusmods.com/fallout4/mods/99342) and install the published mod package through your mod manager, or extract it directly into the game's `Data` folder. There is no ESP file, so load order does not matter.

### Compatibility

- **FRIK v76 and below** is not supported - the in-game config UI and the FRIK flashlight integration require FRIK v77+.
- Other flashlight mods that drive the Pip-Boy light may conflict; run only one flashlight replacement at a time.

## Usage

Move the light physically with VR gestures: reach to the flashlight stowed on your chest and grab it into a hand, bring your offhand near your head to mount it as a cap lamp, or near your primary hand to move it to the hand or the equipped weapon. Beam settings are tuned in-game through the FRIK configuration UI (hold both thumbsticks for ~2 seconds, then select Immersive Flashlight) and applied live, with advanced options in the INI. The vanilla Pip-Boy light toggle is disabled by default so only the mod's gestures control the light.

See the **[Usage and Configuration Guide](docs/README.md)** for the full gesture reference, default bindings, beam settings, restrictions, and advanced INI configuration.

### Documentation

- [Usage and Configuration Guide](docs/README.md)
- [FAQ and Troubleshooting](docs/faq.md)
- [Development](docs/development.md)

## Development

### Prerequisites

- `VCPKG_ROOT` environment variable pointing to a [vcpkg](https://github.com/microsoft/vcpkg) installation
  - `git clone https://github.com/microsoft/vcpkg.git`
  - run `bootstrap-vcpkg.bat`. Example: `C:\github\vcpkg\bootstrap-vcpkg.bat`
  - Set environment variable `VCPKG_ROOT`. Example: `setx VCPKG_ROOT "C:\github\vcpkg"`
- Visual Studio 2022 (v143) or 2026 (v145), x64 only, with the C++ Desktop Development workload
- [CMake](https://cmake.org/) 4.2+

### Build

```sh
git clone https://github.com/ArthurHub/F4VR-ImmersiveFlashlight.git
cd F4VR-ImmersiveFlashlight
git submodule update --init --recursive
cmake --preset default
cmake --build build --config Release
```

This generates the Visual Studio solution in `build/`. Open `build/ImmersiveFlashlightVR.slnx` if you prefer building or debugging in Visual Studio. Project configuration belongs in `CMakeLists.txt`, not the generated VS project files. Release builds automatically produce a `.7z` package in `build/package`.

For local post-build copying, see `CMakeUserPresets.json.template` and set `COPY_PLUGIN_BASE_PATH` to your MO2 mod folder or Fallout 4 VR `Data` folder. More development notes are in [docs/development.md](docs/development.md).

## Maintainers

- [@ArthurHub](https://github.com/ArthurHub)

## Contributing

PRs accepted. For larger changes, please open an issue first to discuss what you would like to change.

Code style is enforced by clang-format and pre-commit. After cloning, run `pre-commit install` once so local checks run before commits. See [CLAUDE.md](CLAUDE.md) for repository architecture, build expectations, and coding conventions.

## Support

If you like my work, consider helping out.

[![become a patron](https://theartofdev.wordpress.com/wp-content/uploads/2025/06/become_a_patron_button.png)](https://patreon.com/theartofdev)

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/G2G61G72WH)

## Acknowledgements

This mod is built on the work of the Fallout VR modding community and public code.

- Ryan, rsm, McKenzie, alandtse, and the other [CommonLibVR](https://github.com/alandtse/CommonLibVR/tree/vr) contributors.
- RollingRock, alandtse, shizof, and CylonSurfer for open-source mods such as FRIK and Virtual Holsters.
- CylonSurfer for the original flashlight head/hand switching implementation in FRIK, which served as inspiration.
- Existing flashlight mods, including [Pip-Boy Flashlight](https://www.nexusmods.com/fallout4/mods/10840), for design inspiration.

## License

[GPL-3.0](LICENSE) © 2025 Arthur T
