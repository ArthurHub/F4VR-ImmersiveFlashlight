"""The gestures driven for real: devbench's input tool puts a controller inside a gesture's zone and presses its
trigger, so the zone test, the binding, the suppression and the handler all run as with a headset. Each test holds
one reach (about 20 s, 40 s when a second one corrects it) and makes its presses during the hold."""

import time

import pytest

from vrinput import COOLDOWN_SECONDS, VRInput

pytestmark = pytest.mark.input


@pytest.fixture
def vr(game, flashlight):
    """devbench's input, with the reach stopped after the test so the physical controllers take over again."""
    vr = VRInput(game, flashlight)
    if not vr.ready():
        pytest.fail("devbench's input tool is not ready (no OpenVR, or its hooks are not in place)")
    yield vr
    vr.stop()


def press_and_expect(flashlight, events, press, expected, what):
    """Make a press, then wait until the light is where expected (on, location) says; an expected None means the press
    is inert: the light stays as it was and no gesture fires."""
    before = flashlight.state()["light"]
    fired_before = len(events.all("gesture"))
    press()
    if expected is None:
        time.sleep(1)
        state = flashlight.state()["light"]
        assert (state["on"], state["location"]) == (before["on"], before["location"]), f"{what}: inert, the light stays"
        assert len(events.all("gesture")) == fired_before, f"{what}: inert, no gesture fires"
    else:
        on, location = expected
        flashlight.wait(lambda s: s["light"]["on"] is on and s["light"]["location"] == location, 3, what)
        assert len(events.all("gesture")) == fired_before + 1, f"{what}: one gesture fires"
    time.sleep(COOLDOWN_SECONDS)


def assert_suppressing(zone, binding="primary"):
    """The zone is live and swallows the binding's button while the hand is inside, so the game doesn't see it."""
    assert zone["enabled"], "the zone is live"
    assert zone[binding] and zone[binding]["suppressing"], f"the {binding} binding is suppressed inside the zone: {zone[binding]}"


def test_head_zone(flashlight, vr, events):
    zone = vr.reach_into_zone("head", "offhand")
    assert_suppressing(zone)
    tap, long = (lambda: vr.tap("offhand")), (lambda: vr.long_press("offhand"))
    press_and_expect(flashlight, events, tap, (True, "OnHead"), "tap: on at the head")
    press_and_expect(flashlight, events, tap, (False, "OnHead"), "tap: off on the head")
    press_and_expect(flashlight, events, tap, (True, "OnHead"), "tap: on again")
    press_and_expect(flashlight, events, long, (True, "InOffhand"), "long: to the offhand")
    press_and_expect(flashlight, events, long, None, "long: off the head")
    press_and_expect(flashlight, events, tap, (True, "OnHead"), "tap: from the offhand back to the head")


def test_primary_hand_zone(flashlight, vr, events):
    flashlight.light(location="offhand", op="on")
    zone = vr.reach_into_zone("primaryHand", "offhand", node="primaryWandNode", front=None)
    assert_suppressing(zone)
    tap, long = (lambda: vr.tap("offhand")), (lambda: vr.long_press("offhand"))
    press_and_expect(flashlight, events, tap, (True, "InPrimaryHand"), "tap: offhand to primary hand")
    press_and_expect(flashlight, events, tap, (True, "InOffhand"), "tap: primary hand to offhand")
    press_and_expect(flashlight, events, long, None, "long: inert off the weapon")


@pytest.mark.parametrize("hand, location", [("offhand", "InOffhand"), ("primaryHand", "InPrimaryHand")])
def test_body_grab(flashlight, vr, events, hand, location):
    # the zone hangs off the body's chest bone, not the first-person skeleton's of the same name
    zone = vr.reach_into_zone("bodyGrab", hand, node=None, front=None, root="player")
    assert_suppressing(zone, "primary" if hand == "offhand" else "secondary")
    tap = lambda: vr.tap(hand)  # noqa: E731
    press_and_expect(flashlight, events, tap, (True, location), "tap: grabs it from the belt")
    press_and_expect(flashlight, events, tap, (False, location), "tap: returns it")


def test_outside_the_zones(flashlight, vr, events):
    """A trigger press with the hand in no zone fires nothing, and the zones don't swallow the button."""
    vr.reach("offhand", "hmdNode", front=40)
    zones = flashlight.zones()["zones"]
    assert not any(zone["wands"]["offhand"]["inside"] for zone in zones.values() if "wands" in zone), "the offhand is in no zone"
    assert not any((zone[b] or {}).get("suppressing") for zone in zones.values() for b in ("primary", "secondary"))
    flashlight.light(location="offhand", op="on")
    press_and_expect(flashlight, events, lambda: vr.tap("offhand"), None, "tap outside the zones")
