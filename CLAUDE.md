# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

F4VR-ImmersiveFlashlight is a **C++23 DLL plugin** for F4SE (Fallout 4 Script Extender) that replaces the VR flashlight system. It supports 5 mounting positions (head, power-armor head, offhand, primary hand, weapon-mounted) with per-mode beam configuration (intensity, radius, FOV, color, gobo textures) and an in-game VR configuration UI.

## Build System

**Prerequisites:**
- `VCPKG_ROOT` environment variable pointing to a vcpkg installation
- Visual Studio 2022 or 2026 (MSVC toolchain, x64)
- Git submodules initialized: `git submodule update --init --recursive`

**Configure and build:**
```bash
cmake --preset default                                    # uses VS 2026; substitute vs2022 preset if needed
cmake --build --preset default --config Release
cmake --build --preset default --config Debug
```

The default preset inherits `cmake-dev + vcpkg + windows + vs2026`. The build output is `build/`. In-source builds are blocked.

**Auto-copy to game (optional):** Pass `-DPOST_BUILD_COPY_PLUGIN=ON -DCOPY_PLUGIN_BASE_PATH=<Fallout4VR_path>` during configure to copy the DLL and PDB to `<path>/F4SE/Plugins/` on each build. Multiple paths can be separated by `;`.

**Release packaging:** Release builds automatically run `cmake/package.cmake` to produce a `.7z` archive with the DLL and mod data files.

**No automated test suite** — testing is manual via in-game gameplay and the configuration UI.

## Architecture

### Plugin Lifecycle

The entry point is [src/ImmersiveFlashlight.h](src/ImmersiveFlashlight.h) / [src/ImmersiveFlashlight.cpp](src/ImmersiveFlashlight.cpp), a `ModBase` subclass that implements the F4SE plugin hooks:
- `F4SEPlugin_Query` / `F4SEPlugin_Load` — standard F4SE registration
- `onGameLoaded()` — creates `Flashlight` and `FlashlightConfigMode` instances, loads config, registers the config button with FRIK, and patches the mining helmet keyword
- `onFrameUpdate()` — drives both the flashlight logic and config UI each frame

### Main Components

| Component | Files | Responsibility |
|-----------|-------|----------------|
| **Flashlight** | [src/Flashlight.h](src/Flashlight.h) / [.cpp](src/Flashlight.cpp) | Per-frame position tracking, on/off detection, PA transition fix, haptic feedback, location switching |
| **FlashlightConfigMode** | [src/FlashlightConfigMode.h](src/FlashlightConfigMode.h) / [.cpp](src/FlashlightConfigMode.cpp) | In-game VR config UI: beam tuning, color picker, gobo selector, shadow toggle |
| **Config** | [src/Config.h](src/Config.h) / [.cpp](src/Config.cpp) | INI load/save via `ConfigBase`, per-location settings, live hot-reload via file watcher |
| **Utils** | [src/Utils.h](src/Utils.h) / [.cpp](src/Utils.cpp) | `setLightValues()`, location switching helpers, gobo texture cache, FRIK hand position getters |
| **FRIK API** | [src/api/FRIKApi.h](src/api/FRIKApi.h) | Header-only client for FRIK mod: finger positions, hand poses, config button registration |

### Location Model

There are two related enums:
- `FlashlightLocation` — 5 runtime positions (Head, PAHead, OffHand, PrimaryHand, OnWeapon)
- `FlashlightConfigLocation` — 4 UI-facing positions (OnWeapon excluded from the config UI)

Config stores independent beam settings (fade, radius, FOV, RGB, gobo path, angle offset) for each location. `Utils::setFlashlightRuntimeLocationOverride()` enables temporary override used during config UI preview.

### Config Hot-Reload

`Config` subscribes to `thomasmonkman-filewatch` change events on the INI file. Changes made externally while the game is running are automatically applied without restart via `refreshFlashlightLocation()` / `setLightValues()`.

### Key INI File

[data/config/ImmersiveFlashlightVR.ini](data/config/ImmersiveFlashlightVR.ini) — the bundled default config. Deployed to the game directory; editable by users. Section `[ImmersiveFlashlightVR]` holds all beam and location settings. Section `[Debug]` controls log level.

### Dependencies

Pulled via vcpkg (`vcpkg.json`) with a **pinned baseline** required for CommonLibF4 compatibility:
- `F4VR-CommonFramework` (git submodule at `external/`) — re-exports CommonLibF4 and the framework base classes
- `spdlog`, `nlohmann-json`, `simpleini`, `thomasmonkman-filewatch`, `args`, `rapidcsv`, `rsm-mmio`, `xbyak`, `cpptrace`

The vcpkg baseline **must not be changed** without verifying CommonLibF4 still builds; it was specifically pinned to `b4a3d89125e45bc8f80fb94bef9761d4f4e14fb9`.

## F4VR Modding Reference Library

A curated reference library lives at **`C:\Stuff\GitHub\Mine\Modding-Reference\F4VR`**. Consult it before implementing any non-trivial F4VR feature. Its `CLAUDE.md` explains the trust-tier system (gold/silver/bronze) and the full file index.

**Quick lookup guide:**

| Task | File to read |
|------|-------------|
| Any CommonLibF4VR type/function | `Analysis/gold/CommonLibF4VR_API_REFERENCE.md` |
| Known bugs & missing APIs in CommonLibF4VR | `knowledge-base/commonlibf4vr_f4sevr_gap_analysis.md` |
| Attaching meshes/nodes to the player's hand | `knowledge-base/item_in_hand_techniques.md` |
| PlayerNodes offsets (hand, head, weapon bones) | `Analysis/gold/FRIK_RE_REFERENCE.md` |
| VR button input blocking / remapping | `knowledge-base/openvr_controller_state_interception.md` |
| Physics, animation, scene graph RVAs | `Analysis/gold/F4VR-CommonFramework_RE_REFERENCE.md` |
| VR globals, MCM pattern, dialogue hooks | `Analysis/gold/Neanka-mods-repo_RE_REFERENCE.md` |
| AddressLib ID → VR address mapping | `Analysis/gold/fallout_vr_address_library_RE_REFERENCE.md` |
| Authoritative VR struct layouts | `Analysis/gold/f4sevr_0_6_21_RE_REFERENCE.md` |
| Full modern plugin source example | `manual-repos/mith077-Daytripper4/` |

**Key patterns documented there:**
- `REL::VariantID(f4ID, ngID, vrRawOffset)` — for VR addresses missing from AddressLib
- `REL::Module::IsVR()` — runtime detection (VR=1.2.72)
- PlayerNodes table at `PlayerCharacter+0x6E0` — 43 bone pointers including `primaryWandNode` (+0x6F0), `HeadLightParentNode` (+0x808), `HmdNode` (+0x7E0)

## Code Style

- **Formatting:** `.clang-format` (LLVM-based) — run clang-format before committing
- **Naming:** camelCase for variables/functions, PascalCase for types/classes
- **Logging:** spdlog throughout; use `logger::trace/debug/info/warn/error` macros from the framework
- **C++ standard:** C++23; use modern features (ranges, constexpr, smart pointers, structured bindings)
- **PCH:** [src/PCH.h](src/PCH.h) is the precompiled header — add widely-used includes there
