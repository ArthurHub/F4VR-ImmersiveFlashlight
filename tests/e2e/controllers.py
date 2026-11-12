"""Real controller input through devbench's input `pose`: it holds the HMD and both controllers in a ready pose (the
head pitched down, the hands in front, in view), moves a hand to a world point without calibrating, and presses go on
top of the hold, so the framework's input handling sees them as real.

The tests aim a hand at a gesture zone's center (as the mod reports it) and let devbench place it; a zone that moved
with the arm (FRIK leans the body) is aimed at again.
"""

import math
import os
import time

from devbench import DevbenchError
from report import REPORT

# the hold and its presses belong to this owner
OWNER = "imfl-e2e"

# a tap is released within 0.3 s, a long press held over 0.6 s (the framework's defaults); after a gesture fires its
# sphere ignores the next press for 400 ms
TAP_MS = 100
LONG_PRESS_MS = 1000
COOLDOWN_SECONDS = 0.5

# times a zone that moved with the arm is aimed at again
AIMS = 3


class Controllers:
    """devbench's pose for a module: a held ready pose, hands placed into zones and back, presses on top."""

    def __init__(self, bench, flashlight):
        self.bench = bench
        self.flashlight = flashlight
        self.token = None
        self.left_handed = False

    def ready(self):
        capabilities = self.bench.tool("input", action="capabilities")["capabilities"]["vrTrackedSet"]
        return capabilities["available"] and capabilities["ready"]

    def physical(self, hand):
        """The controller (left|right) of a logical hand (offhand|primaryHand), from the mod's handedness."""
        return "left" if (hand == "offhand") != self.left_handed else "right"

    def start(self):
        """Take over the HMD and both controllers in devbench's ready pose."""
        self.left_handed = self.flashlight.zones().get("leftHanded", False)
        self._pose()

    def stop(self):
        """Hand the devices back to the physical ones."""
        if self.token:
            try:
                self.bench.tool("input", action="stop", owner=OWNER, controlToken=self.token)
            except DevbenchError:
                pass
            self.token = None

    def rest(self):
        """Both hands and the head back in devbench's ready pose."""
        self._pose(hands="ready", head="ready")

    def place_in_zone(self, zone, hand, head=None, edge=None, controller=None):
        """Move a logical hand into a gesture's zone and return the zone as the mod reports it. The head turns first
        to head, a devbench head preset ("ready", "down") or {pitch, yaw, lookAt}, as FRIK leans the body with it;
        then the hand is aimed at the zone as it is now, again while it misses.
        edge (outward, forward, up) puts the wand off the center by those fractions of the radius, in the head's frame
        with outward toward the hand's own side; controller {pitch, yaw} points the controller (the yaw as for the left
        one, mirrored for the right)."""
        started = time.monotonic()
        if head is not None:
            self._pose(head=head)
        side = self.physical(hand)
        placement = dict(controller or {})
        if side == "right" and "yaw" in placement:
            placement["yaw"] = -placement["yaw"]
        margin = 1 - (math.hypot(*edge) if edge else 0.0)
        aims = 0
        while aims < AIMS:
            entry, wand = self._zone(zone, hand)
            aim = self._aim(entry, edge, side)
            if aims and math.dist(wand, aim) < entry["radius"] * margin:
                break
            answer = self._pose(hands={side: {"world": aim, **placement}})
            aims += 1
        entry, _ = self._zone(zone, hand)
        self._log(f"{hand} into {zone}", started, aims, answer.get("corrections"), entry["wands"][hand]["distance"], entry["radius"])
        assert entry["wands"][hand]["inside"], f"the {hand} wand is {entry['wands'][hand]['distance']:.1f} units from the {zone} zone's center (radius {entry['radius']})"
        return entry

    def press(self, hand, duration_ms=TAP_MS, buttons=("trigger",)):
        """Hold buttons on a logical hand's controller for duration_ms, on top of the held pose, and wait until they are
        released."""
        self.bench.tool("input", action="press", owner=OWNER, hand=self.physical(hand), buttons=list(buttons), durationMs=duration_ms)
        time.sleep(duration_ms / 1000 + 0.1)

    def tap(self, hand):
        self.press(hand, TAP_MS)

    def long_press(self, hand):
        self.press(hand, LONG_PRESS_MS)

    def _pose(self, **args):
        """One devbench pose call as this owner, keeping its control token for stop."""
        answer = self.bench.tool("input", action="pose", owner=OWNER, **args)
        self.token = answer.get("controlToken") or self.token
        return answer

    def _aim(self, entry, edge, side):
        """The point a hand aims at in a zone: its center, moved by edge (outward, forward, up) fractions of the radius
        along the head's axes (the HMD node's rotation rows: right, forward, up), outward toward the side's own side."""
        if not edge:
            return entry["center"]
        hmd = self.bench.tool("nodes", action="get", root="hmdNode")
        right, forward, up = hmd.get("node", hmd)["world"]["rot"]
        outward = -1.0 if side == "left" else 1.0
        weights = (outward * edge[0], edge[1], edge[2])
        return [c + entry["radius"] * sum(w * axis[i] for w, axis in zip(weights, (right, forward, up))) for i, c in enumerate(entry["center"])]

    def _zone(self, zone, hand):
        zones = self.flashlight.zones()
        return zones["zones"][zone], zones["wands"][hand]

    def _log(self, what, started, aims, corrections, distance, radius):
        """Record a placement in the report and print it."""
        record = {"test": _current_test(), "what": what, "seconds": time.monotonic() - started, "aims": aims, "corrections": corrections, "distance": distance, "radius": radius}
        REPORT.moves.append(record)
        print(f"place {what}: {record['seconds']:.1f}s, {aims} aims, {corrections} devbench corrections, {distance:.1f} / r {radius:.1f} from the center")


def _current_test():
    """The running test's name, as pytest sets it."""
    return os.environ.get("PYTEST_CURRENT_TEST", "").split("::")[-1].rsplit(" ", 1)[0]
