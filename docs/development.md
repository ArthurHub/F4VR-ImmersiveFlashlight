# Development

Build and contribution notes for the Immersive Flashlight VR F4SE plugin. For an overview of the components and architecture, see [CLAUDE.md](../CLAUDE.md).

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
  cmake --build build --config Release
  cmake --build build --config Debug
  ```

Release builds automatically produce a `.7z` package of the DLL and mod data in `build/package`.

### Auto-copy to the game (optional)

To copy the built DLL/PDB into your game or mod-manager folder on each build, copy `CMakeUserPresets.json.template` to `CMakeUserPresets.json`, set `COPY_PLUGIN_BASE_PATH` to your Fallout 4 VR `Data` folder or MO2 mod folder, then configure with that preset:

```
cmake --preset custom
```

## Code style

clang-format enforces style via `.clang-format`. After cloning, run `pre-commit install` once so clang-format runs automatically on every commit.

## Testing

There is no automated test suite — testing is manual, via in-game gameplay and the in-game configuration UI.

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
