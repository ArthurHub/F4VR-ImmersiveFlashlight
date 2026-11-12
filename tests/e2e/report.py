"""The report of a run: one HTML page with each test's result by area, the game's dialogs the run closed, and notes.

It is written to build/e2e/<date>-<time>/index.html, and the run ends with its link.
"""

import html
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# the areas, by the file of their tests, in the order they run and the page shows them: power armor after the others,
# and health last, its log check covering the whole run
AREAS = {
    "test_light.py": "Light action",
    "test_gestures.py": "Gestures (simulated)",
    "test_overrides.py": "Config overrides",
    "test_headgear.py": "Headgear restriction",
    "test_menus.py": "Menus",
    "test_weapon.py": "Weapon mount",
    "test_controllers.py": "Gestures (controllers)",
    "test_power_armor.py": "Power armor",
    "test_health.py": "Health",
}

STYLE = """
:root { --bg: #ffffff; --fg: #1d2125; --muted: #5f6b76; --line: #d8dde2; --card: #f5f7f9;
  --pass: #1f7a3a; --fail: #b3261e; --skip: #8a6d00; }
@media (prefers-color-scheme: dark) { :root { --bg: #15181b; --fg: #e3e7ea; --muted: #97a3ad; --line: #2c3237;
  --card: #1d2125; --pass: #5cc97f; --fail: #ff8a80; --skip: #e3c55a; } }
* { box-sizing: border-box; }
body { margin: 0; padding: 24px 16px 48px; background: var(--bg); color: var(--fg);
  font: 14px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width: 1100px; margin: 0 auto; }
h1 { font-size: 22px; margin: 0 0 4px; } h2 { font-size: 17px; margin: 32px 0 8px; }
.meta { color: var(--muted); margin-bottom: 16px; }
.totals { display: flex; flex-wrap: wrap; gap: 8px; margin: 12px 0; }
.total { background: var(--card); border: 1px solid var(--line); border-radius: 8px; padding: 8px 14px; min-width: 96px; }
.total b { display: block; font-size: 20px; }
table { width: 100%; border-collapse: collapse; }
th, td { text-align: left; padding: 6px 8px; border-bottom: 1px solid var(--line); vertical-align: top; }
th { color: var(--muted); font-weight: 600; font-size: 12px; text-transform: uppercase; letter-spacing: .04em; }
.passed { color: var(--pass); } .failed, .error { color: var(--fail); } .skipped { color: var(--skip); }
.badge { font-weight: 600; }
pre { white-space: pre-wrap; word-break: break-word; background: var(--card); border: 1px solid var(--line);
  border-radius: 6px; padding: 8px; margin: 6px 0 0; font-size: 12px; }
ul.list { margin: 4px 0 0; padding-left: 18px; font-size: 13px; }
@media (max-width: 640px) { td, th { padding: 4px; } }
"""


class Report:
    """What a run did, collected as it runs and written as one page when it ends."""

    def __init__(self):
        self.started = time.monotonic()
        self.date = datetime.now()
        self.folder = ROOT / "build" / "e2e" / self.date.strftime("%Y%m%d-%H%M%S")
        self.tests = []
        self.dialogs = []
        self.notes = []
        # each hand placed in a gesture zone (controllers.py): its time, corrections and landing
        self.moves = []

    def add_result(self, report):
        """A test's result, from pytest's report of its setup, call or teardown: the call's, or a setup's or a
        teardown's that failed or skipped."""
        if report.when != "call" and report.outcome == "passed":
            return
        nodeid = report.nodeid
        entry = next((test for test in self.tests if test["nodeid"] == nodeid), None)
        outcome = report.outcome if report.when == "call" else ("error" if report.failed else report.outcome)
        message = ""
        if report.failed:
            message = getattr(report.longrepr, "reprcrash", None)
            message = message.message if message else str(report.longrepr)[-3000:]
        elif report.skipped and isinstance(report.longrepr, tuple):
            message = report.longrepr[2]
        if entry:
            if outcome != "passed":
                entry.update(outcome=outcome, message=entry["message"] or message)
            return
        file, _, name = nodeid.partition("::")
        self.tests.append({"nodeid": nodeid, "file": Path(file).name, "name": name, "outcome": outcome, "duration": report.duration, "message": message})

    def write(self):
        """Write the page, and return its path."""
        self.folder.mkdir(parents=True, exist_ok=True)
        page = self.folder / "index.html"
        page.write_text(self._html(), encoding="utf-8")
        return page

    def _html(self):
        counts = {}
        for test in self.tests:
            counts[test["outcome"]] = counts.get(test["outcome"], 0) + 1
        seconds = time.monotonic() - self.started
        out = [
            "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>",
            f"<title>E2E Run Report</title><style>{STYLE}</style></head><body><main>",
            "<h1>Immersive Flashlight VR: end-to-end run</h1>",
            f"<div class='meta'>{self.date:%Y-%m-%d %H:%M} &middot; {seconds / 60:.1f} min &middot; {len(self.tests)} tests</div>",
            "<div class='totals'>",
        ]
        for outcome in ("passed", "failed", "error", "skipped"):
            out.append(f"<div class='total'><b class='{outcome}'>{counts.get(outcome, 0)}</b>{outcome}</div>")
        out.append("</div>")
        for note in self.notes:
            out.append(f"<div class='meta'>{html.escape(note)}</div>")

        failures = [test for test in self.tests if test["outcome"] in ("failed", "error")]
        if failures:
            out.append("<h2>Failures</h2><table><tr><th>Test</th><th>What</th></tr>")
            for test in failures:
                out.append(f"<tr><td class='failed'>{html.escape(test['name'])}<br><small>{html.escape(test['file'])}</small></td>"
                           f"<td><pre>{html.escape(test['message'])}</pre></td></tr>")
            out.append("</table>")

        for file, area in AREAS.items():
            tests = [test for test in self.tests if test["file"] == file]
            if not tests:
                continue
            out.append(f"<h2>{html.escape(area)}</h2>")
            out.append("<table><tr><th>Test</th><th>Result</th><th>Time</th></tr>")
            for test in tests:
                skipped = f"<br><small>{html.escape(test['message'])}</small>" if test["outcome"] == "skipped" and test["message"] else ""
                out.append(f"<tr><td>{html.escape(test['name'])}{skipped}</td><td class='badge {test['outcome']}'>{test['outcome']}</td>"
                           f"<td>{test['duration']:.1f} s</td></tr>")
            out.append("</table>")

        if self.moves:
            total = sum(move["seconds"] for move in self.moves)
            out.append(f"<h2>Controller placements</h2><div class='meta'>{len(self.moves)} placements, {total:.1f} s in all</div>")
            out.append("<table><tr><th>Test</th><th>Placement</th><th>Time</th><th>Aims / devbench corrections</th><th>Landed</th></tr>")
            for move in self.moves:
                out.append(
                    f"<tr><td>{html.escape(move['test'])}</td><td>{html.escape(move['what'])}</td><td>{move['seconds']:.1f} s</td>"
                    f"<td>{move['aims']} / {move['corrections']}</td><td>{move['distance']:.1f} from the center (radius {move['radius']:.1f})</td></tr>"
                )
            out.append("</table>")

        if self.dialogs:
            out.append("<h2>Game dialogs the run closed</h2><ul class='list'>")
            out += [f"<li>{html.escape(said)}</li>" for said in self.dialogs]
            out.append("</ul>")
        out.append("</main></body></html>")
        return "\n".join(out)


REPORT = Report()
