"""The mod's health: the stowed model in the scene, its per-frame cost, and no error in its log over the run. Runs
last (conftest orders the areas), so the log check covers every test before it."""

import re
import time

import pytest

pytestmark = pytest.mark.health

# the mod's whole frame on the game thread, and its budget at p95 with the light held in the offhand (the model
# shown, the light placed every frame)
FRAME_SITE = "Flashlight::onFrameUpdate"
FRAME_P95_BUDGET_MS = 0.5
HOLD_SECONDS = 5


def test_stowed_model_in_scene(game, flashlight):
    matches = game.tool("nodes", action="find", name="ImmersiveFlashlightBody")["matches"]
    assert matches, "the stowed body model is in the scene"


def test_frame_cost(flashlight):
    flashlight.light(location="offhand", op="on")
    flashlight.perf(reset=True)
    time.sleep(HOLD_SECONDS)
    perf = flashlight.perf()
    site = perf["sites"].get(FRAME_SITE)
    assert site, f"no {FRAME_SITE} in perf: {sorted(perf['sites'])[:10]}"
    assert site["p95Ms"] <= FRAME_P95_BUDGET_MS, f"p95 {site['p95Ms']:.3f} ms over {perf['frames']} frames"


def test_no_errors_in_log(log_mark):
    errors = log_mark.errors()
    warnings = [line for line in log_mark.lines() if re.match(r"^\S+ warn(ing)? ", line)]
    if warnings:
        print("log warnings:", *warnings[:5], sep="\n  ")
    assert not errors, "\n".join(errors[:5])
