#include "MainScreen.h"

#include "Config.h"
#include "FlashlightState.h"
#include "Utils.h"
#include "api/FRIKApi.h"
#include "f4vr/PlayerNodes.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIManager.h"
#include "vrui/UITextPanel.h"

// ShellExecuteA is already declared transitively via the CommonLibF4/Windows headers; we only need the
// import lib to link it. (Directly #include-ing <shellapi.h> here breaks against the F4SE Windows setup.)
#pragma comment(lib, "Shell32.lib")

namespace
{
    // SW_SHOWNORMAL, declared locally so we don't depend on the winuser.h macro being visible here.
    constexpr int SHELL_SW_SHOWNORMAL = 1;
}

using namespace vrui;

namespace ImFl::config
{
    MainScreen::MainScreen()
    {
        _beam.setOnBackHandler([this] { _pending = Nav::Menu; });
        _misc.setOnBackHandler([this] { _pending = Nav::Menu; });
    }

    bool MainScreen::isConfigOpen() const
    {
        return _active != Nav::None;
    }

    /**
     * Open the config UI on the main menu, turning the light on so its beam is visible while configuring.
     */
    void MainScreen::openConfigMode()
    {
        if (isConfigOpen()) {
            return;
        }
        logger::info("Open config menu by call...");
        FlashlightState::refreshFlashlightLocation();
        openMenu();
        Utils::turnFlashlightOn();
    }

    /**
     * Fully close the config UI, closing whichever screen is currently active.
     */
    void MainScreen::closeConfigMode()
    {
        _pending = Nav::None;
        closeActiveScreen();
    }

    /**
     * Drive the active screen each frame. Auto-closes when FRIK config opens, then applies any pending
     * screen switch before updating the now-active screen.
     */
    void MainScreen::onFrameUpdate()
    {
        if (!isConfigOpen() && _pending == Nav::None) {
            return;
        }

        // close this mod config mode if FRIK config is opened
        if (frik::api::FRIKApi::inst->isConfigOpen()) {
            closeConfigMode();
            return;
        }

        processPendingNavigation();

        switch (_active) {
        case Nav::Menu:
            if (_menuUI) {
                _menuUI->setPosition(0, 0, f4vr::isNodeVisible(f4vr::getWeaponNode()) ? 6.0f : 0.0f);
            }
            break;
        case Nav::Beam:
            _beam.onFrameUpdate();
            break;
        case Nav::Misc:
            _misc.onFrameUpdate();
            break;
        case Nav::None:
        case Nav::Exit:
        default:
            break;
        }
    }

    void MainScreen::openMenu()
    {
        createMenuUI();
        _active = Nav::Menu;
    }

    /**
     * Close whichever screen is currently active and mark none active.
     */
    void MainScreen::closeActiveScreen()
    {
        switch (_active) {
        case Nav::Menu:
            if (_menuUI) {
                g_uiManager->detachElement(_menuUI, true);
                _menuUI.reset();
            }
            break;
        case Nav::Beam:
            _beam.close();
            break;
        case Nav::Misc:
            _misc.close();
            break;
        case Nav::None:
        case Nav::Exit:
        default:
            break;
        }
        _active = Nav::None;
    }

    /**
     * Apply a deferred screen switch: close the active screen, then open the requested one. Run from
     * onFrameUpdate, after the UI manager finished iterating its elements (safe to attach/detach).
     */
    void MainScreen::processPendingNavigation()
    {
        if (_pending == Nav::None) {
            return;
        }
        const Nav target = _pending;
        _pending = Nav::None;

        closeActiveScreen();

        switch (target) {
        case Nav::Menu:
            openMenu();
            break;
        case Nav::Beam:
            _beam.open();
            _active = Nav::Beam;
            break;
        case Nav::Misc:
            _misc.open();
            _active = Nav::Misc;
            break;
        case Nav::Exit:
        case Nav::None:
            break;
        }
    }

    /**
     * Open the config INI in the default editor for advanced hand-editing; the config hot-reload applies
     * saved changes live.
     */
    void MainScreen::openIniFileForAdvancedEditing()
    {
        logger::info("Open config INI for advanced editing: {}", INI_PATH);
        ShellExecuteA(nullptr, "open", "notepad.exe", INI_PATH.c_str(), nullptr, SHELL_SW_SHOWNORMAL);
        f4vr::showNotification("Config INI opened in Notepad on your PC.\nSwitch to your monitor to edit advanced settings.\nSaved changes apply live.");
    }

    /**
     * Open the mod wiki in the default web browser.
     */
    void MainScreen::openWiki()
    {
        logger::info("Open wiki");
        ShellExecuteA(nullptr, "open", "explorer.exe", "https://github.com/ArthurHub/F4VR-ImmersiveFlashlight/blob/main/docs/README.md", nullptr, SHELL_SW_SHOWNORMAL);
        f4vr::showNotification("Help wiki opened in your browser.\nSwitch to your monitor to read it.");
    }

    /**
     * Create all the main menu UI elements.
     */
    void MainScreen::createMenuUI()
    {
        // open beam config reuses the same icon FRIK shows to open this config
        const auto beamConfigBtn = std::make_shared<UIButtonPanel>("ImFl_BeamConfigButton");
        beamConfigBtn->setTopText("TUNING");
        beamConfigBtn->setImage("vrui\\per-location-config.DDS");
        beamConfigBtn->setBottomText("PER LOCATION");
        beamConfigBtn->setOnPressHandler([this](UIButtonPanel*) { _pending = Nav::Beam; });

        const auto miscConfigBtn = std::make_shared<UIButtonPanel>("ImFl_MiscConfigButton");
        miscConfigBtn->setTopText("MISC");
        miscConfigBtn->setImage("vrui\\misc-config.DDS");
        miscConfigBtn->setBottomText("CONFIG");
        miscConfigBtn->setOnPressHandler([this](UIButtonPanel*) { _pending = Nav::Misc; });

        const auto row1 = std::make_shared<UIContainer>("MainNav", UIContainerLayout::HorizontalCenter, 0.3f);
        row1->addElement(beamConfigBtn);
        row1->addElement(miscConfigBtn);

        const auto advancedConfigBtn = std::make_shared<UIButtonPanel>("ImFl_AdvancedConfigButton");
        advancedConfigBtn->setTopText("OPEN");
        advancedConfigBtn->setImage("vrui\\advanced-config.DDS");
        advancedConfigBtn->setBottomText("INI CONFIG");
        advancedConfigBtn->setOnPressHandler([](UIButtonPanel*) { openIniFileForAdvancedEditing(); });

        const auto wikiBtn = std::make_shared<UIButtonPanel>("ImFl_HelpWikiButton");
        wikiBtn->setTopText("OPEN");
        wikiBtn->setImage("vrui\\help-wiki.DDS");
        wikiBtn->setBottomText("HELP WIKI");
        wikiBtn->setOnPressHandler([](UIButtonPanel*) { openWiki(); });

        const auto exitBtn = std::make_shared<UIButtonPanel>("ImFl_ExitButton");
        exitBtn->setImage("vrui\\exit.DDS");
        exitBtn->setBottomText("EXIT");
        exitBtn->setOnPressHandler([this](UIButtonPanel*) { _pending = Nav::Exit; });

        const auto row2 = std::make_shared<UIContainer>("MainExit", UIContainerLayout::HorizontalCenter, 0.3f);
        row2->addElement(advancedConfigBtn);
        row2->addElement(wikiBtn);
        row2->addElement(exitBtn);

        const auto mainMsg = std::make_shared<UITextPanel>("ImFl_MainFooter");
        mainMsg->setStyle(F4VR_PANEL_STYLE);
        mainMsg->setTextHeight(0.2f);
        mainMsg->setContent([](std::vector<TextRow>& rows) {
            rows.emplace_back("TURN ON/OFF BY OFFHAND TRIGGER TAP");
            rows.emplace_back("IN EACH ACTIVATION LOCATION:");
            rows.emplace_back("LEFT HIP / FOREHEAD / WEAPON");
        });

        const auto row3 = std::make_shared<UIContainer>("Row3", UIContainerLayout::HorizontalCenter, 0.3f);
        row3->addElement(mainMsg);

        const auto header = std::make_shared<UITextPanel>("ImFl_Title");
        header->setStyle(UIPanelStyle{ .color = F4VR_PANEL_STYLE.color });
        header->setTextHeight(0.5f);
        header->setContent([](std::vector<TextRow>& rows) { rows.emplace_back("IMMERSIVE FLASHLIGHT: CONFIG", std::nullopt, f4cf::render::TextDecoration::Underline); });

        _menuUI = std::make_shared<UIContainer>("MainConfig", UIContainerLayout::VerticalUp, 0.35f, 1.6f);
        _menuUI->addElement(row3);
        _menuUI->addElement(row2);
        _menuUI->addElement(row1);
        _menuUI->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_menuUI, { 0, 0, 0 });
    }
}
