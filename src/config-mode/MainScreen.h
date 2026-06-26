#pragma once

#include <memory>

#include "BeamScreen.h"
#include "MiscScreen.h"
#include "vrui/UIContainer.h"

namespace ImFl::config
{
    /**
     * Top-level config screen and coordinator for the in-game VR config UI. Shows the main menu (open
     * the beam / misc config screens, open the INI for advanced editing, open the wiki, exit) and owns
     * the BeamScreen / MiscScreen, switching between them. FlashlightMod drives this one object: open it
     * from the FRIK menu, close it on session load, and frame-update it each frame.
     *
     * Screen switches requested from a button handler are deferred to onFrameUpdate (via _pending): the
     * handler fires while the UI manager is iterating its elements, and attaching/detaching a screen then
     * would invalidate that loop.
     */
    class MainScreen
    {
    public:
        MainScreen();

        bool isConfigOpen() const;
        void openConfigMode();
        void closeConfigMode();
        void onFrameUpdate();

    private:
        // The shown / requested screen. _active is never Exit; _pending == None means no request.
        enum class Nav : uint8_t
        {
            None,
            Menu,
            Beam,
            Misc,
            Exit
        };

        void openMenu();
        void closeActiveScreen();
        void processPendingNavigation();
        void createMenuUI();
        static void openIniFileForAdvancedEditing();
        static void openWiki();

        std::shared_ptr<vrui::UIContainer> _menuUI;
        BeamScreen _beam;
        MiscScreen _misc;

        Nav _active = Nav::None;
        Nav _pending = Nav::None;
    };
}
