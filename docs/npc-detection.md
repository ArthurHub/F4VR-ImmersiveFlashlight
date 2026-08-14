# NPC Light Detection

Your flashlight can now give you away. Point the beam at a raider and they notice — and if you catch them
full in the face at close range, they come straight for you.

For how the light itself works see the [Usage and Configuration Guide](README.md); for install and
requirements see the [main README](../README.md). The engineering notes behind this feature live in
[docs/tech/npc-light-detection.md](tech/npc-light-detection.md).

## Contents

- [Why this exists](#why-this-exists)
- [How it works](#how-it-works)
- [Compared to Flashlight Stealth Fix](#compared-to-flashlight-stealth-fix)
- [Configuration](#configuration)

## Why this exists

Vanilla Fallout 4 asks exactly one question about how visible you are: **"how lit is the player?"** A
ceiling lamp above you raises that. A flashlight does not — the beam projects *away* from you, so the game
considers you unlit no matter where you aim it.

The result is a long-standing stealth hole: you can stand in the dark, put a beam squarely on a raider's
chest from ten metres, and they will neither see the light nor react to it. Nothing in the engine ever asks
whether an NPC is standing *in* somebody's beam.

This feature closes that hole, and does it **directionally** — where you point the light is what matters.

## How it works

Two separate things happen while the light is on. They stack, and either can be turned off on its own.

### 1. You become visible

Holding a light source makes you easier to see, so the game is told you are lit:

- **Just having the light on** applies a constant amount (`fNpcDetectionLightLevelBaseline`), whether or not
  the beam is on anybody, and whether or not you're sneaking. Carrying a light gives you away by itself.
- **Putting the beam on someone** raises it much further, scaling with how hard the beam actually lands on
  them — near `fNpcDetectionLightLevelMin` for a weak or distant hit, up to `fNpcDetectionLightLevelMax`
  point-blank. Your beam settings feed into this, so a bright long-throw hand torch exposes you more than a
  dim head lamp at the same distance.
- When the beam moves off, that extra visibility **fades out** over `fNpcDetectionLightLevelDecayMs` rather
  than vanishing instantly, so sweeping the light past someone doesn't flicker.

This is *your* illumination, so while it's raised **every** NPC who can see you benefits from it, not only
the one you're pointing at. That's why the ceiling is tuned to roughly "standing under a street light"
rather than something blinding.

It also never makes you *harder* to see than you really are: if you're already standing in a bright room,
the beam adds nothing.

### 2. NPCs investigate what your beam touches

Several times a second, the mod looks at what the beam is actually hitting and reports it as something
worth investigating:

- **Beam on an NPC** — the nearest one standing in the beam with a clear line of sight is alerted, and the
  alert is placed *between them and you*, so they turn and move toward the light source instead of
  wandering around their own feet.
- **Beam in their face** — if the beam lands hard enough on a hostile who is facing you (bright, and close),
  they don't investigate, they've **seen you**. The alert goes on your actual position and they come
  straight at you.
- **Beam on a wall or the floor** — if the beam touches nobody, the bright patch it paints can still be
  noticed, but only if somebody is near it, facing it, and can genuinely see it. A light patch on the floor
  of an empty corridor does nothing.

A few rules keep this honest:

- **Line of sight is checked.** An NPC behind cover isn't alerted by a beam that can't reach them.
- **The detection cone is smaller than the beam you see** — narrower (`fNpcDetectionFovMult`) because the
  dim outer edge shouldn't alert anyone, and shorter (`fNpcDetectionMaxRange`) because a beam that reaches
  90 m shouldn't alert at 90 m.
- **Only hostiles react** by default, and **companions never do** — they already know where you are.

## Compared to Flashlight Stealth Fix

[Flashlight Stealth Fix](https://www.nexusmods.com/fallout4/mods/76586) is the established solution to the
same vanilla hole, and it's a good mod. It solves the problem differently, and the trade-offs are worth
knowing.

| | Flashlight Stealth Fix | This mod |
| --- | --- | --- |
| Approach | Papyrus + ESP + perk | Native code, no ESP |
| Requires | BakaFramework, MCM, an ESP slot | nothing extra |
| Aim matters? | **No** — you're equally exposed whichever way you point | **Yes** — only what the beam touches counts |
| Through walls? | Yes — its alert ignores line of sight | Direct alerts are line-of-sight checked |
| Alert strength | Flat | Scales with beam brightness and distance |
| Alert position | Always your own position | Where the beam lands, so they investigate the light |
| How often | Every 3 seconds | Several times a second |
| Works with | Any flashlight, and flat Fallout 4 | This mod's flashlight, VR only |

The practical difference is aim. Flashlight Stealth Fix applies a large fixed penalty the whole time your
light is on and pings your position every three seconds regardless of direction, which is what produces the
familiar "enemies turned around even though my beam was on the floor behind me". Here, pointing the beam
away from someone costs you only the baseline; pointing it at them is what gets you caught.

Where Flashlight Stealth Fix wins is reach: it works with any flashlight, including the vanilla Pip-Boy
light and other mods' lights, and it works in flat Fallout 4. This feature only knows about its own beam,
because knowing exactly where that beam is pointing every frame is what makes it directional in the first
place.

> **Don't run both.** They solve the same problem and stack — you'd get this mod's visibility *plus* the
> perk's fixed penalty, and two sets of alerts. Pick one: either uninstall Flashlight Stealth Fix, or set
> `bNpcDetectionEnabled = false` here.

## Configuration

INI-only for now — there's no in-game screen for these yet. Changes are live-reloaded into the running game
when you save the file, so you can tune while playing. See
[Advanced Configuration](README.md#advanced-configuration) for the INI location.

All keys live in the `[ImFl_NpcDetection]` section.

### Master switches

| Setting | Default | Meaning |
| --- | --- | --- |
| `bNpcDetectionEnabled` | `true` | The whole feature. Off = vanilla behaviour, flashlights are free. |
| `bNpcDetectionOnlyWhenSneaking` | `false` | Only alert NPCs while sneaking. Your visibility still applies either way. |
| `bNpcDetectionOnlyHostileNpcs` | `true` | Only enemies react. Off = settlers and guards notice your beam too. |

### How visible the light makes you

| Setting | Default | Meaning |
| --- | --- | --- |
| `bNpcDetectionLightLevelEnabled` | `true` | Whether the beam makes you genuinely *visible* (as opposed to only alerting NPCs). |
| `fNpcDetectionLightLevelBaseline` | `50` | Applied just for having the light on. Keep it below Min. |
| `fNpcDetectionLightLevelMin` | `80` | Applied on a weak or distant hit. |
| `fNpcDetectionLightLevelMax` | `250` | Applied point-blank. |
| `fNpcDetectionLightLevelCurve` | `1.0` | `1` ramps evenly with distance; higher keeps you dim until the beam is close. |
| `fNpcDetectionLightLevelDecayMs` | `1500` | How long that fades out over once the beam moves off. |

For scale: an ordinary dim interior reads about 20–40, standing under a street light about 150, and bright
indoor lighting can reach 400.

### What NPCs notice

| Setting | Default | Meaning |
| --- | --- | --- |
| `bNpcDetectionDirectEnabled` | `true` | Alert an NPC standing in your beam. |
| `iNpcDetectionDirectSoundLevel` | `100` | How alarming that is (0–500). `0` disables it. |
| `bNpcDetectionLitSpotEnabled` | `true` | Alert on the bright patch your beam paints when it hits nobody. |
| `iNpcDetectionLitSpotSoundLevel` | `40` | How alarming that patch is. `0` disables it. |
| `iNpcDetectionSpottedEventLevel` | `100` | How hard the beam must land before they've *seen* you rather than investigating. Raise above 150 to never be spotted this way. |

### Reach and timing

| Setting | Default | Meaning |
| --- | --- | --- |
| `fNpcDetectionMaxRange` | `4000` | Furthest the beam can alert anyone, in game units (~100 per 1.4 m). |
| `fNpcDetectionFovMult` | `0.5` | Fraction of the visible beam cone that counts. `1.0` alerts from the dim outer edge. |
| `iNpcDetectionIntervalMs` | `1000` | How often the beam is checked. Lower = snappier reactions. |
| `sNpcDetectionLosCollisionFilter` | `02420028` | Advanced: which collision layers block the beam's line of sight. Leave alone unless debugging. |

### Common adjustments

| Goal | Change |
| --- | --- |
| Turn the whole thing off | `bNpcDetectionEnabled = false` |
| Keep alerts, drop the visibility | `bNpcDetectionLightLevelEnabled = false` |
| Keep visibility, drop the alerts | `bNpcDetectionDirectEnabled` and `bNpcDetectionLitSpotEnabled` = `false` |
| Only matters while sneaking | `bNpcDetectionOnlyWhenSneaking = true` |
| Never get instantly spotted | `iNpcDetectionSpottedEventLevel` above `150` |
| Carrying a light should cost more | `fNpcDetectionLightLevelBaseline` up |
| Caught too easily at range | `fNpcDetectionLightLevelCurve` up, or `fNpcDetectionMaxRange` down |
| Stop light patches giving you away | `iNpcDetectionLitSpotSoundLevel = 0` |
