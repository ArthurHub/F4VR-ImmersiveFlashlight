# Development

Build and contribution notes for the Immersive Flashlight VR F4SE plugin. For the components and architecture, see [CLAUDE.md](../CLAUDE.md) and [tech/architecture.md](tech/architecture.md).

## Requirements

### Runtime

- [F4SE/VR](https://f4se.silverlock.org/)
- [VR Address Library for F4SEVR Plugins](https://www.nexusmods.com/fallout4/mods/64879)

### Build tools

- [Visual Studio](https://visualstudio.microsoft.com/) 2022 or 2026, x64, with the **Desktop development with C++** workload
- [CMake](https://cmake.org/) 4.2+
- [vcpkg](https://github.com/microsoft/vcpkg) with the `VCPKG_ROOT` environment variable set
  - `git clone https://github.com/microsoft/vcpkg.git`
  - run `bootstrap-vcpkg.bat`. Example: `C:\github\vcpkg\bootstrap-vcpkg.bat`
  - set the environment variable. Example: `setx VCPKG_ROOT "C:\github\vcpkg"`

## Building

Clone the repo and initialize the [F4VR-CommonFramework](https://github.com/ArthurHub/F4VR-CommonFramework) submodule:

```
git clone https://github.com/ArthurHub/F4VR-ImmersiveFlashlight.git
cd F4VR-ImmersiveFlashlight
git submodule update --init --recursive
```

Configure with the default preset (generates the Visual Studio solution in `build/`):

```
cmake --preset default
```

The `default` preset targets Visual Studio 2026; for VS 2022 add a preset inheriting the hidden `vs2022` preset.

Then either build in Visual Studio or from the command line:

- **Visual Studio**: open `build/ImmersiveFlashlightVR.slnx`, then build and debug as usual. Make project changes in `CMakeLists.txt`, not the generated VS project files.
- **Command line**:

  ```
  cmake --build --preset release
  cmake --build --preset debug
  ```

  The `release` / `debug` build presets build the `default` configure preset; with another configure preset, use `cmake --build build --config Release` (or `Debug`).

A normal build does not package. To produce the versioned `.7z` of the DLL and mod data in `build/package`, build the `package_mod` target:

```
cmake --build --preset package
```

(or `cmake --build build --config Release --target package_mod`). It builds the plugin first if needed.

### Auto-copy to the game (optional)

To copy the built DLL/PDB into your game or mod-manager folder on each build, copy `CMakeUserPresets.json.template` to `CMakeUserPresets.json`, set `COPY_PLUGIN_BASE_PATH` to your Fallout 4 VR `Data` folder or MO2 mod folder, then configure with that preset:

```
cmake --preset custom
cmake --build --preset custom-debug   # or custom-release, or custom-package to also package the mod
```

Set `COPY_PLUGIN_CONFIGURATIONS` in that preset to limit the copy to some build configurations: `all` (the default) or a `;` list such as `"Release"` or `"Debug;RelWithDebInfo"` (case-insensitive).

## Code style

clang-format enforces C++ style via `.clang-format`, and Prettier enforces JSON style via `.prettierrc.json` (180 columns, two-space indentation, CRLF). After cloning, run `pre-commit install` once so the checks run automatically on every commit.

JSON specs under `data/mod/` are formatted; exported JSON configs under `data/config/` keep their native layout. The hooks pin Prettier 3.7.4 and Node 26.13.0, and the existing maintenance workflow runs the same checks on GitHub.

## Testing

There is no unit test suite. Testing is in game.

Before a release, run the end-to-end tests in [tests/e2e/](../tests/e2e/README.md) against the running game (needs [devbench](https://github.com/ArthurHub/devbench) and `pip install -r tests/e2e/requirements.txt`):

```powershell
pytest tests/e2e
pytest tests/e2e -m "not powerarmor"  # KabutoVR enabled (it blocks the scripted exit)
```

- **Covers:** every light location and beam, every gesture's state rules (through the devbench `gesture` action), the headgear and weapon-lamp restrictions with their fallback, menu blocking, the config UI, session overrides, stepping into and out of power armor, frame time, and errors in the log.
- **Before running:** be at the main menu, or in QASmoke. The run starts a game there and adds a helmet and a weapon itself.
- **Leaves:** the mod's config files as they were (backed up through devbench and put back). Nothing reaches a save.
- **Writes:** an HTML report in `build/e2e/`. The tests README has the manual checklist of what needs the headset.

## Tips

- Reference wikis:
  - [Skyrim CommonLib - Getting Started](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE/wiki/Getting-Started)
  - [Skyrim CommonLib - Query and Load](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE/wiki/Query-and-Load)

- Logs are in `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE`:
  - `ImmersiveFlashlightVR.log` for the main log
  - `crash-<date time>.log` for crash logs

- To attach a debugger and use breakpoints, see [Modding Tip: Attaching a Debugger to a Steam Game](https://theartofdev.com/2025/08/04/modding-tip-attaching-a-debugger-to-a-steam-game/):
  - get the latest [Steamless](https://github.com/atom0s/Steamless/releases)
  - unpack `Fallout4VR.exe` (use the default settings)
  - rename `Fallout4VR.exe.unpacked.exe` to `Fallout4VR.exe`
  - add the `-ForceSteamLoader` argument when running the game via `f4sevr_loader.exe` (or you will get "buffout was loaded too late")

- Open the Windows environment-variables editor with `rundll32.exe sysdm.cpl,EditEnvironmentVariables`
