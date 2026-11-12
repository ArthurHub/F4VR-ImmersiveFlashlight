"""The mod through its devbench tool: the light, the gestures, the config's session overrides, its state, and the
player's loadout the tests need (a helmet, a weapon; QASmoke's player has the Pip-Boy)."""

import time

from devbench import DevbenchError

# Fallout4.esm forms
# in the shipped Immersive headgear allow list, so the head is allowed whatever the keywords say
HELMET = "000F6D86"
# in the shipped Immersive headgear deny list: a soft hat, never light-capable
DENIED_HAT = "000316D4"
# a ranged weapon without a modeled lamp: the assault rifle
WEAPON = "0000463F"

# FRIK's devbench tool, whose probe sets a two-handed grip
FRIK_TOOL = "frik"

# the light action's location argument for each configured location
LOCATION_ARG = {"OnHead": "head", "InOffhand": "offhand", "InPrimaryHand": "primaryHand"}

# the game's handedness setting: the offhand is the right controller while it is 1
LEFT_HANDED_INI = "bLeftHandedMode:VR"

# the node the light hangs from while it is in a hand
LIGHT_NODE = "ImmersiveFlashlight"


def physical_hand(hand, left_handed):
    """The controller (left|right) of a logical hand (offhand|primaryHand) for the player's handedness."""
    return "left" if (hand == "offhand") != left_handed else "right"


def set_left_handed(bench, left_handed):
    """Set the game's handedness, and wait until the mod reads it; answers whether it was left-handed before."""
    was = bench.console_number(f'getini "{LEFT_HANDED_INI}"') == 1
    if was != left_handed:
        bench.console(f'setini "{LEFT_HANDED_INI}" {int(left_handed)}')
    flashlight = Flashlight(bench)
    flashlight.wait(lambda s: flashlight.left_handed() == left_handed, 5, f"the mod reading {LEFT_HANDED_INI} {int(left_handed)}")
    return was


def summary(state):
    """The light's state in a few words, for a failure message."""
    light = state.get("light", {})
    return f"on={light.get('on')} location={light.get('location')} beam.radius={light.get('beam', {}).get('radius')}"


class Flashlight:
    """The mod's devbench actions, and waiting on its state."""

    def __init__(self, bench):
        self.bench = bench

    def state(self):
        return self.bench.mod("state")

    def wait(self, condition, timeout=5.0, what="the state"):
        """Poll the state until condition(state) holds and return it; fail with the last state after timeout seconds."""
        deadline = time.monotonic() + timeout
        while True:
            state = self.state()
            if condition(state):
                return state
            if time.monotonic() > deadline:
                raise AssertionError(f"{what} did not hold within {timeout} s: {summary(state)}")
            time.sleep(0.1)

    def light(self, location=None, op=None):
        """Switch the configured location (head|offhand|primaryHand) and/or turn the light on|off|toggle."""
        args = {key: value for key, value in (("location", location), ("op", op)) if value}
        return self.bench.mod("light", **args)

    def gesture(self, gesture, press="tap", hand="offhand"):
        """Fire a gesture through its real gating, without its zone and button: answers fired, inert, lightOn, location."""
        return self.bench.mod("gesture", gesture=gesture, press=press, hand=hand)

    def two_handed_grip(self, on=True):
        """Set or drop a two-handed grip through FRIK's devbench probe (its setOffHandGripping API, as a weapon mod
        reports one), the offhand supporting the weapon; FRIK drops it on a weapon change too. FRIK answers whether it
        reports the grip now."""
        return self.bench.tool(FRIK_TOOL, action="probe", op="grip", hand=self.physical("offhand"), on=on)["gripping"]

    def left_handed(self):
        """Whether the mod reads the player as left-handed (the offhand is the right controller)."""
        return self.zones().get("leftHanded", False)

    def physical(self, hand):
        """The controller (left|right) of a logical hand, from the mod's handedness."""
        return physical_hand(hand, self.left_handed())

    def light_node(self):
        """The node the light hangs from in a hand, as devbench's nodes tool finds it (path, world), or None."""
        matches = self.bench.tool("nodes", action="find", name=LIGHT_NODE, root="scene", limit=1)["matches"]
        return matches[0] if matches else None

    def config(self, key, section=None):
        """A config value as the mod has it, override included, as its text."""
        args = {"key": key, **({"section": section} if section else {})}
        return self.bench.mod("config", **args)["value"]

    def config_int(self, key, section=None):
        return int(float(self.config(key, section)))

    def config_bool(self, key, section=None):
        return str(self.config(key, section)).lower() in ("true", "1")

    def set(self, key, value, section=None):
        """Override a config value for the session (not written to the file)."""
        args = {"key": key, "value": str(value), **({"section": section} if section else {})}
        self.bench.mod("set", **args)

    def clear(self, key=None):
        """Drop one session override, or all of them."""
        self.bench.mod("clear", **({"key": key} if key else {"all": True}))

    def overrides(self):
        return self.bench.mod("overrides")["overrides"]

    def restrictions(self):
        return self.bench.mod("restrictions")

    def zones(self):
        return self.bench.mod("zones")

    def ui(self, op):
        return self.bench.mod("ui", op=op)

    def perf(self, reset=False):
        return self.bench.mod("perf", format="flat", reset=reset)

    def health(self):
        return self.bench.mod("health")


class Loadout:
    """What the player wears and holds, set through the console. Nothing reaches a save: the run is in QASmoke."""

    def __init__(self, bench, flashlight):
        self.bench = bench
        self.flashlight = flashlight

    def item_count(self, form):
        return int(self.bench.console_number(f"player.getitemcount {form}"))

    def equip(self, form):
        """Equip an item, adding one first when the player has none."""
        if self.item_count(form) == 0:
            self.bench.console(f"player.additem {form} 1")
        self.bench.console(f"player.equipitem {form}")

    def unequip(self, form):
        self.bench.console(f"player.unequipitem {form}")

    def wear_helmet(self, form=HELMET):
        self.equip(form)
        return self.flashlight.wait(lambda s: self.worn_headgear() == form, 5, f"the headgear {form} worn")

    def worn_headgear(self):
        """The form id of the worn headgear, or None."""
        worn = self.flashlight.restrictions()["headgear"]["worn"]
        return worn.rsplit("[", 1)[-1].rstrip("]") if worn else None

    def remove_headgear(self):
        worn = self.worn_headgear()
        if worn:
            self.unequip(worn)
            self.flashlight.wait(lambda s: self.worn_headgear() is None, 5, "the headgear off")

    def _switch_weapon(self, action):
        """Draw or sheathe through devbench's player tool (the engine's own call). It repeats a call the engine
        ignored and waits for the weapon state, so refused means the switch did not happen."""
        answer = self.bench.tool("player", action=action, timeoutMs=5000)
        assert answer["outcome"] in ("switched", "already"), f"{action}: {answer['outcome']}, the weapon is {answer['weaponState']} ({answer.get('note')})"
        return answer

    def draw_weapon(self, form=WEAPON):
        """Equip the weapon and draw it."""
        self.equip(form)
        self._switch_weapon("drawWeapon")
        return self.flashlight.wait(lambda s: s["restrictions"]["weaponEquipped"], 5, "the weapon drawn")

    def holster_weapon(self):
        """Sheathe what is drawn, fists included (the mod counts drawn fists as a weapon). The weapon stays equipped."""
        self._switch_weapon("sheatheWeapon")
        self.flashlight.wait(lambda s: not s["restrictions"]["weaponEquipped"], 5, "the weapon holstered")

    def power_armor_frame(self, radius):
        """The nearest power armor frame in the player's cell within radius game units, as devbench's player tool
        lists it ({ formId, baseFormId, name, distance }), or None."""
        frames = self.bench.tool("player", action="findPowerArmor", radius=radius, limit=1)["frames"]
        return frames[0] if frames else None

    def power_armor(self, action, **args):
        """A power armor switch through devbench's player tool: enterPowerArmor / exitPowerArmor at once (setup), or
        stepIn / stepOut through the controllers. It waits for the engine state and answers its outcome (switched,
        already, refused, notAimed)."""
        return self.bench.tool("player", action=action, **args)

    def leave_power_armor(self):
        """Get out of power armor, with the exit gesture when the scripted exit is refused (a mod such as KabutoVR
        can block it), and wait until the mod sees it."""
        answer = self.power_armor("exitPowerArmor")
        if answer["outcome"] == "refused":
            answer = self.power_armor("stepOut")
        assert answer["outcome"] in ("switched", "already"), f"out of power armor: {answer['outcome']} ({answer.get('note')})"
        self.flashlight.wait(lambda s: not s["gating"]["inPowerArmor"], 5, "the mod out of power armor")


def close_game_menus(bench):
    """Close the menus a test may have left open."""
    for name in ("PauseMenu",):
        try:
            bench.tool("menu", action="close", name=name)
        except DevbenchError:
            pass
