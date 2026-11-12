"""The light action: switching the configured location and turning the light on and off, as the gestures do, and
each location's beam."""

import math

import pytest

from devbench import wait_until

pytestmark = pytest.mark.light

# how far the light may hang from its hand's wand, in game units (FRIK's hand bone follows the wand)
HAND_REACH = 20


def where(answer):
    return answer["lightOn"], answer["location"]


@pytest.mark.parametrize(
    "location, landed, radius_key",
    [
        ("head", "OnHead", "iFlashlightOnHeadRadius"),
        ("offhand", "InOffhand", "iFlashlightInHandRadius"),
    ],
)
def test_location_on_with_its_beam(flashlight, location, landed, radius_key):
    assert where(flashlight.light(location=location, op="on")) == (True, landed)
    radius = flashlight.config_int(radius_key)
    flashlight.wait(lambda s: s["light"]["beam"]["radius"] == radius, 2, f"the beam radius {radius} of {radius_key}")


def test_primary_hand_without_a_weapon(flashlight):
    assert where(flashlight.light(location="primaryHand", op="on")) == (True, "InPrimaryHand")


def test_on_off_toggle(flashlight):
    flashlight.light(location="primaryHand", op="on")
    assert where(flashlight.light(op="off")) == (False, "InPrimaryHand")
    assert where(flashlight.light(op="toggle")) == (True, "InPrimaryHand")
    assert where(flashlight.light(op="toggle")) == (False, "InPrimaryHand")


def test_state_follows(flashlight):
    """The state action, read from a frame snapshot, catches up with the action."""
    flashlight.light(location="offhand", op="on")
    flashlight.wait(lambda s: s["light"]["on"] and s["light"]["location"] == "InOffhand" and s["light"]["configLocation"] == "InOffhand", 2, "the state")


@pytest.mark.parametrize("hand, location", [("offhand", "InOffhand"), ("primaryHand", "InPrimaryHand")])
def test_light_in_its_physical_hand(flashlight, hand, location):
    """The light in a hand hangs under that hand's bone, at its wand: the left or the right one by the handedness."""
    assert where(flashlight.light(location=hand, op="on")) == (True, location)
    bone = f"{flashlight.physical(hand)[0].upper()}Arm_Hand"

    def under_bone():
        node = flashlight.light_node()
        return node if node and f"/{bone}/" in node["path"] else None

    node = wait_until(under_bone, 3, what=f"the light under {bone}")
    distance = math.dist(node["world"]["pos"], flashlight.zones()["wands"][hand])
    assert distance < HAND_REACH, f"the light {distance:.1f} units from the {hand} wand"
