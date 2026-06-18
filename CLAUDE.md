# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

F4VR-ImmersiveFlashlight is a **C++23 DLL plugin** for F4SE (Fallout 4 Script Extender) that replaces the VR flashlight system. It supports 5 mounting positions (head, power-armor head, offhand, primary hand, weapon-mounted) with per-mode beam configuration (intensity, radius, FOV, color, gobo textures) and an in-game VR configuration UI. It also stows a beam-less flashlight model on the body that the player can physically grab to turn the light on into a hand (and put back to turn it off).

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

## Code Style

clang-format enforces style (`.clang-format`): LLVM-based, 180-column limit, 4-space indent, CRLF line endings, pointer-left (`T* p`), namespace indentation enabled, braces on new lines for classes/functions/namespaces.

Run formatter: `clang-format -i <file>` or format-on-save in your editor.

After cloning, run `pre-commit install` once to enforce clang-format on every commit (`.pre-commit-config.yaml`); the hook lives in `.git/hooks/` and is not version-controlled.

Conventions:

- **Naming:** camelCase for variables/functions, PascalCase for types/classes
- **Namespace:** all project code lives in `namespace ImFl` (the FRIK client header is `frik::api`)
- **Logging:** spdlog throughout; use `logger::trace/debug/info/warn/error` macros from the framework
- **C++ standard:** C++23; use modern features (ranges, constexpr, smart pointers, structured bindings)
- **PCH:** [src/PCH.h](src/PCH.h) is the precompiled header — add widely-used includes there

## Architecture

### Plugin Lifecycle

The entry point is [src/FlashlightMod.h](src/FlashlightMod.h) / [src/FlashlightMod.cpp](src/FlashlightMod.cpp), a `ModBase` subclass that implements the F4SE plugin hooks:

- `F4SEPlugin_Query` / `F4SEPlugin_Load` — standard F4SE registration
- `onGameLoaded()` — creates `Flashlight` and `FlashlightConfigMode` instances, loads config, registers the config button with FRIK, and patches the mining helmet keyword
- `onFrameUpdate()` — drives both the flashlight logic and config UI each frame

### Main Components

| Component                | Files                                                                                           | Responsibility                                                                                                                                                                  |
| ------------------------ | ----------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Flashlight**           | [src/Flashlight.h](src/Flashlight.h) / [.cpp](src/Flashlight.cpp)                               | Per-frame position tracking, on/off detection, PA transition fix, haptic feedback, location switching                                                                           |
| **FlashlightMesh**       | [src/FlashlightMesh.h](src/FlashlightMesh.h) / [.cpp](src/FlashlightMesh.cpp)                   | Cloned 3D mesh lifecycle for hand modes: attach as child of the wand node, re-apply mesh transform + hand pose on grip-style change, show/hide, `invalidate()` on PA transition |
| **BodyFlashlight**       | [src/BodyFlashlight.h](src/BodyFlashlight.h) / [.cpp](src/BodyFlashlight.cpp)                   | Controls the body-stow feature: owns `BodyFlashlightMesh`, runs the proximity grab/return interaction + grab-button suppression, toggles the light on/off and switches it to the grabbing hand |
| **BodyFlashlightMesh**   | [src/BodyFlashlightMesh.h](src/BodyFlashlightMesh.h) / [.cpp](src/BodyFlashlightMesh.cpp)       | Node-lifecycle helper for the stowed model: clones the beam-less model (hides `Flashlight_lamp_FX`), attaches it to the chest bone (mirrored when left-handed, PA-aware), computes the grab zone (`isWithinGrabSphere()`) from the configured transform, and attaches the `debug-sphere.nif` only when `bDebugShowGrabSphere` is on to visualize the zone; `invalidate()` on PA transition |
| **FlashlightConfigMode** | [src/FlashlightConfigMode.h](src/FlashlightConfigMode.h) / [.cpp](src/FlashlightConfigMode.cpp) | In-game VR config UI: beam tuning, color picker, gobo selector, shadow toggle                                                                                                   |
| **Config**               | [src/Config.h](src/Config.h) / [.cpp](src/Config.cpp)                                           | INI load/save via `ConfigBase`, per-location settings, live hot-reload via file watcher                                                                                         |
| **Utils**                | [src/Utils.h](src/Utils.h) / [.cpp](src/Utils.cpp)                                              | `setLightValues()`, location switching helpers, gobo texture cache, FRIK hand position getters                                                                                  |
| **FRIK API**             | [src/api/FRIKApi.h](src/api/FRIKApi.h)                                                          | Header-only client for FRIK mod: finger positions, hand poses, config button registration                                                                                       |

### Location Model

There are two related enums:

- `FlashlightLocation` — 5 runtime positions (Head, PAHead, OffHand, PrimaryHand, OnWeapon)
- `FlashlightConfigLocation` — 4 UI-facing positions (OnWeapon excluded from the config UI)

Config stores independent beam settings (fade, radius, FOV, RGB, gobo path, angle offset) for each location. `Utils::setFlashlightRuntimeLocationOverride()` enables temporary override used during config UI preview.

The chosen `FlashlightConfigLocation` is tracked separately for out of / in power armor (`iFlashlightLocation` / `iFlashlightLocationInPA`). `Utils::getActiveFlashlightConfigLocation()` picks the variant by `f4vr::isInPowerArmor()`, so `getFlashlightLocation()` resolves the runtime location against the right one and `switchFlashlightConfigLocation()` saves only the current state's value. Entering/exiting PA switches the location smoothly via the per-frame `refreshFlashlightLocation()`.

### Grip Style

Hand-held locations (`InOffhand` / `InPrimaryHand`) have an additional `FlashlightGripStyle` dimension: `Forward` (thumb-up grip, controller top up) and `Overhand` (fist/ice-pick grip, controller flipped around its barrel so the top faces down while the light still points forward). `Utils::refreshGripStyle()` runs each frame and picks the style from `Config::flashlightGripMode` — `Auto` measures the angle between the wand's local +Z and world up and applies hysteresis (`fFlashlightGripOverhandTiltDegrees` + `fFlashlightGripHysteresisDegrees`); `ForwardOnly` / `OverhandOnly` lock the style.

Each grip style has its own light transform (`tFlashlightIn{Offhand,PrimaryHand}Transform[Overhand]`), mesh transform (`tFlashlightMeshTransform[Overhand]`), and FRIK hand pose (`hFlashlightHandPose[Overhand]`). Light transform is re-resolved every frame; mesh transform and hand pose are re-applied by `FlashlightMesh` when the grip style changes mid-attach (no detach/reattach — same parent node).

Each of these in-hand spatial configs also has a power-armor variant (`...PA` suffix) because PA gauntlets are sized/posed differently than bare hands. `Config::getFlashlightInHandLightTransform()`, `getFlashlightMeshTransform()`, and `getFlashlightHandPose()` select the PA or non-PA value by `f4vr::isInPowerArmor()`; each PA key defaults to its non-PA counterpart when omitted from the INI. Beam values (fade/radius/FOV/color/gobo) are **not** split — in-hand beam is shared in and out of PA. On a PA transition the skeleton swaps, so `Flashlight::handlePowerArmorTransition()` calls `FlashlightMesh::invalidate()` to force a detach/reattach that re-resolves the PA transforms and pose.

### Body Stow & Grab

`BodyFlashlight` owns the whole feature; `Flashlight` just delegates to it. `Flashlight::onFrameUpdate()` calls `_bodyFlashlight.onFrameUpdate()` early (it reads the light state itself), then re-reads `isPipboyLightOn()` because a grab/return may toggle the light mid-frame. `BodyFlashlight` owns a `BodyFlashlightMesh` (the stowed model) plus the grab/return state (haptic latch, post-toggle cooldown). Light mutations go through `Utils::` (`turnFlashlightOn`/`turnFlashlightOff`/`switchFlashlightConfigLocation`), so no class but `Utils` touches global flashlight state.

- **Stowed model:** while the flashlight is **off**, `BodyFlashlightMesh` shows a beam-less clone on the chest bone (the in-hand `FlashlightMesh` shows only while on). It stays attached whenever the feature is enabled; only visibility flips. `Flashlight::handlePowerArmorTransition()` / `onGameSessionLoaded()` call `BodyFlashlight::invalidate()` (→ `BodyFlashlightMesh::invalidate()`) on skeleton swaps.
- **Stow location:** the model attaches to the **chest** bone (`f4vr::SkellyBones::Chest`). The model transform (`tFlashlightBodyTransform` + `...PA`) and the grab-sphere transform (`tFlashlightGrabSphereTransform` + `...PA`) are authored for a right-handed player and auto-mirrored (Z translate + heading/roll) when the player is left-handed (`f4vr::isLeftHandedMode()`), so one value works for both handedness modes.
- **Grab zone (no mesh):** `BodyFlashlightMesh::isWithinGrabSphere()` computes the zone's center + radius from `tFlashlightGrabSphereTransform` via the shared `grabSphereBoneTransform()` helper, which offsets the grab translate by `tFlashlightBodyTransform`'s translate so the zone is measured **from the stowed model**, not the bone origin (the codebase's local→world convention from `MatrixUtils::calculateRelocation`). The same helper feeds the debug sphere, so the visual and the test always agree (both apply the model's Z-mirror). Only the model's *translate* shifts the origin — its rotation/scale don't tilt or size the zone. The grab-sphere scale sizes the unit-**diameter** `debug-sphere.nif`, so the grab radius is `scale * boneWorldScale * SPHERE_NIF_BASE_RADIUS` (≈ half the scale). `bDebugShowGrabSphere` loads/attaches that sphere mesh (only while debugging) to visualize the exact zone for tuning; nothing is loaded when it's off.
- **Grab / return** (`BodyFlashlight::checkGrab()`): for each hand whose grab binding isn't `none`, while the wand is inside the grab zone its configured button is suppressed from the game/FRIK via `vrcf::VRControllersSuppress` (owner key `ImFl_BodyGrab`) and a one-shot haptic hints the zone. Firing the binding either turns the light **on** and switches the location to that hand (grab — either hand → that hand), or, when the light is already **on and held in that hand**, turns it **off** (return). A hand occupied by a **drawn weapon** can neither grab nor return — in practice this gates the primary hand (`isNodeVisible(getWeaponNode())`), since you can't take/stow the flashlight with the hand holding the gun/melee. A short post-toggle cooldown (`BODY_GRAB_COOLDOWN_MS`) prevents an immediate grab-then-return. `VRControllersSuppress` is auto-driven each frame by `ModBase`; suppression/release run on the main thread.

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

| Task                                           | File to read                                               |
| ---------------------------------------------- | ---------------------------------------------------------- |
| Any CommonLibF4VR type/function                | `Analysis/gold/CommonLibF4VR_API_REFERENCE.md`             |
| Known bugs & missing APIs in CommonLibF4VR     | `knowledge-base/commonlibf4vr_f4sevr_gap_analysis.md`      |
| Attaching meshes/nodes to the player's hand    | `knowledge-base/item_in_hand_techniques.md`                |
| PlayerNodes offsets (hand, head, weapon bones) | `Analysis/gold/FRIK_RE_REFERENCE.md`                       |
| VR button input blocking / remapping           | `knowledge-base/openvr_controller_state_interception.md`   |
| Physics, animation, scene graph RVAs           | `Analysis/gold/F4VR-CommonFramework_RE_REFERENCE.md`       |
| VR globals, MCM pattern, dialogue hooks        | `Analysis/gold/Neanka-mods-repo_RE_REFERENCE.md`           |
| AddressLib ID → VR address mapping             | `Analysis/gold/fallout_vr_address_library_RE_REFERENCE.md` |
| Authoritative VR struct layouts                | `Analysis/gold/f4sevr_0_6_21_RE_REFERENCE.md`              |
| Full modern plugin source example              | `manual-repos/mith077-Daytripper4/`                        |

**Key patterns documented there:**

- `REL::VariantID(f4ID, ngID, vrRawOffset)` — for VR addresses missing from AddressLib
- `REL::Module::IsVR()` — runtime detection (VR=1.2.72)
- PlayerNodes table at `PlayerCharacter+0x6E0` — 43 bone pointers including `primaryWandNode` (+0x6F0), `HeadLightParentNode` (+0x808), `HmdNode` (+0x7E0)
