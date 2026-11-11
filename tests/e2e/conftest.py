"""The session the end-to-end tests run in: devbench, a game in QASmoke, the backup of the mod's config, the player's
loadout, and a light reset before each test."""

import re
import time
from pathlib import Path

import pytest

import backup
from devbench import Devbench, DevbenchError, Events, find_port, wait_until
from flashlight import HELMET, Flashlight, Loadout, close_game_menus
from report import AREAS, REPORT

ROOT = Path(__file__).resolve().parents[2]
LOG_FILE = "ImmersiveFlashlightVR.log"

# the actions the tests call that an older build of the mod lacks
REQUIRED_ACTIONS = ["gesture", "zones", "light", "restrictions", "ui"]

# a new game is stable once no mod's dialog has popped up for QUIET_SECONDS, which gives its quests time to start
# before a test reads what they set; a game that keeps popping them up is used after SETTLE_LIMIT_SECONDS
QUIET_SECONDS = 10
SETTLE_LIMIT_SECONDS = 120

# what the session found at its start, for the tests that check the run as a whole
RUN = {"log_start": 0}

# the settings every test starts with, as session overrides, so a result doesn't depend on the player's config file:
# (key, section, value); a test overrides one of them again to try another value
BASELINE = [
    ("bRestrictionFallback", None, "true"),
    ("iFlashlightHeadgearRequirement", "ImFl_HeadgearRestriction", "2"),
    ("iWeaponFlashlightMeshRequired", "ImFl_WeaponMount", "0"),
    ("bShowFlashlightOnBody", "ImFl_BodyStow", "true"),
]


def pytest_addoption(parser):
    parser.addoption("--devbench-port", type=int, help="devbench's port; by default the one its runtime.json has")
    parser.addoption("--e2e-restore", action="store_true", help="put back the files of a backup a run left, and run no test")
    parser.addoption("--power-armor-radius", type=float, default=2000.0, help="how far to look for a power armor frame, in game units")


def pytest_sessionstart(session):
    if not session.config.getoption("--e2e-restore"):
        return
    if not backup.pending():
        pytest.exit("no backup is waiting to be put back", returncode=0)
    bench = _connect(session.config)
    written, deleted = backup.restore(bench)
    pytest.exit(f"put back the backup: {len(written)} files written, {len(deleted)} deleted", returncode=0)


def pytest_collection_modifyitems(items):
    """Run the areas in the report's order, so the health checks (the log of the whole run) come last."""
    order = list(AREAS)
    items.sort(key=lambda item: order.index(item.path.name) if item.path.name in order else len(order))


def pytest_runtest_logreport(report):
    REPORT.add_result(report)


def pytest_terminal_summary(terminalreporter, config):
    if config.getoption("collectonly") or not REPORT.tests:
        return
    terminalreporter.write_line(f"Report: {REPORT.write().as_uri()}", bold=True)


def _connect(config):
    port = config.getoption("--devbench-port") or find_port()
    bench = Devbench(port)
    try:
        ping = bench.tool("ping")
    except (OSError, DevbenchError) as error:
        pytest.exit(f"devbench does not answer on port {port}: {error}", returncode=2)
    if ping.get("game") != "fallout4" or not ping.get("vr"):
        pytest.exit(f"devbench on port {port} is not Fallout 4 VR: {ping}", returncode=2)
    return bench


@pytest.fixture(scope="session")
def bench(pytestconfig):
    bench = _connect(pytestconfig)
    yield bench
    bench.close()


def close_dialogs(bench):
    """Answer each game message box that is open with its first button, as the mods' popups of a new game block it,
    and return what each said."""
    answered = []
    while bench.tool("menu", action="list")["messageBoxOpen"] and len(answered) < 20:
        try:
            box = bench.tool("menu", action="describe")
            said = f"{box.get('headerText') or ''} {box.get('bodyText') or ''}".strip()
        except DevbenchError as error:
            said = f"(not read: {error})"
        bench.tool("menu", action="accept", index=0)
        print(f"closed a game dialog: {said[:200]!r}")
        answered.append(said)
        REPORT.dialogs.append(said)
        time.sleep(0.5)
    return answered


def _settle(bench):
    """Wait until no dialog has popped up for QUIET_SECONDS, closing each one that does."""
    deadline = time.monotonic() + SETTLE_LIMIT_SECONDS
    quiet_since = time.monotonic()
    while time.monotonic() < deadline:
        if close_dialogs(bench):
            quiet_since = time.monotonic()
        elif time.monotonic() - quiet_since >= QUIET_SECONDS:
            return
        time.sleep(1)
    print(f"the game still pops up dialogs after {SETTLE_LIMIT_SECONDS} s: the tests start anyway")


def _start_game(bench):
    """A game in QASmoke, so nothing the tests do reaches a save: a new one from the main menu, or the one that is
    loaded when it is in QASmoke already."""
    # the console's buffer drops what a command prints until the Console menu is first made
    bench.tool("menu", action="open", name="Console")
    bench.tool("menu", action="close", name="Console")
    # the player exists at the main menu too: the main menu is what tells it from a game
    if "MainMenu" not in bench.open_menus():
        if not bench.tool("inspect", kind="state")["playerLoaded"]:
            pytest.exit("the game is neither at the main menu nor in a game", returncode=2)
        if bench.console_number("player.GetInCell QASmoke") != 1:
            pytest.exit("a game that is not in QASmoke is loaded: quit to the main menu, and the tests start one there", returncode=2)
        close_dialogs(bench)
        REPORT.notes.append("Ran in the game already in QASmoke.")
        return
    REPORT.notes.append("Started a new game in QASmoke from the main menu.")
    bench.console("coc qasmoke")

    def in_game():
        close_dialogs(bench)
        menus = bench.open_menus()
        return "HUDMenu" in menus and "MainMenu" not in menus and "LoadingMenu" not in menus

    # a coc is neither a new game nor a load to F4SE: the game is in once the main menu and the loading screen are
    # gone and the HUD is up
    wait_until(in_game, 300, interval=1, what="the game in QASmoke")
    _settle(bench)


def _check_mod(bench):
    """The mod answers with the player's 3D up, and is the build of this checkout."""
    try:
        health = bench.mod("health")
    except DevbenchError as error:
        pytest.exit(f"the mod's devbench tool does not answer: {error}", returncode=2)
    version = re.search(r"^set\(VERSION ([^)]+)\)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), re.MULTILINE).group(1)
    if health["version"] != version:
        pytest.exit(f"the game runs the mod {health['version']}, the checkout is {version}: deploy this build", returncode=2)
    missing = [action for action in REQUIRED_ACTIONS if action not in health["actions"]]
    if missing:
        pytest.exit(f"the mod's devbench tool lacks {', '.join(missing)}: deploy this build", returncode=2)
    wait_until(lambda: bench.mod("state")["liveness"]["ready"], 30, interval=0.5, what="the flashlight up (the player's 3D)")
    REPORT.notes.append(f"Immersive Flashlight VR {health['version']}.")


def log_line_count(bench):
    return bench.tool("log", action="tail", file=LOG_FILE, tail=1)["linesScanned"]


@pytest.fixture(scope="session")
def game(bench):
    """The game the tests run in, with the mod's config backed up for the whole run and put back after it."""
    if backup.pending():
        backup.restore(bench)
    _start_game(bench)
    _check_mod(bench)
    files = backup.take(bench)
    RUN["log_start"] = log_line_count(bench)
    yield bench
    try:
        Flashlight(bench).clear()
    finally:
        written, deleted = backup.restore(bench)
        REPORT.notes.append(f"Backed up {files} config files, and put them back after: {len(written)} written again, {len(deleted)} the run made deleted.")


@pytest.fixture(scope="session")
def loadout(game):
    """The player's loadout, which a test changes through it. QASmoke's player wears the Pip-Boy, without which there
    is no flashlight."""
    loadout = Loadout(game, Flashlight(game))
    if not loadout.flashlight.state()["gating"]["flashlightAvailable"]:
        pytest.exit("the flashlight isn't available: the player wears no Pip-Boy", returncode=2)
    return loadout


@pytest.fixture
def flashlight(game, loadout):
    """The mod, each test starting the same: no menu, no config UI, the BASELINE overrides alone, out of power armor,
    no weapon drawn, the allow-listed helmet on, and the light off on the head. A test sets the rest itself."""
    flashlight = loadout.flashlight
    close_game_menus(game)
    flashlight.ui("close")
    flashlight.clear()
    for key, section, value in BASELINE:
        flashlight.set(key, value, section)
    loadout.leave_power_armor()
    loadout.holster_weapon()
    if loadout.worn_headgear() != HELMET:
        loadout.wear_helmet()
    flashlight.light(location="head", op="off")
    yield flashlight
    flashlight.clear()
    flashlight.ui("close")
    close_game_menus(game)


@pytest.fixture
def weapon(flashlight, loadout):
    """The test weapon drawn (a ranged one without a modeled lamp), holstered after the test."""
    loadout.draw_weapon()
    yield
    loadout.holster_weapon()


@pytest.fixture
def events(game):
    """The events published from the start of the test."""
    return Events(game)
