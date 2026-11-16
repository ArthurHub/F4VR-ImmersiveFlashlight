"""The session the end-to-end tests run in. devbench's pytest plugin enters the game in QASmoke, checks the mod's build,
backs up its config, sets the handedness, checks FRIK, waits for the flashlight and writes the report; this conftest
adds the mod following the handedness, the player's loadout and a light reset before each test."""

import re
from pathlib import Path

import pytest

from devbench import wait_until
from devbench.testing import Area, Config, Dependency
from flashlight import FRIK_TOOL, HELMET, MOD_TOOL, Flashlight, Loadout, close_game_menus

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
    # others, and health last, its log check covering the whole run. Each file's tests carry its marker (-m <marker>)
    areas={
        "test_light.py": Area("Light action", "light"),
        "test_gestures.py": Area("Gestures (simulated)", "gestures"),
        "test_overrides.py": Area("Config overrides", "overrides"),
        "test_headgear.py": Area("Headgear restriction", "headgear"),
        "test_menus.py": Area("Menus", "menus"),
        "test_weapon.py": Area("Weapon mount", "weapon"),
        "test_controllers.py": Area("Gestures (controllers)", "controllers"),
        "test_power_armor.py": Area("Power armor", "powerarmor"),
        "test_health.py": Area("Health", "health"),
    },
    # FRIK's devbench probe sets the two-handed grip the weapon and controller tests need (its grip op: FRIK 0.79)
    requires=[Dependency(FRIK_TOOL, actions=["probe"], min_version="0.79")],
    # the flashlight up (the player's 3D) before the first test
    ready=lambda mod: mod.call("state")["liveness"]["ready"],
    title="Immersive Flashlight VR",
    # the lit frame, dumped when a test fails
    picture=6,
    # right-handed unless --left-handed; the player's handedness is put back after
    handedness="right",
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
    parser.addoption("--power-armor-radius", type=float, default=2000.0, help="how far to look for a power armor frame, in game units")


@pytest.fixture(scope="session")
def game(game, left_handed):
    """The plugin's game (in the run's handedness, which the plugin set and puts back, with the flashlight up and FRIK
    checked), with the mod following the handedness."""
    flashlight = Flashlight(game)
    wait_until(lambda: flashlight.left_handed() == left_handed, 5, what=f"the mod reading the game {'left' if left_handed else 'right'}-handed")
    yield game
    flashlight.clear()
    flashlight.two_handed_grip(on=False)


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
