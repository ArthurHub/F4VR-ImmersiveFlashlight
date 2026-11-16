"""Power armor: in a frame the head light goes on the power-armor head with its beam, the head gestures work there, and
out of it the out-of-armor location comes back. devbench's player tool gets the player in and out, at once as setup or
by stepping in and out through the controllers. QASmoke has frames standing within --power-armor-radius."""

import pytest

from devbench import DevbenchError
from devbench.games import fallout4

# the runtime location in power armor of each configured location
IN_PA = {"OnHead": "OnPAHead", "InOffhand": "InOffhand", "InPrimaryHand": "InPrimaryHand"}


@pytest.fixture
def frame(game, loadout, pytestconfig):
    """The nearest power armor frame, and the player out of it after the test."""
    radius = pytestconfig.getoption("--power-armor-radius")
    frame = fallout4.nearest_power_armor(game, radius)
    assert frame, f"no power armor frame within {radius:.0f} units: raise --power-armor-radius"
    yield frame
    loadout.leave_power_armor()


def assert_switched(answer, what):
    """The switch happened now: devbench's helpers also accept already, which these tests rule out."""
    assert answer["outcome"] == "switched", f"{what}: {answer['outcome']} ({answer.get('note')})"


def test_power_armor_head(game, flashlight, frame):
    pa_radius = flashlight.config_int("iFlashlightOnPAHeadRadius")
    flashlight.light(location="head", op="on")

    assert_switched(fallout4.enter_power_armor(game, form_id=frame["formId"]), f"into the frame {frame['formId']}")
    flashlight.wait(lambda s: s["gating"]["inPowerArmor"] and s["light"]["on"], 5, "the light restored in power armor")
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

    try:
        answer = fallout4.exit_power_armor(game)
    except DevbenchError as error:
        if isinstance(error.answer, dict) and error.answer.get("outcome") == "refused":
            pytest.skip("a mod (KabutoVR) refused the scripted exit: test_step_in_and_out covers getting out")
        raise
    assert_switched(answer, "out of the frame")
    # the out-of-armor location is still the head, set before stepping in
    flashlight.wait(lambda s: not s["gating"]["inPowerArmor"] and s["light"]["on"] and s["light"]["location"] == "OnHead", 5, "the light restored on the head")


def test_step_in_and_out(game, flashlight, frame):
    """The transition a player makes: walking into the frame (aim, A on the primary hand) and the exit gesture (held
    A/X on the offhand), with their animations and FRIK's skeleton swap. The light comes back on at each side's
    location."""
    flashlight.light(location="head", op="on")

    assert_switched(fallout4.step_into_power_armor(game, form_id=frame["formId"]), f"stepping into the frame {frame['formId']}")
    state = flashlight.wait(lambda s: s["gating"]["inPowerArmor"] and s["light"]["on"], 5, "the light restored in power armor")
    expected = IN_PA[state["light"]["configLocation"]]
    assert state["light"]["location"] == expected, f"the light at the in-armor location {state['light']['configLocation']}"

    assert_switched(fallout4.step_out_of_power_armor(game), "stepping out")
    flashlight.wait(lambda s: not s["gating"]["inPowerArmor"] and s["light"]["on"] and s["light"]["location"] == "OnHead", 5, "the light restored on the head")
