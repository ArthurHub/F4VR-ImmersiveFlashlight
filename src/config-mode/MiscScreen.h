#pragma once

#include <functional>
#include <memory>

#include "Config.h"
#include "vrui/UIContainer.h"
#include "vrui/UIToggleButton.h"

namespace ImFl::config
{
    /**
     * Misc config screen: a collection of global toggle settings (beam shadows, debug spheres, stowed
     * body model, headgear requirement, weapon-flashlight requirement, vanilla-toggle disable, more to
     * come). Reached from the main config screen; a Back button returns to it. Each toggle persists to the
     * INI immediately and applies live.
     */
    class MiscScreen
    {
    public:
        bool isOpen() const;
        void open();
        void close();
        void onFrameUpdate() const;

        void setOnBackHandler(std::function<void()> handler);

    private:
        void createUI();
        static void toggleShadows(bool shadowsEnabled);
        static void toggleDebugSpheres(bool enabled);
        static void toggleShowOnBody(bool enabled);
        static void onHeadgearRequirementChanged(FlashlightHeadgearRequirement requirement);
        static void toggleWeaponFlashlightRequired(bool required);
        static void toggleDisableVanillaToggle(bool enabled);

        std::shared_ptr<vrui::UIContainer> _ui;
        std::function<void()> _onBack;
    };
}
