"""The light action: switching the configured location and turning the light on and off, as the gestures do, and
each location's beam."""

import pytest

pytestmark = pytest.mark.light


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
