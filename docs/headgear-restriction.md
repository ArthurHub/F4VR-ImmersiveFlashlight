# Headgear Requirement

A head lamp has to clip to something. Out of power armor, the light only goes on your head while you're
wearing headgear it could plausibly be mounted on — and it ships requiring a proper helmet or hard hat, not
a baseball cap.

For how the light itself works see the [Usage and Configuration Guide](README.md); for install and
requirements see the [main README](../README.md).

## Contents

- [Why this exists](#why-this-exists)
- [How it works](#how-it-works)
- [When you're not wearing eligible headgear](#when-youre-not-wearing-eligible-headgear)
- [Choosing what counts](#choosing-what-counts)
- [Checking what qualifies in your load order](#checking-what-qualifies-in-your-load-order)
- [Configuration](#configuration)
- [Common adjustments](#common-adjustments)

## Why this exists

The head-mounted light is the most convenient mount in the mod: hands-free, always pointing where you look,
and free to use while both hands are on a weapon. Without a gate, it's also the mount with no cost — you can
be bare-headed in a vault suit and still have a cap lamp.

The requirement puts a price on it. Wear a helmet and you get the hands-free light; take it off and the
light goes back to your hands. It's a self-imposed rule, so it has an off switch, but it's on by default
because "the light is attached to your helmet" is the reading that makes the head mount make sense.

Power armor is exempt entirely. The helmet lamp is part of the suit, so in power armor the head light always
works no matter how this is set.

## How it works

`iFlashlightHeadgearRequirement` has three settings, changed on the misc config screen or in the INI:

| Mode | Meaning |
| --- | --- |
| `0` **None** | No restriction. The head is always available. |
| `1` **Any headgear** | Anything worn in the head slot is enough — a cap, a bandana, a helmet. |
| `2` **Immersive** *(default)* | Only "light-capable" headgear: helmets and hard hats, not soft hats. |

What you're wearing is read from the head armor slot — biped slot 30, where every hat and helmet in the game
sits. Only armor counts, so a hat-shaped miscellaneous item won't satisfy it.

The **Immersive** rule decides light-capable in this order:

1. If the item is on the **deny list**, it never counts. (This is how a soft hat that happens to carry a
   helmet keyword is excluded.)
2. If the item is on the **allow list**, it counts.
3. Otherwise it counts if it carries **any of the configured keywords**.

Deny beats allow, allow beats keywords. All three lists ship filled in and are yours to edit — see
[Choosing what counts](#choosing-what-counts).

The requirement is checked live, every frame, so changing it (in-game or in the INI) takes effect
immediately, and so does putting a helmet on or taking one off.

## When you're not wearing eligible headgear

Two things happen, both aimed at "the light can't be there" rather than "you did something wrong":

- **The put-it-on-your-head gesture goes inert.** Bringing your offhand to your head does nothing — no
  haptic, and the button passes straight through to the game as a normal trigger press. Turning an
  *already* head-mounted light off with the same gesture still works, so you can never get stuck.
- **A head-mounted light turns off** as soon as the requirement stops being met. Take your helmet off while
  the cap lamp is on and the light goes out; it does not silently relocate to a hand.

Everything else is unaffected — grabbing the light into a hand, mounting it on a weapon, and the beam
settings all behave the same.

One interaction is worth knowing: when your light is in your **offhand** and you take a **two-handed grip**
on a weapon, the offhand is busy, so the light normally moves onto the weapon. If the
[weapon-flashlight requirement](weapon-mount-restriction.md) is on and your gun has no lamp, it falls back
to your **head** instead of being lost — and that fallback is then subject to *this* requirement. Bare-headed
with a lampless gun, gripping two-handed turns the light off. Releasing the grip brings it back to your
offhand.

## Choosing what counts

The Immersive rule is driven by three lists in the `[ImFl_HeadgearRestriction]` INI section. They are
live-reloaded, so you can edit and save the file while the game runs and see the result immediately.

**`sHeadLightKeywords`** — comma-separated keyword **editor IDs** (the names you see in xEdit or the Creation
Kit, e.g. `ArmorTypePower`). Any headgear carrying one of them is light-capable. This is the broad stroke:
one keyword can qualify a whole mod's worth of helmets at once.

**`sHeadLightAllowList`** — headgear the keywords miss. Comma-separated `localFormID|plugin` entries, where
the local FormID is the hex ID as shown in xEdit **without** the load-order prefix:

```
sHeadLightAllowList = 0F6D86|Fallout4.esm, 0026D8|SS2Extended.esp
```

**`sHeadLightDenyList`** — the opposite: headgear that carries a qualifying keyword but where a mounted light
would be silly. Same format, and it wins over everything else.

Both lists are resolved load-order-independently, and **entries for plugins you don't have are simply
skipped** — no errors, no wasted slots. That means a list can be shared or copied wholesale between setups
regardless of which mods each person runs.

The shipped defaults cover the base game and its DLC: a keyword set for the broad armor categories, a
curated allow list of base-game, Far Harbor and Nuka-World helmets the keywords miss, and a deny entry for a
base-game soft hat that sneaks through on keywords. Add your own entries rather than replacing them, unless
you want a deliberately narrower rule.

## Checking what qualifies in your load order

Rather than guessing which of your helmets pass, ask the mod. In the `[Debug]` section of the INI:

```
sDumpDataOnceNames = headgear
```

Save the file and the mod writes every head-slot armor in your load order into the log, split into two
lists — **allowed (light-capable)** and **blocked** — using the resolved keyword/allow/deny lists exactly as
the game is running them. It's one-shot: the token is removed after it runs, so it never fires again on its
own.

The log is at:

```
%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log
```

The dump always applies the **Immersive** rule, whatever mode is currently selected, so you can tune the
lists before switching to it. Copy the FormID of anything on the wrong side into the allow or deny list,
save, and dump again.

## Configuration

The in-game misc screen cycles the mode; the lists are INI-only. All keys live in the
`[ImFl_HeadgearRestriction]` section. See [Advanced Configuration](README.md#advanced-configuration) for the
INI location.

| Setting | Default | Meaning |
| --- | --- | --- |
| `iFlashlightHeadgearRequirement` | `2` (Immersive) | `0` none, `1` any headgear, `2` light-capable headgear only. |
| `sHeadLightKeywords` | base-game head/armor/power keywords | Keyword **editor IDs**; any headgear carrying one is light-capable. |
| `sHeadLightAllowList` | curated base-game + DLC helmets | Force-include, as `localFormID\|plugin` pairs. |
| `sHeadLightDenyList` | one base-game soft hat | Force-exclude. Wins over the allow list and the keywords. |

## Common adjustments

| Goal | Change |
| --- | --- |
| Turn the restriction off | `iFlashlightHeadgearRequirement = 0` |
| Any hat is enough | `iFlashlightHeadgearRequirement = 1` |
| Make one more helmet eligible | Add its `localFormID\|plugin` to `sHeadLightAllowList` |
| Stop a soft hat from qualifying | Add it to `sHeadLightDenyList` |
| Qualify a whole mod's helmets at once | Add that mod's helmet keyword editor ID to `sHeadLightKeywords` |
| See exactly what qualifies right now | `[Debug] sDumpDataOnceNames = headgear`, then read the log |
| Tighten the rule to a hand-picked set | Empty `sHeadLightKeywords` and list only what you want in `sHeadLightAllowList` |
