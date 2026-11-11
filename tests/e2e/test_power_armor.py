"""Power armor: stepping in puts the head light on the power-armor head with its beam, the gestures work there, and
stepping out restores the out-of-armor location. QASmoke has frames standing within --power-armor-radius; KabutoVR
blocks the scripted exit, so run with -m "not powerarmor" when it is enabled."""

import pytest

pytestmark = pytest.mark.powerarmor


@pytest.fixture
def frame(flashlight, loadout, pytestconfig):
    """The nearest power armor frame, and the player out of it after the test."""
    radius = pytestconfig.getoption("--power-armor-radius")
    frame = flashlight.power_armor_frame(radius)
    assert frame, f"no power armor frame within {radius:.0f} units: raise --power-armor-radius"
    yield frame
    loadout.leave_power_armor()


def test_power_armor_head(game, flashlight, frame):
    pa_radius = flashlight.config_int("iFlashlightOnPAHeadRadius")
    flashlight.light(location="head", op="on")

    game.console(f'player.cf "Actor.SwitchToPowerArmor" {frame["formId"]}')
    flashlight.wait(lambda s: s["gating"]["inPowerArmor"], 10, f"in the frame {frame['formId']}")
    flashlight.wait(lambda s: s["light"]["on"], 3, "the light restored in power armor")
    # in power armor the light goes where iFlashlightLocationInPA says (the player's file): set it to the head there
    flashlight.light(location="head")
    flashlight.wait(
        lambda s: s["light"]["on"] and s["light"]["location"] == "OnPAHead" and s["light"]["beam"]["radius"] == pa_radius,
        3,
        f"the light on the PA head with its radius {pa_radius}",
    )

    answer = flashlight.gesture("head", press="long")
    assert (answer["fired"], answer["lightOn"], answer["location"]) == (True, True, "InOffhand"), "head long: to the offhand in PA"
    answer = flashlight.gesture("head")
    assert (answer["fired"], answer["lightOn"], answer["location"]) == (True, True, "OnPAHead"), "head tap: back on the PA head"

    game.console('player.cf "Actor.SwitchToPowerArmor" none')
    flashlight.wait(lambda s: not s["gating"]["inPowerArmor"], 10, "out of power armor (KabutoVR blocks the scripted exit)")
    # the out-of-armor location is still the head, set before stepping in
    flashlight.wait(lambda s: s["light"]["on"] and s["light"]["location"] == "OnHead", 3, "the light restored on the head")
