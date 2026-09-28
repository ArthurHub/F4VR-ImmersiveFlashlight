#pragma once

namespace ImFl::devbench
{
    /**
     * Set up the mod's devbench tool on the framework's f4cf::devbench: its description, the state behind the state
     * action (and the transition events it emits), and the mod's actions (light, ui, restrictions). Call once while the mod
     * loads, before onGameLoaded returns, so the tool is registered with all of it.
     */
    void setupDevBenchTool();
}
