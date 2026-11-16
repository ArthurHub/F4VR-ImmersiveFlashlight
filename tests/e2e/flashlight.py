"""The mod through its devbench tool: the light, the gestures, the config's session overrides, its state, and the
player's loadout the tests need (a helmet, a weapon; QASmoke's player has the Pip-Boy)."""

from devbench import ModTool, close_menus, wait_for
from devbench.games import fallout4

# Fallout4.esm forms
# in the shipped Immersive headgear allow list, so the head is allowed whatever the keywords say
HELMET = "000F6D86"
# in the shipped Immersive headgear deny list: a soft hat, never light-capable
DENIED_HAT = "000316D4"
# a ranged weapon without a modeled lamp: the assault rifle
WEAPON = "0000463F"

# the mod's devbench tool, and the prefix of its events
MOD_TOOL = "immersiveflashlightvr"

# FRIK's devbench tool, whose probe sets a two-handed grip
FRIK_TOOL = "frik"

# the light action's location argument for each configured location
LOCATION_ARG = {"OnHead": "head", "InOffhand": "offhand", "InPrimaryHand": "primaryHand"}

# the node the light hangs from while it is in a hand
LIGHT_NODE = "ImmersiveFlashlight"


def physical_hand(hand, left_handed):
    """The controller (left|right) of a logical hand (offhand|primaryHand) for the player's handedness."""
    return "left" if (hand == "offhand") != left_handed else "right"


def summary(state):
    """The light's state in a few words, for a failure message."""
    light = state.get("light", {})
    return f"on={light.get('on')} location={light.get('location')} beam.radius={light.get('beam', {}).get('radius')}"


class Flashlight:
    """The mod's devbench actions, and waiting on its state."""

    def __init__(self, bench):
        self.bench = bench
        self.mod = ModTool(bench, MOD_TOOL)

    def state(self):
        return self.mod.call("state")

    def wait(self, condition, timeout=5.0, what="the state"):
        """Poll the state until condition(state) holds and return it; fail with the light's last state after timeout
        seconds."""
        return wait_for(self.state, condition, timeout, what=what, describe=summary)

    def light(self, location=None, op=None):
        """Switch the configured location (head|offhand|primaryHand) and/or turn the light on|off|toggle."""
        args = {key: value for key, value in (("location", location), ("op", op)) if value}
        return self.mod.call("light", **args)

    def gesture(self, gesture, press="tap", hand="offhand"):
        """Fire a gesture through its real gating, without its zone and button: answers fired, inert, lightOn, location."""
        return self.mod.call("gesture", gesture=gesture, press=press, hand=hand)

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
        return self.mod.call("config", **args)["value"]

    def config_int(self, key, section=None):
        return int(float(self.config(key, section)))

    def config_bool(self, key, section=None):
        return str(self.config(key, section)).lower() in ("true", "1")

    def set(self, key, value, section=None):
        """Override a config value for the session (not written to the file)."""
        args = {"key": key, "value": str(value), **({"section": section} if section else {})}
        self.mod.call("set", **args)

    def clear(self, key=None):
        """Drop one session override, or all of them."""
        self.mod.call("clear", **({"key": key} if key else {"all": True}))

    def overrides(self):
        return self.mod.call("overrides")["overrides"]

    def restrictions(self):
        return self.mod.call("restrictions")

    def zones(self):
        return self.mod.call("zones")

    def ui(self, op):
        return self.mod.call("ui", op=op)

    def perf(self, reset=False):
        return self.mod.call("perf", format="flat", reset=reset)

    def health(self):
        return self.mod.call("health")


class Loadout:
    """What the player wears and holds, through devbench's Fallout 4 helpers, each change waited on until the mod sees
    it. Nothing reaches a save: the run is in QASmoke."""

    def __init__(self, bench, flashlight):
        self.bench = bench
        self.flashlight = flashlight

    def wear_helmet(self, form=HELMET):
        fallout4.equip(self.bench, form)
        return self.flashlight.wait(lambda s: self.worn_headgear() == form, 5, f"the headgear {form} worn")

    def worn_headgear(self):
        """The form id of the worn headgear, or None."""
        worn = self.flashlight.restrictions()["headgear"]["worn"]
        return worn.rsplit("[", 1)[-1].rstrip("]") if worn else None

    def remove_headgear(self):
        worn = self.worn_headgear()
        if worn:
            fallout4.unequip(self.bench, worn)
            self.flashlight.wait(lambda s: self.worn_headgear() is None, 5, "the headgear off")

    def draw_weapon(self, form=WEAPON):
        """Equip the weapon and draw it."""
        fallout4.equip(self.bench, form)
        fallout4.draw_weapon(self.bench)
        return self.flashlight.wait(lambda s: s["restrictions"]["weaponEquipped"], 5, "the weapon drawn")

    def holster_weapon(self):
        """Sheathe what is drawn, fists included (the mod counts drawn fists as a weapon). The weapon stays equipped."""
        fallout4.sheathe_weapon(self.bench)
        self.flashlight.wait(lambda s: not s["restrictions"]["weaponEquipped"], 5, "the weapon holstered")

    def leave_power_armor(self):
        """Get out of power armor (the exit gesture when a mod such as KabutoVR refuses the scripted exit), and wait
        until the mod sees it."""
        fallout4.leave_power_armor(self.bench)
        self.flashlight.wait(lambda s: not s["gating"]["inPowerArmor"], 5, "the mod out of power armor")


def close_game_menus(bench):
    """Close the menus a test may have left open."""
    close_menus(bench, ["PauseMenu"])
