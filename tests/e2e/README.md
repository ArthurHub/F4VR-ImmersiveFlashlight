# End-to-end tests

pytest tests that drive the running game through devbench's REST port and check the light, the gestures, the restrictions and the mod's health. Run by hand before a release; CI has no game. The layout follows Mods Config's `tests/e2e`.

## Requirements

- The game running with [devbench](https://github.com/ArthurHub/devbench) (a build with its `files` and `player` tools, and `input` with its `pose` action and its head presets), at the main menu or already in QASmoke. The tests `coc qasmoke` so nothing reaches a save; any other loaded game is refused.
- This checkout's build deployed: a run stops at start when the game's mod version or devbench actions don't match.
- FRIK 0.79 or later, whose devbench `probe` sets the two-handed grip: a run stops at start without it.
- `pip install -r tests/e2e/requirements.txt`

QASmoke's player has a working Pip-Boy, and frames stand there for the power armor test. The run adds and equips the rest through the console: the allow-listed helmet (`000F6D86`) and the test weapon (the assault rifle), which devbench's `player` tool draws and sheathes.

## Running

```powershell
pytest tests/e2e                          # all
pytest tests/e2e -m gestures              # one area: light, gestures, overrides, headgear, menus, weapon, controllers, powerarmor, health
pytest tests/e2e --left-handed            # all, left-handed
pytest tests/e2e --e2e-restore            # only restore a crashed run's backup
```

The devbench port comes from its `runtime.json`, or `--devbench-port`. `--power-armor-radius` sets how far to look for a frame (default 2000 units: the nearest QASmoke frame is about 1350 from where the run starts).

## State

- The run is right-handed, or left-handed with `--left-handed`: it sets the game's `bLeftHandedMode:VR` through the console at start (the game, FRIK and the mod follow it live) and puts it back after. The tests name the hands offhand and primary hand, and drive the controller of each by the handedness. Run both before a release.
- Each test starts the same (the `flashlight` fixture): no menu, no config UI, out of power armor, no weapon drawn, the helmet on, and the light off on the head.
- No two-handed grip: a test that needs one sets it through FRIK's devbench `probe` (`op: grip`, its `setOffHandGripping` API, as a weapon mod reports one), cleared after it.
- Settings a result depends on are pinned as session overrides (`BASELINE` in `conftest.py`): restriction fallback on, the Immersive headgear rule, no weapon-lamp requirement, the body stow shown. A test overrides one again to try another value.
- A location switch saves to the config file, so the run backs up `My Games\Fallout4VR\Mods_Config\ImmersiveFlashlightVR\` through devbench into `tests/e2e/.backup/manifest.json` and puts it back after. A crashed run leaves the manifest, and the next run (or `--e2e-restore`) restores it first; the game must be running.

## Report

`build/e2e/<date>-<time>/index.html`: totals, failures, per-area results and times, and the game dialogs the run closed.

## Tests

| File | Covers |
| --- | --- |
| `test_light.py` | The light action: each location on with its beam radius, on / off / toggle; the light in each hand hanging under that physical hand's bone, at its wand. |
| `test_gestures.py` | Every gesture transition with no weapon, fired through its gating (the `gesture` action, without zone and button), and its event. |
| `test_overrides.py` | A session override on the live beam, and clearing it. |
| `test_headgear.py` | Helmet off and on, with and without fallback; the deny list under Immersive and AnyHeadGear; no requirement. |
| `test_menus.py` | The pause menu blocking the gestures and zones; the config UI. |
| `test_weapon.py` | The weapon's gesture transitions; the lamp requirement refusing the weapon and moving the light off it; the two-handed toggle in a grip set through FRIK (no hand on the weapon), and the primary-hand gesture standing down while gripped. |
| `test_controllers.py` | The gestures for real: devbench's input `pose` holds the head and hands in its ready pose (in view on the desktop mirror), places a hand into the head, primary-hand and body-grab zones and presses its trigger (tap, long press), checking the zone suppresses the button and nothing fires outside the zones; the offhand's trigger toggling the weapon light anywhere in a two-handed grip set through FRIK. A placement takes 0.5-1.5 s without calibrating; the head looks down at the belt (devbench's `down` preset) for the body grab, and the head gesture is placed as recorded in the headset (the zone's offhand side, the controller tilted back) so the hand hangs beside the head, not in the face. The report's "Controller placements" table times each one. |
| `test_power_armor.py` | In and out of a frame through devbench's `player`: at once, with the PA head and its beam, the head gestures in PA and the location restored; and stepping in and out through the controllers, the light restored at each side. A refused scripted exit (KabutoVR) skips the rest of its test, and the exit gesture gets the player out. |
| `test_health.py` | The stowed model in the scene, `Flashlight::onFrameUpdate` p95 with the light in hand, no error in the log over the run. Runs last. |

Helpers: `devbench.py` (client, kept close to Mods Config's), `flashlight.py` (the mod's actions, the loadout through the console and devbench's `player`), `controllers.py` (devbench's pose and press, zone aiming, and a log of each placement), `backup.py`, `report.py`.

## Manual checklist

What the tests don't reach, checked in the headset before a release:

- [ ] Gestures by hand: the zones' look, icons and haptics (`test_controllers.py` covers the zones and buttons)
- [ ] The two-handed toggle with the offhand on the weapon for real (the tests set FRIK's grip), and with ROCK's grip; ROCK weapon in the offhand
- [ ] Pip-Boy open: the light stays on
- [ ] Power armor: the light on the PA head looks right through the step-in animation
- [ ] A weapon with a modeled lamp: the beam leaves the lamp, the glow cone lines up
- [ ] Beam look per location, gobos, colors; the beam tuning screen from Mods Config (Save / Discard)
- [ ] A hostile NPC notices the beam (state `npc.lastEvent`)
