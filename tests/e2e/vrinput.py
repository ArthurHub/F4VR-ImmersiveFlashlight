"""Real controller input through devbench's input tool: a reach moves a controller until its wand sits at a player
node and holds it there, and presses go on top of that hold, so the framework's input handling sees them as real.

Every reach calibrates first (about 16 s per hand) and starts from the same standing rest pose, so a world offset
measured during one reach lands the same way in the next. That is how a hand is put into a zone whose node a reach
can't name: the body grab's chest bone resolves to the first-person skeleton's, far from the body.
"""

import time

from devbench import DevbenchError

# the reach and its presses belong to this owner; one reach runs at a time
OWNER = "imfl-e2e"

# long enough for every press a test makes while one reach holds
HOLD_MS = 180_000
REACH_TIMEOUT_SECONDS = 90

# a tap is released within 0.3 s, a long press held over 0.6 s (the framework's defaults); after a gesture fires its
# sphere ignores the next press for 400 ms
TAP_MS = 100
LONG_PRESS_MS = 1000
COOLDOWN_SECONDS = 0.5


class VRInput:
    """devbench's input tool, for one test: a reach at a time, presses on top of it."""

    def __init__(self, bench, flashlight):
        self.bench = bench
        self.flashlight = flashlight

    def ready(self):
        capabilities = self.bench.tool("input", action="capabilities")["capabilities"]["vrTrackedSet"]
        return capabilities["available"] and capabilities["ready"]

    def physical(self, hand):
        """The controller (left|right) of a logical hand (offhand|primaryHand), from the mod's handedness."""
        left_handed = self.flashlight.zones().get("leftHanded", False)
        return "left" if (hand == "offhand") != left_handed else "right"

    def stop(self):
        """End the reach, and wait until it has: reachStop answers before the run lets the devices go."""
        try:
            if self.bench.tool("input", action="reachStop", owner=OWNER).get("notActive"):
                return
            deadline = time.monotonic() + 5
            while self.bench.tool("input", action="reachStatus").get("active") and time.monotonic() < deadline:
                time.sleep(0.1)
        except DevbenchError:
            # no reach has run: there is none to stop, and reachStatus has no run to answer for
            return

    def reach(self, hand, node, front=None, offset=None):
        """Move a logical hand's controller to a player node (plus front units along the facing and a world offset),
        and wait until it holds there."""
        self.stop()
        target = {"node": node, **({"front": front} if front is not None else {}), **({"offset": offset} if offset else {})}
        run = self.bench.tool("input", action="reach", owner=OWNER, hands={self.physical(hand): target}, holdMs=HOLD_MS)["runId"]
        deadline = time.monotonic() + REACH_TIMEOUT_SECONDS
        while True:
            status = self.bench.tool("input", action="reachStatus", runId=run)
            if status["phase"] == "holding":
                return status
            if status["phase"] in ("done", "failed", "stopped"):
                raise AssertionError(f"the reach to {node} ended {status['phase']}: {status.get('error')}")
            if time.monotonic() > deadline:
                raise AssertionError(f"the reach to {node} did not hold within {REACH_TIMEOUT_SECONDS} s: {status['phase']}")
            time.sleep(0.5)

    def reach_into_zone(self, zone, hand, node="hmdNode", front=10):
        """Put a logical hand inside a gesture's zone and return the zone as the mod reports it: a reach to node first,
        then, when the wand isn't inside, a second one shifted by how far the wand is from the zone's center."""
        self.reach(hand, node, front)
        entry, wand = self._zone(zone, hand)
        if entry["wands"][hand]["inside"]:
            return entry
        offset = [center - at for center, at in zip(entry["center"], wand)]
        self.reach(hand, node, front, offset)
        entry, _ = self._zone(zone, hand)
        assert entry["wands"][hand]["inside"], f"the {hand} wand is {entry['wands'][hand]['distance']:.1f} units from the {zone} zone's center (radius {entry['radius']})"
        return entry

    def _zone(self, zone, hand):
        zones = self.flashlight.zones()
        return zones["zones"][zone], zones["wands"][hand]

    def press(self, hand, duration_ms=TAP_MS, buttons=("trigger",)):
        """Hold buttons on a logical hand's controller for duration_ms, on top of the reach's hold, and wait until
        they are released."""
        self.bench.tool("input", action="press", owner=OWNER, hand=self.physical(hand), buttons=list(buttons), durationMs=duration_ms)
        time.sleep(duration_ms / 1000 + 0.1)

    def tap(self, hand):
        self.press(hand, TAP_MS)

    def long_press(self, hand):
        self.press(hand, LONG_PRESS_MS)
