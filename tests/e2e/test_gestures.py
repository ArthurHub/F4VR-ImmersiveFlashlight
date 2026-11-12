"""The gestures' transitions, fired through the gesture action: the real gating and handler, without the zone and the
button (test_controllers.py drives those). Each case starts from its own light state, with no weapon drawn; the weapon's
cases are in test_weapon.py."""

import pytest

from flashlight import LOCATION_ARG

pytestmark = pytest.mark.gestures

# (start on, start location, gesture, press, hand) -> (fired, on, location)
CASES = {
    "head tap: on at the head from off": ((False, "OnHead", "head", "tap", "offhand"), (True, True, "OnHead")),
    "head tap: off on the head": ((True, "OnHead", "head", "tap", "offhand"), (True, False, "OnHead")),
    "head tap: from the offhand to the head": ((True, "InOffhand", "head", "tap", "offhand"), (True, True, "OnHead")),
    "head long: from the head to the offhand": ((True, "OnHead", "head", "long", "offhand"), (True, True, "InOffhand")),
    "head long: inert off the head": ((True, "InOffhand", "head", "long", "offhand"), (False, True, "InOffhand")),
    "primaryHand tap: offhand to primary hand": ((True, "InOffhand", "primaryHand", "tap", "offhand"), (True, True, "InPrimaryHand")),
    "primaryHand tap: primary hand to offhand": ((True, "InPrimaryHand", "primaryHand", "tap", "offhand"), (True, True, "InOffhand")),
    "primaryHand long: inert off the weapon": ((True, "InOffhand", "primaryHand", "long", "offhand"), (False, True, "InOffhand")),
    "primaryHand tap: inert from off without a weapon": ((False, "InOffhand", "primaryHand", "tap", "offhand"), (False, False, "InOffhand")),
    "weaponToggle: inert without a two-handed grip": ((False, "InOffhand", "weaponToggle", "tap", "offhand"), (False, False, "InOffhand")),
    "bodyGrab offhand: returns it (off)": ((True, "InOffhand", "bodyGrab", "tap", "offhand"), (True, False, "InOffhand")),
    "bodyGrab offhand: grabs it from the head": ((True, "OnHead", "bodyGrab", "tap", "offhand"), (True, True, "InOffhand")),
    "bodyGrab primary hand: grabs it (on)": ((False, "InOffhand", "bodyGrab", "tap", "primaryHand"), (True, True, "InPrimaryHand")),
    "bodyGrab offhand: inert while the other hand holds it": ((True, "InPrimaryHand", "bodyGrab", "tap", "offhand"), (False, True, "InPrimaryHand")),
    "bodyGrab primary hand: returns it (off)": ((True, "InPrimaryHand", "bodyGrab", "tap", "primaryHand"), (True, False, "InPrimaryHand")),
}


@pytest.mark.parametrize("start, expected", CASES.values(), ids=CASES.keys())
def test_gesture(flashlight, events, start, expected):
    on, location, gesture, press, hand = start
    flashlight.light(location=LOCATION_ARG[location], op="on" if on else "off")
    answer = flashlight.gesture(gesture, press=press, hand=hand)
    got = (answer["fired"], answer["lightOn"], answer["location"])
    assert got == expected, f"inert: {answer.get('inert')}"

    fired = [event for event in events.all("gesture") if event["data"]["gesture"] == gesture]
    if answer["fired"]:
        assert fired and fired[-1]["data"]["location"] == answer["location"], "a fired gesture publishes its event"
    else:
        assert not fired, "an inert gesture publishes no event"
