"""The headgear restriction: what it accepts under each requirement, and what the light does when the head stops
being allowed and is allowed again."""

import pytest

from flashlight import DENIED_HAT

pytestmark = pytest.mark.headgear

SECTION = "ImFl_HeadgearRestriction"


@pytest.mark.parametrize("fallback", [True, False], ids=["fallback", "no fallback"])
def test_helmet_off_and_on(flashlight, loadout, fallback):
    flashlight.set("bRestrictionFallback", str(fallback).lower())
    flashlight.light(location="head", op="on")

    loadout.remove_headgear()
    state = flashlight.wait(lambda s: not s["restrictions"]["headAllowed"] and (s["light"]["location"] != "OnHead" or not s["light"]["on"]), 5, "the light leaving the head")
    if fallback:
        assert state["light"]["on"] and state["light"]["location"] == "InOffhand", "the light moves to the offhand"
    else:
        assert not state["light"]["on"], "the light turns off"

    answer = flashlight.gesture("head")
    assert not answer["fired"], "head tap is inert without a helmet"

    loadout.wear_helmet()
    flashlight.wait(lambda s: s["restrictions"]["headAllowed"] and s["light"]["location"] == "OnHead", 5, "the light back on the head")


@pytest.mark.parametrize(
    "requirement, allowed",
    [("2", False), ("1", True)],
    ids=["Immersive: deny-listed hat refused", "AnyHeadGear: any hat accepted"],
)
def test_deny_listed_hat(flashlight, loadout, requirement, allowed):
    flashlight.set("iFlashlightHeadgearRequirement", requirement, SECTION)
    loadout.remove_headgear()
    loadout.wear_helmet(DENIED_HAT)
    flashlight.wait(lambda s: s["restrictions"]["headAllowed"] is allowed, 3, f"headAllowed {allowed}")
    assert flashlight.restrictions()["headgear"]["allowed"] is allowed


def test_no_requirement_without_headgear(flashlight, loadout):
    flashlight.set("iFlashlightHeadgearRequirement", "0", SECTION)
    loadout.remove_headgear()
    flashlight.wait(lambda s: s["restrictions"]["headAllowed"], 3, "the head allowed bare")
    answer = flashlight.gesture("head")
    assert (answer["fired"], answer["lightOn"], answer["location"]) == (True, True, "OnHead")
