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

# the light action's location argument for each configured location
LOCATION_ARG = {"OnHead": "head", "InOffhand": "offhand", "InPrimaryHand": "primaryHand"}


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
        """Draw or sheathe through devbench's player tool (the engine's own call), which waits for the weapon state.
        The engine drops a switch made right after the previous one settled (a test shorter than about 0.5 s), and
        devbench makes the call once, so a refused one is made again."""
        for _ in range(4):
            answer = self.bench.tool("player", action=action, timeoutMs=1500)
            if answer["outcome"] in ("switched", "already"):
                return answer
        raise AssertionError(f"{action}: {answer['outcome']}, the weapon is {answer['weaponState']}")

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
