"""The weapon mount, with a ranged weapon without a modeled lamp drawn: the primary-hand gesture moves the light on
and off the weapon, and the lamp requirement keeps it off a weapon without one."""

import pytest

pytestmark = [
    pytest.mark.weapon,
    pytest.mark.usefixtures("weapon"),
]

SECTION = "ImFl_WeaponMount"

# the light action's location that lands on each runtime location with the weapon drawn
START = {"InOffhand": "offhand", "OnWeapon": "primaryHand", "OnHead": "head"}

# (start on, start location, gesture, press) -> (fired, on, location)
CASES = {
    "primaryHand tap: offhand to weapon": ((True, "InOffhand", "primaryHand", "tap"), (True, True, "OnWeapon")),
    "primaryHand long: weapon to offhand": ((True, "OnWeapon", "primaryHand", "long"), (True, True, "InOffhand")),
    "primaryHand tap: off on the weapon": ((True, "OnWeapon", "primaryHand", "tap"), (True, False, "OnWeapon")),
    "primaryHand tap: on at the weapon from off": ((False, "OnWeapon", "primaryHand", "tap"), (True, True, "OnWeapon")),
    "primaryHand tap: head to weapon": ((True, "OnHead", "primaryHand", "tap"), (True, True, "OnWeapon")),
    "head tap: weapon to head": ((True, "OnWeapon", "head", "tap"), (True, True, "OnHead")),
}


@pytest.mark.parametrize("start, expected", CASES.values(), ids=CASES.keys())
def test_weapon_gesture(flashlight, start, expected):
    on, location, gesture, press = start
    landed = flashlight.light(location=START[location], op="on" if on else "off")
    assert landed["location"] == location, "the start state"
    answer = flashlight.gesture(gesture, press=press)
    assert (answer["fired"], answer["lightOn"], answer["location"]) == expected, f"inert: {answer.get('inert')}"


def test_light_action_primary_hand_lands_on_weapon(flashlight):
    answer = flashlight.light(location="primaryHand", op="on")
    assert (answer["lightOn"], answer["location"]) == (True, "OnWeapon")


def test_lamp_required_without_a_lamp(flashlight):
    flashlight.set("iWeaponFlashlightMeshRequired", "1", SECTION)
    flashlight.wait(lambda s: s["restrictions"]["weaponEquipped"] and not s["restrictions"]["weaponFlashlightAllowed"], 5, "the weapon refused")
    assert flashlight.restrictions()["weapon"]["lampNode"] is None

    flashlight.light(location="offhand", op="on")
    answer = flashlight.gesture("primaryHand")
    assert (answer["fired"], answer["lightOn"], answer["location"]) == (False, True, "InOffhand"), "the weapon cannot carry it"


@pytest.mark.parametrize("fallback", [True, False], ids=["fallback", "no fallback"])
def test_lamp_required_again_leaves_the_weapon(flashlight, fallback):
    flashlight.set("bRestrictionFallback", str(fallback).lower())
    flashlight.light(location="primaryHand", op="on")
    flashlight.wait(lambda s: s["light"]["on"] and s["light"]["location"] == "OnWeapon", 3, "the light on the weapon")

    flashlight.set("iWeaponFlashlightMeshRequired", "1", SECTION)
    state = flashlight.wait(lambda s: s["light"]["location"] != "OnWeapon" or not s["light"]["on"], 3, "the light leaving the weapon")
    if fallback:
        assert state["light"]["on"], "the light moves to the next free place"
    else:
        assert not state["light"]["on"], "the light turns off"
