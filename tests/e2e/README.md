# End-to-end tests

pytest tests that drive the running game through devbench's REST port and check the light, the gestures, the restrictions and the mod's health. Run by hand before a release; CI has no game. The layout follows Mods Config's `tests/e2e`.

## Requirements

- The game running with [devbench](https://github.com/ArthurHub/devbench) (a build with its `files`, `input` and `player` tools), at the main menu or already in QASmoke. The tests `coc qasmoke` so nothing reaches a save; any other loaded game is refused.
- This checkout's build deployed: a run stops at start when the game's mod version or devbench actions don't match.
- `pip install -r tests/e2e/requirements.txt`

QASmoke's player has a working Pip-Boy, and frames stand there for the power armor test. The run adds and equips the rest through the console: the allow-listed helmet (`000F6D86`) and the test weapon (the assault rifle), which devbench's `player` tool draws and sheathes.

## Running

```powershell
pytest tests/e2e                          # all
pytest tests/e2e -m gestures              # one area: light, gestures, overrides, headgear, menus, weapon, powerarmor, input, health
pytest tests/e2e -m "not powerarmor"      # KabutoVR enabled (it blocks the scripted exit)
pytest tests/e2e --e2e-restore            # only restore a crashed run's backup
```

The devbench port comes from its `runtime.json`, or `--devbench-port`. `--power-armor-radius` sets how far to look for a frame (default 2000 units: the nearest QASmoke frame is about 1350 from where the run starts).

## State

- Each test starts the same (the `flashlight` fixture): no menu, no config UI, out of power armor, no weapon drawn, the helmet on, and the light off on the head.
- Settings a result depends on are pinned as session overrides (`BASELINE` in `conftest.py`): restriction fallback on, the Immersive headgear rule, no weapon-lamp requirement, the body stow shown. A test overrides one again to try another value.
- A location switch saves to the config file, so the run backs up `My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR\` through devbench into `tests/e2e/.backup/manifest.json` and puts it back after. A crashed run leaves the manifest, and the next run (or `--e2e-restore`) restores it first; the game must be running.

## Report

`build/e2e/<date>-<time>/index.html`: totals, failures, per-area results and times, and the game dialogs the run closed.

## Tests

| File | Covers |
| --- | --- |
| `test_light.py` | The light action: each location on with its beam radius, on / off / toggle. |
| `test_gestures.py` | Every gesture transition with no weapon, fired through its gating (the `gesture` action, without zone and button), and its event. |
| `test_overrides.py` | A session override on the live beam, and clearing it. |
| `test_headgear.py` | Helmet off and on, with and without fallback; the deny list under Immersive and AnyHeadGear; no requirement. |
| `test_menus.py` | The pause menu blocking the gestures and zones; the config UI. |
| `test_weapon.py` | The weapon's gesture transitions; the lamp requirement refusing the weapon and moving the light off it. |
| `test_power_armor.py` | In and out of a frame: the PA head and its beam, the head gestures in PA, the location restored. |
| `test_input.py` | The gestures for real: devbench's `input` reaches a controller into the head, primary-hand and body-grab zones and presses its trigger (tap, long press), checking the zone suppresses the button and nothing fires outside the zones. About 20-40 s per test, as every reach calibrates. |
| `test_health.py` | The stowed model in the scene, `Flashlight::onFrameUpdate` p95 with the light in hand, no error in the log over the run. Runs last. |

Helpers: `devbench.py` (client, kept close to Mods Config's), `flashlight.py` (the mod's actions, the loadout), `vrinput.py` (devbench's reach and press), `backup.py`, `report.py`.

## Manual checklist

What the tests don't reach, checked in the headset before a release:

- [ ] Gestures by hand: the zones' look, icons and haptics (`test_input.py` covers the zones and buttons)
- [ ] Two-handed weapon toggle with a FRIK / ROCK two-handed grip; ROCK weapon in the offhand
- [ ] Pip-Boy open: the light stays on
- [ ] Power armor: step in and out with the animation (the light is restored)
- [ ] A weapon with a modeled lamp: the beam leaves the lamp, the glow cone lines up
- [ ] Beam look per location, gobos, colors; the beam tuning screen from Mods Config (Save / Discard)
- [ ] A hostile NPC notices the beam (state `npc.lastEvent`)
