# CLAUDE.md

Guidance for Claude Code in this repository.

## Project

A C++23 F4SE plugin for Fallout 4 VR that replaces the Pip-Boy flashlight. The light mounts on the head, the power-armor head, either hand or the weapon, and each mount has its own beam (intensity, radius, FOV, color, gobo).

- **Gestures:** a flashlight model stowed on the belt is grabbed into a hand and put back to turn it off. The offhand near the HMD puts it on the head. The offhand near the primary hand moves it between the hands and onto the weapon.
- **Optional gates:** the head light can require headgear, and the weapon light a modeled lamp on the weapon.
- **NPCs** notice the beam.
- **Config:** Mods Config FO4 VR edits the settings. Its Beam page opens an in-world beam tuning screen.

Feature behavior and its traps: [docs/tech/architecture.md](docs/tech/architecture.md). Build setup, auto-copy, packaging, logs and debugging: [docs/development.md](docs/development.md).

## Build

```bash
cmake --preset default          # VS 2026 (inherit the hidden vs2022 preset for VS 2022)
cmake --build --preset release  # or debug; tracy = Release + Tracy zones
cmake --build --preset package  # versioned .7z of the DLL + data/mod in build/package
```

- Needs `VCPKG_ROOT`, MSVC x64, and `git submodule update --init --recursive`.
- The vcpkg baseline follows the framework's, as CommonLibF4 is built against it. Change it only together with the framework.
- There is no unit test suite: testing is in game. Before a release, run `pwsh tools/release-check.ps1` with the game running. It drives the mod through devbench and needs KabutoVR disabled, or `-SkipPowerArmor`. See [docs/development.md](docs/development.md#testing).

## Code style

- clang-format (`.clang-format`, 180 columns, CRLF), run through pre-commit.
- All code is in `namespace ImFl`; the FRIK client is `frik::api`.
- camelCase functions and variables, PascalCase types.
- Logging through `logger::trace/debug/info/warn/error`.
- [src/PCH.h](src/PCH.h) is the precompiled header.

## Architecture

[src/FlashlightMod.cpp](src/FlashlightMod.cpp) is the `ModBase` subclass:

- `onModLoaded()` sets up devbench, blocks FRIK's flashlight and registers the Mods Config client.
- `onGameLoaded()` patches the mining-helmet keyword, initializes `RestrictionHandler` / `WeaponGripHandler`, creates `Flashlight` and `BeamScreen`, and initializes the FRIK API.
- `onFrameUpdate()` drives `Flashlight`, then `BeamScreen`.

| Component | Responsibility |
| --- | --- |
| [Flashlight](src/Flashlight.cpp) | Per-frame driver: light placement, on/off, PA transitions, the three activation spheres and the gestures, the zone-less two-handed weapon toggle. Owns the mesh helpers. |
| [FlashlightState](src/FlashlightState.cpp) | Static runtime state: the location, grip style and active beam values. Location switching and `refreshLightValues(mode)`. |
| [Utils](src/Utils.cpp) | Raw light on/off, gobo cache, menu gating, the vanilla-toggle disable and the keep-on-in-Pip-Boy patch. |
| [LiveLight](src/LiveLight.cpp) | Refreshes the live engine light in place, or re-creates it. See [pipboy-light-internals.md](docs/tech/pipboy-light-internals.md). |
| [FlashlightMesh](src/FlashlightMesh.cpp) / [BodyFlashlightMesh](src/BodyFlashlightMesh.cpp) / [OnWeaponBeamMesh](src/OnWeaponBeamMesh.cpp) / [BeamGlowMesh](src/BeamGlowMesh.cpp) | Node lifecycles: the in-hand model (roots the light at its lens), the stowed belt model, the glow at the weapon's lamp, and the visible beam cone. |
| [RestrictionHandler](src/RestrictionHandler.cpp) | The headgear and weapon-lamp gates, Pip-Boy availability, and `enforceRestrictions()` turning the light off. |
| [WeaponGripHandler](src/WeaponGripHandler.cpp) | Which hand holds the weapon (FRIK / ROCK), ROCK's post-solve callback and trigger remap. The ROCK SDK headers in [src/api/ROCK/](src/api/ROCK/) are copied as-is. |
| [NpcDetectionHandler](src/NpcDetectionHandler.cpp) | NPCs react to the beam. See [npc-light-detection.md](docs/tech/npc-light-detection.md). |
| [BeamScreen](src/config-mode/BeamScreen.cpp) | In-world beam tuning per location, built from framework `vrui` panels. Hands its edits to Mods Config. |
| [Config](src/Config.cpp) | Typed TOML values, per-location beam settings, hot-reload. |
| [devbench](src/devbench/) | The mod's state, events and actions of the `immersiveflashlightvr` devbench tool. |

The framework (`external/F4VR-CommonFramework`) provides `WandActivationSphere`, `vrui`, input bindings, config, Mods Config previews, devbench and perf. Each has a README under `external/F4VR-CommonFramework/src/<area>/`.

## Rules

- **Light state** changes only through `Utils::` (on/off) and `FlashlightState::` (location, beam values). A gesture that turns the light on into a new location switches the location first, so the light comes on with that location's values.
- **Scene graph:** attach nothing under the player's 3D, which character creation rebuilds (a node of ours in it crashes the game). Hang a node under the VR primary-hand UI attach node and place it each frame.
- **Game thread:** engine calls and light refreshes run there. A config reload, even one from the file watcher's thread, is applied in `Flashlight::onFrameUpdate()` (`consumeValuesReloaded()`).
- **vrui handlers** fire mid-`UIManager` iteration: set a flag and attach or detach in the next frame update.
- **Handedness:** zones and transforms are authored right-handed, and the caller mirrors them (Z) for a left-handed player.

## Config

- [data/config/ImmersiveFlashlightVR.toml](data/config/ImmersiveFlashlightVR.toml) ships embedded in the DLL and is hot-reloaded while the game runs.
- `[ImmersiveFlashlightVR]` holds the common settings. Each advanced feature has its own `[ImFl_*]` section, and `[Debug]` the logging.
- Key prefixes give the type: `b` `i` `f` `s`, `t` transform, `c` color, `k` binding, `h` hand pose, `a` list.
- A key's section is the `SECTION_*` constant (or `DEFAULT_SECTION`) it is read with in `Config::loadConfigInternal()`. A moved key needs the same section where it is written.
- Bumping `iVersion` rewrites a user's file into the shipped layout, keeping values that match by section and key. A moved key resets to its default.
- [data/mod/ModsConfigFO4/Config/ImmersiveFlashlightVR/spec.json](data/mod/ModsConfigFO4/Config/ImmersiveFlashlightVR/spec.json) is the Mods Config spec. A setting's id is `<key>:<section>`, and its `default` must match the TOML's. A key added, renamed or moved in the TOML needs the same change here.

## Reference

The F4VR modding reference library is at `C:\Stuff\GitHub\Mine\Modding-Reference\F4VR`. Its `CLAUDE.md` indexes it. Consult it for CommonLibF4VR types, VR addresses, PlayerNodes offsets and hand-attachment techniques.
