"""Game menus that take the controllers block the gestures and leave the light on; the mod's config UI opens and
closes."""

import pytest

pytestmark = pytest.mark.menus


def test_pause_menu_blocks_gestures(game, flashlight):
    flashlight.light(location="head", op="on")
    game.tool("menu", action="open", name="PauseMenu")
    state = flashlight.wait(lambda s: s["gating"]["gesturesBlockedByMenu"], 3, "the gestures blocked")
    assert state["gating"]["blockingMenu"] == "PauseMenu"
    assert state["light"]["on"], "the light stays on behind the menu"

    answer = flashlight.gesture("head")
    assert not answer["fired"] and "menu" in answer["inert"]
    zones = flashlight.zones()["zones"]
    assert not any(zone["enabled"] for zone in zones.values()), "no zone is live behind the menu"

    game.tool("menu", action="close", name="PauseMenu")
    flashlight.wait(lambda s: not s["gating"]["gesturesBlockedByMenu"], 3, "the gestures unblocked")


def test_config_ui_opens_and_closes(flashlight):
    assert flashlight.ui("open")["configOpen"]
    assert not flashlight.ui("close")["configOpen"]
