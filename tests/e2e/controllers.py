"""Real controller input through devbench's Hands: it holds the HMD and both controllers in a ready pose (the head
pitched down, the hands in front, in view), moves a hand to a world point without calibrating, and presses go on top of
the hold, so the framework's input handling sees them as real.

The tests aim a hand at a gesture zone's center (as the mod reports it) and let devbench place it; a zone that moved
with the arm (FRIK leans the body) is aimed at again.
"""

import math
import time

from devbench import Hands
from devbench.vr import LONG_PRESS_MS, TAP_MS
from flashlight import physical_hand

# the hold and its presses belong to this owner
OWNER = "imfl-e2e"

# after a gesture fires its sphere ignores the next press for 400 ms
COOLDOWN_SECONDS = 0.5

# times a zone that moved with the arm is aimed at again
AIMS = 3

# devbench's role name of each of the mod's logical hands, which devbench resolves to a controller by the handedness
ROLE = {"offhand": "offhand", "primaryHand": "primary"}


class Controllers:
    """devbench's Hands for the mod: logical hands (offhand|primaryHand) passed to devbench as roles, hands placed into
    zones and back, presses on top, and a log of each placement for the report, tagged with the test record (the run's
    report) says is running."""

    def __init__(self, bench, flashlight, record):
        self.bench = bench
        self.flashlight = flashlight
        self.record = record
        self.hands = Hands(bench, OWNER)
        self.left_handed = False
        # each hand placed in a gesture zone: its time, aims, corrections and landing
        self.moves = []

    def ready(self):
        return self.hands.ready()

    def physical(self, hand):
        """The controller (left|right) of a logical hand (offhand|primaryHand), from the mod's handedness."""
        return physical_hand(hand, self.left_handed)

    def start(self):
        """Take over the HMD and both controllers in devbench's ready pose."""
        self.left_handed = self.flashlight.left_handed()
        self.hands.start()

    def stop(self):
        """Hand the devices back to the physical ones."""
        self.hands.stop()

    def rest(self):
        """Both hands and the head back in devbench's ready pose."""
        self.hands.rest()

    def place_in_zone(self, zone, hand, head=None, edge=None, controller=None):
        """Move a logical hand into a gesture's zone and return the zone as the mod reports it. The head turns first
        to head, a devbench head preset ("ready", "down") or {pitch, yaw, lookAt}, as FRIK leans the body with it;
        then the hand is aimed at the zone as it is now, again while it misses.
        edge (outward, forward, up) puts the wand off the center by those fractions of the radius, in the head's frame
        with outward toward the hand's own side (devbench resolves it); controller {pitch, yaw_in} points the controller,
        yaw_in turning it toward the body for either hand."""
        started = time.monotonic()
        if head is not None:
            self.hands.head(head)
        side = self.physical(hand)
        pitch, yaw_in = (controller or {}).get("pitch"), (controller or {}).get("yaw_in")
        margin = 1 - (math.hypot(*edge) if edge else 0.0)
        aims = 0
        while aims < AIMS:
            entry, wand = self._zone(zone, hand)
            if aims:
                # the point devbench aimed at, moved with the zone since; the head stays, so its offset holds
                target = answer["hands"][side]["target"]
                aim = [t + now - then for t, now, then in zip(target, entry["center"], placed_center)]
                if math.dist(wand, aim) < entry["radius"] * margin:
                    break
            offset = [entry["radius"] * e for e in edge] if edge else None
            answer = self.hands.place(ROLE[hand], entry["center"], head_offset=offset, pitch=pitch, yaw_in=yaw_in)
            placed_center = entry["center"]
            aims += 1
        entry, _ = self._zone(zone, hand)
        self._log(f"{hand} into {zone}", started, aims, answer.get("corrections"), entry["wands"][hand]["distance"], entry["radius"])
        assert entry["wands"][hand]["inside"], f"the {hand} wand is {entry['wands'][hand]['distance']:.1f} units from the {zone} zone's center (radius {entry['radius']})"
        return entry

    def press(self, hand, duration_ms=TAP_MS, buttons=("trigger",)):
        """Hold buttons on a logical hand's controller for duration_ms, on top of the held pose, and wait until they are
        released."""
        self.hands.press(ROLE[hand], buttons, duration_ms)

    def tap(self, hand):
        self.press(hand, TAP_MS)

    def long_press(self, hand):
        self.press(hand, LONG_PRESS_MS)

    def write_section(self, report):
        """Add the placements to the report as its "Controller placements" table, with their count and total time."""
        if not self.moves:
            return
        total = sum(move["seconds"] for move in self.moves)
        report.section(
            "Controller placements",
            intro=f"{len(self.moves)} placements, {total:.1f} s in all",
            columns=["Test", "Placement", "Time", "Aims / devbench corrections", "Landed"],
            rows=[
                (move["test"], move["what"], f"{move['seconds']:.1f} s", f"{move['aims']} / {move['corrections']}", f"{move['distance']:.1f} from the center (radius {move['radius']:.1f})")
                for move in self.moves
            ],
        )

    def _zone(self, zone, hand):
        zones = self.flashlight.zones()
        return zones["zones"][zone], zones["wands"][hand]

    def _log(self, what, started, aims, corrections, distance, radius):
        """Keep a placement for the report and print it."""
        record = {"test": self.record.current_test, "what": what, "seconds": time.monotonic() - started, "aims": aims, "corrections": corrections, "distance": distance, "radius": radius}
        self.moves.append(record)
        print(f"place {what}: {record['seconds']:.1f}s, {aims} aims, {corrections} devbench corrections, {distance:.1f} / r {radius:.1f} from the center")
