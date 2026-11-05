# Headgear Requirement

Out of power armor, the head light only goes on while you wear headgear it could clip to. By default that means a helmet or hard hat, not a baseball cap. In power armor the helmet lamp always works.

How the light works: [Usage and Configuration Guide](README.md). Settings and the config file: [Settings](README.md#settings).

## Contents

- [Modes](#modes)
- [When you're not wearing eligible headgear](#when-youre-not-wearing-eligible-headgear)
- [Choosing what counts](#choosing-what-counts)
- [Checking what qualifies](#checking-what-qualifies)
- [Settings](#settings)

## Modes

**Require headgear for flashlight on head**, on the General page (`iFlashlightHeadgearRequirement`):

| Option | Meaning |
| --- | --- |
| **Not required** (`0`) | The head is always available. |
| **Any headgear** (`1`) | Anything in the head slot: a cap, a bandana, a helmet. |
| **Helmets only** (`2`, default) | Light-capable headgear: helmets and hard hats, not soft hats. |

The head slot is biped slot 30, and only armor counts. The requirement is checked every frame, so a change applies at once, and so does putting a helmet on or taking it off.

The head light is the most convenient mount: hands-free and pointing where you look. The requirement gives it a cost, as it makes sense when the light is clipped to your helmet. Set it to **Not required** if you'd rather not have it.

## When you're not wearing eligible headgear

- **The head gesture won't put the light on your head.** No haptic, and the trigger reaches the game. A light already on your head can still be turned off.
- **A head light moves to your hand** once the requirement stops being met. Take the helmet off and the light goes to your offhand, or onto the weapon if both hands are on it. Put an eligible helmet back on and it returns to your head. With **Move the light when its place isn't allowed** (General page) off, the light turns off instead, and turning it on bare-headed shows a message saying why.

When a gun without a lamp would carry the light, it falls back to your head (see [Weapon Flashlight Requirement](weapon-mount-restriction.md#when-the-weapon-has-no-lamp)), and bare-headed on to your offhand. With both hands on that gun there's no place left, so the light turns off, and turning it on there shows a message saying why.

## Choosing what counts

**Helmets only** decides in this order:

1. On the **deny list**: it never counts.
2. On the **allow list**: it counts.
3. Otherwise it counts if it carries **one of the keywords**.

The three lists are in the config file's `[ImFl_HeadgearRestriction]` section, and reload as you save:

- **`aHeadLightKeywords`:** keyword editor IDs, as xEdit shows them (e.g. `ArmorTypePower`). One keyword can qualify a whole mod's helmets.
- **`aHeadLightAllowList`:** headgear the keywords miss, as `"localFormID|plugin"`. The FormID is hex, without the load-order prefix:

  ```toml
  aHeadLightAllowList = ["0F6D86|Fallout4.esm", "0026D8|SS2Extended.esp"]
  ```

- **`aHeadLightDenyList`:** headgear that carries a keyword but shouldn't count. It has the same form, and it wins over everything.

An entry for a plugin you don't have is skipped, so a list can be shared between setups. The defaults cover the base game and its DLC. Add to them rather than replacing them.

## Checking what qualifies

Set this in the config file's `[Debug]` section and save:

```toml
aDumpDataOnce = ["headgear"]
```

The log (`%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ImmersiveFlashlightVR.log`) then lists every head-slot armor in your load order, as allowed or blocked. It uses the Helmets only rule whatever mode is selected, so you can tune the lists first. It runs once, and the entry is removed.

## Settings

| Setting | Default | Meaning |
| --- | --- | --- |
| `iFlashlightHeadgearRequirement` | `2` | `0` not required, `1` any headgear, `2` helmets only. |
| `aHeadLightKeywords` | base-game armor keywords | Keyword editor IDs that make headgear count. |
| `aHeadLightAllowList` | base-game and DLC helmets | Always counts, as `localFormID\|plugin`. |
| `aHeadLightDenyList` | one base-game soft hat | Never counts. Wins over the other two. |

To qualify a whole mod's helmets, add its helmet keyword to `aHeadLightKeywords`. To allow only a hand-picked set, empty `aHeadLightKeywords` and list the helmets in `aHeadLightAllowList`.
