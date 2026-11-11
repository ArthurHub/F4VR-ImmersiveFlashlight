"""The config's session overrides, as Mods Config's live preview uses them: applied to the live light, and taken
back to the file's value."""

import json

import pytest

pytestmark = pytest.mark.overrides


def test_override_applies_live_and_clears(flashlight):
    radius = flashlight.config_int("iFlashlightOnHeadRadius")
    flashlight.light(location="head", op="on")
    flashlight.set("iFlashlightOnHeadRadius", 1234)
    flashlight.wait(lambda s: s["light"]["beam"]["radius"] == 1234, 3, "the override on the live beam")

    flashlight.clear("iFlashlightOnHeadRadius")
    flashlight.wait(lambda s: s["light"]["beam"]["radius"] == radius, 3, f"the file's radius {radius} back")
    assert "iFlashlightOnHeadRadius" not in json.dumps(flashlight.overrides())
