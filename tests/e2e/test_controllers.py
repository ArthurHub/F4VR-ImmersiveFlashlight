"""The gestures driven for real: devbench's input tool holds the HMD and both controllers in a ready pose (the hands in
front, in view), moves a hand into a gesture's zone and presses its trigger, so the zone test, the binding, the
suppression and the handler all run as with a headset. Each test puts the hands back in the ready pose after."""

import time

import pytest

from controllers import COOLDOWN_SECONDS, Controllers
from devbench import DevbenchError

pytestmark = pytest.mark.controllers

# the head gesture as recorded in the headset: the wand at the head zone's offhand side (fractions of its radius:
# outward, forward, up), the controller tilted back past vertical and turned in toward the face
HEAD_EDGE = (0.8, -0.1, -0.15)
HEAD_CONTROLLER = {"pitch": 123, "yaw": 14}


@pytest.fixture(scope="module")
def held(game, loadout):
    """devbench's input holding the ready pose for the module, handed back to the physical devices after it."""
    controllers = Controllers(game, loadout.flashlight)
    if not controllers.ready():
        pytest.fail("devbench's input tool is not ready (no OpenVR, or its hooks are not in place)")
    controllers.start()
    yield controllers
    controllers.stop()


@pytest.fixture
def vr(held, flashlight):
    """The held controllers for a test, back in the ready pose after it."""
    yield held
    held.rest()


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


def blocked_by(flashlight):
    """What blocks the gestures, for a failure message: the menu the mod names, and a message box's text."""
    menu = flashlight.state()["gating"]["blockingMenu"]
    if menu == "MessageBoxMenu":
        try:
            box = flashlight.bench.tool("menu", action="describe")
            menu += f": {box.get('headerText') or ''} {box.get('bodyText') or ''}".rstrip()
        except DevbenchError as error:
            menu += f" (not read: {error})"
    return menu


def assert_suppressing(flashlight, zone, binding="primary"):
    """The zone is live and swallows the binding's button while the hand is inside, so the game doesn't see it."""
    assert zone["enabled"], f"the zone is live (gestures blocked by: {blocked_by(flashlight)})"
    assert zone[binding] and zone[binding]["suppressing"], f"the {binding} binding is suppressed inside the zone: {zone[binding]}"


def binding_icons(flashlight, zone):
    """The binding icons a zone shows now, by their file names in the icons folder."""
    return [path.rsplit("\\", 1)[-1] for path in flashlight.zones()["zones"][zone]["bindingIcons"]]


def test_outside_the_zones(flashlight, vr, events):
    """A trigger press with the hands in the ready pose fires nothing, and the zones don't swallow the button."""
    zones = flashlight.zones()["zones"]
    assert not any(zone["wands"]["offhand"]["inside"] for zone in zones.values() if "wands" in zone), "the offhand is in no zone"
    assert not any((zone[b] or {}).get("suppressing") for zone in zones.values() for b in ("primary", "secondary"))
    flashlight.light(location="offhand", op="on")
    press_and_expect(flashlight, events, lambda: vr.tap("offhand"), None, "tap outside the zones")


def test_head_zone(flashlight, vr, events):
    # the zone sits just in front of the eyes: the hand goes where a player's rests, at the zone's offhand side with the
    # controller tilted back, so the hand hangs beside the head instead of in the face
    zone = vr.place_in_zone("head", "offhand", edge=HEAD_EDGE, controller=HEAD_CONTROLLER)
    assert_suppressing(flashlight, zone)
    tap, long = (lambda: vr.tap("offhand")), (lambda: vr.long_press("offhand"))
    press_and_expect(flashlight, events, tap, (True, "OnHead"), "tap: on at the head")
    press_and_expect(flashlight, events, tap, (False, "OnHead"), "tap: off on the head")
    press_and_expect(flashlight, events, tap, (True, "OnHead"), "tap: on again")
    press_and_expect(flashlight, events, long, (True, "InOffhand"), "long: to the offhand")
    press_and_expect(flashlight, events, long, None, "long: off the head")
    press_and_expect(flashlight, events, tap, (True, "OnHead"), "tap: from the offhand back to the head")


def test_primary_hand_zone(flashlight, vr, events):
    flashlight.light(location="offhand", op="on")
    zone = vr.place_in_zone("primaryHand", "offhand")
    assert_suppressing(flashlight, zone)
    # the shipped config shows all the bindings fed now: the tap only, as the long press pulls the light off the weapon
    # and is gated off while the light is in a hand
    assert binding_icons(flashlight, "primaryHand") == [f"{vr.physical('offhand')}-trigger.dds"]
    tap, long = (lambda: vr.tap("offhand")), (lambda: vr.long_press("offhand"))
    press_and_expect(flashlight, events, tap, (True, "InPrimaryHand"), "tap: offhand to primary hand")
    press_and_expect(flashlight, events, tap, (True, "InOffhand"), "tap: primary hand to offhand")
    press_and_expect(flashlight, events, long, None, "long: inert off the weapon")


@pytest.mark.weapon
@pytest.mark.usefixtures("weapon")
def test_primary_hand_zone_with_weapon(flashlight, vr, events):
    """With a weapon drawn, the offhand at the primary hand puts the light on the weapon, turns it off and on there, and
    the long press takes it back to the offhand. The long press's icon shows only while the light is on the weapon."""
    flashlight.set("iWeaponFlashlightMeshRequired", "0", "ImFl_WeaponMount")  # any weapon carries the light, lamp or not
    flashlight.light(location="offhand", op="on")
    side = vr.physical("offhand")
    tap_icon, long_icon = f"{side}-trigger.dds", f"{side}-longpress-trigger.dds"
    tap, long = (lambda: vr.tap("offhand")), (lambda: vr.long_press("offhand"))

    assert_suppressing(flashlight, vr.place_in_zone("primaryHand", "offhand"))
    assert binding_icons(flashlight, "primaryHand") == [tap_icon], "in the offhand, only the tap can fire"
    press_and_expect(flashlight, events, tap, (True, "OnWeapon"), "tap: offhand to weapon")

    # the hand is aimed at the zone again, as it may follow the light onto the weapon
    assert_suppressing(flashlight, vr.place_in_zone("primaryHand", "offhand"), "secondary")
    assert binding_icons(flashlight, "primaryHand") == [tap_icon, long_icon], "on the weapon, the long press can fire too"
    press_and_expect(flashlight, events, tap, (False, "OnWeapon"), "tap: off on the weapon")
    press_and_expect(flashlight, events, tap, (True, "OnWeapon"), "tap: on again at the weapon")

    vr.place_in_zone("primaryHand", "offhand")
    press_and_expect(flashlight, events, long, (True, "InOffhand"), "long: weapon to offhand")
    assert binding_icons(flashlight, "primaryHand") == [tap_icon], "back in the offhand, only the tap"


@pytest.mark.parametrize("hand, location", [("offhand", "InOffhand"), ("primaryHand", "InPrimaryHand")])
def test_body_grab(flashlight, vr, events, hand, location):
    # the head looks down at the belt first (devbench's "down"): FRIK leans the body with it, and the zone is read in
    # that pose
    zone = vr.place_in_zone("bodyGrab", hand, head="down")
    assert_suppressing(flashlight, zone, "primary" if hand == "offhand" else "secondary")
    # the shipped config shows the bindings of the hand inside only
    assert binding_icons(flashlight, "bodyGrab") == [f"{vr.physical(hand)}-trigger.dds"]
    tap =lambda: vr.tap(hand)  # noqa: E731
    press_and_expect(flashlight, events, tap, (True, location), "tap: grabs it from the belt")
    press_and_expect(flashlight, events, tap, (False, location), "tap: returns it")
