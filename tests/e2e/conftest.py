"""The session the end-to-end tests run in. devbench's pytest plugin enters the game in QASmoke, checks the mod's build,
backs up its config and writes the report; this conftest adds the mod's own setup (FRIK, the handedness), the player's
loadout and a light reset before each test."""

import re
from pathlib import Path

import pytest

from devbench import DevbenchError, wait_until
from devbench.testing import Config
from flashlight import FRIK_TOOL, HELMET, MOD_TOOL, Flashlight, Loadout, close_game_menus, set_left_handed

pytest_plugins = ["devbench.testing"]

ROOT = Path(__file__).resolve().parents[2]


def checkout_version():
    """The mod's version in this checkout's CMakeLists.txt, which the deployed build must report."""
    return re.search(r"^set\(VERSION ([^)]+)\)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), re.MULTILINE).group(1)


DEVBENCH = Config(
    game="fallout4",
    vr=True,
    tool=MOD_TOOL,
    # the actions the tests call that an older build of the mod lacks
    actions=["gesture", "zones", "light", "restrictions", "ui"],
    version=checkout_version,
    # the mod's folder in My Games\Fallout4VR\Mods_Config: its TOML, and the backups an iVersion bump writes. A location
    # switch (a gesture, the light action) saves to the file, so the run would otherwise leave the player's light where
    # the last test put it; the mod hot-reloads the file once it is put back
    backup=[("mygames", "Mods_Config/ImmersiveFlashlightVR/*")],
    log="ImmersiveFlashlightVR.log",
    # the areas, by the file of their tests, in the order they run and the page shows them: power armor after the
    # others, and health last, its log check covering the whole run
    areas={
        "test_light.py": "Light action",
        "test_gestures.py": "Gestures (simulated)",
        "test_overrides.py": "Config overrides",
        "test_headgear.py": "Headgear restriction",
        "test_menus.py": "Menus",
        "test_weapon.py": "Weapon mount",
        "test_controllers.py": "Gestures (controllers)",
        "test_power_armor.py": "Power armor",
        "test_health.py": "Health",
    },
    title="Immersive Flashlight VR",
    # the lit frame, dumped when a test fails
    picture=6,
)

# the settings every test starts with, as session overrides, so a result doesn't depend on the player's config file:
# (key, section, value); a test overrides one of them again to try another value
BASELINE = [
    ("bRestrictionFallback", None, "true"),
    ("iFlashlightHeadgearRequirement", "ImFl_HeadgearRestriction", "2"),
    ("iWeaponFlashlightMeshRequired", "ImFl_WeaponMount", "0"),
    ("bShowFlashlightOnBody", "ImFl_BodyStow", "true"),
    # the binding icons the controller tests read, as shipped
    ("sShowBindingIcon", "ImFl_PrimaryHandActivationSphere", "wheninside"),
    ("sBindingIcons", "ImFl_PrimaryHandActivationSphere", "all"),
    ("sShowBindingIcon", "ImFl_BodyActivationSphere", "wheninside"),
    ("sBindingIcons", "ImFl_BodyActivationSphere", "inside"),
]


def pytest_addoption(parser):
    parser.addoption("--left-handed", action="store_true", help="run left-handed: the game's handedness set for the run, and put back after")
    parser.addoption("--power-armor-radius", type=float, default=2000.0, help="how far to look for a power armor frame, in game units")


def _check_frik(game, report):
    """FRIK answers its devbench probe's grip, which the two-handed tests set the grip through (FRIK 0.79 or later)."""
    try:
        version = game.tool(FRIK_TOOL, action="health")["version"]
        Flashlight(game).two_handed_grip(on=False)
    except DevbenchError as error:
        pytest.exit(f"FRIK's devbench probe doesn't set a grip ({error}): the two-handed tests need FRIK 0.79 or later", returncode=2)
    report.note(f"FRIK {version}.")


@pytest.fixture(scope="session")
def game(game, pytestconfig, report):
    """The plugin's game, with the flashlight up, FRIK checked, and the run right-handed or with --left-handed. The
    handedness is put back after the run, before the plugin puts the config backup back."""
    wait_until(lambda: Flashlight(game).state()["liveness"]["ready"], 30, interval=0.5, what="the flashlight up (the player's 3D)")
    _check_frik(game, report)
    left_handed = pytestconfig.getoption("--left-handed")
    was_left_handed = set_left_handed(game, left_handed)
    report.note("Ran left-handed." if left_handed else "Ran right-handed.")
    yield game
    try:
        Flashlight(game).clear()
        Flashlight(game).two_handed_grip(on=False)
    finally:
        set_left_handed(game, was_left_handed)


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
    """The mod, each test starting the same: no menu, no config UI, the BASELINE overrides alone, no two-handed grip set
    through FRIK, out of power armor, no weapon drawn, the allow-listed helmet on, and the light off on the head. A test
    sets the rest itself."""
    flashlight = loadout.flashlight
    close_game_menus(game)
    flashlight.ui("close")
    flashlight.clear()
    flashlight.two_handed_grip(on=False)
    for key, section, value in BASELINE:
        flashlight.set(key, value, section)
    loadout.leave_power_armor()
    loadout.holster_weapon()
    if loadout.worn_headgear() != HELMET:
        loadout.wear_helmet()
    flashlight.light(location="head", op="off")
    yield flashlight
    flashlight.clear()
    flashlight.two_handed_grip(on=False)
    flashlight.ui("close")
    close_game_menus(game)


@pytest.fixture
def weapon(flashlight, loadout):
    """The test weapon drawn (a ranged one without a modeled lamp), holstered after the test."""
    loadout.draw_weapon()
    yield
    loadout.holster_weapon()
