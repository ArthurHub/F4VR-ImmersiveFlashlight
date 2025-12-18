#include "ConfigMode.h"

#include "Config.h"
#include "Utils.h"
#include "api/FRIKApi.h"
#include "f4vr/PlayerNodes.h"
#include "vrcf/VRControllersManager.h"
#include "vrui/UIButton.h"
#include "vrui/UIManager.h"
#include "vrui/UIMultiStateToggleButton.h"
#include "vrui/UIToggleGroupContainer.h"

using namespace vrui;
using namespace common;

namespace
{
    constexpr std::array<std::array<int, 3>, 9> COLOR_OPTIONS = { {
        { 235, 224, 190 }, // Warm Light a bit yellowish
        { 240, 230, 225 }, // Mostly White
        { 255, 255, 255 }, // Full White
        { 255, 200, 150 }, // Warm White
        { 235, 224, 203 }, // Soft Warm White
        { 239, 104, 65 }, // Red
        { 148, 222, 165 }, // Green
        { 87, 155, 217 }, // Blue
        { 101, 83, 202 }, // Purple
    } };

    int findCurrentColorIndex()
    {
        const int r = *ImFl::g_config.flashlightColorRed;
        const int g = *ImFl::g_config.flashlightColorGreen;
        const int b = *ImFl::g_config.flashlightColorBlue;
        for (std::size_t i = 0; i < COLOR_OPTIONS.size(); ++i) {
            const auto& c = COLOR_OPTIONS[i];
            if (c[0] == r && c[1] == g && c[2] == b) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
}

namespace ImFl
{
    int ConfigMode::isOpen() const
    {
        return _configUI != nullptr;
    }

    void ConfigMode::openConfigMode()
    {
        logger::info("Open config by call...");
        createMainConfigUI();

        // turn flashlight on if it's off
        auto player = f4vr::getPlayer();
        if (!f4vr::isPipboyLightOn(player)) {
            f4vr::togglePipboyLight(player);
        }
    }

    /**
     * Handle main config on every frame update.
     */
    void ConfigMode::onFrameUpdate()
    {
        if (!isOpen()) {
            // TODO: remove after testing
            if (vrcf::VRControllers.isLongPressed(vrcf::Hand::Offhand, vr::k_EButton_A)) {
                openConfigMode();
            }

            return;
        }

        // close this mod config mode if FRIK config is opened
        if (frik::api::FRIKApi::inst->isConfigOpen()) {
            closeConfigMode();
        }

        _configMsg->setVisibility(!_beamTuningTglBtn->isToggleOn());
        _beamTuningMsg->setVisibility(_beamTuningTglBtn->isToggleOn());

        disablePlayerInput(_beamTuningTglBtn->isToggleOn());

        setFlashlightButtonsToggleStateByLocation();

        handleBeamTuningAdjustments();

        showBeamCurrentValuesNotification();
    }

    /**
     * Adjust the beam (Fade, Radius, FOV) values based on thumbstick input.
     * Fade - the intensity of the beam - primary thumbstick up/down
     * Radius - the distance the beam reaches - primary thumbstick left/right
     * FOV - the spread of the beam - offhand thumbstick up/down
     */
    void ConfigMode::handleBeamTuningAdjustments()
    {
        if (!_beamTuningTglBtn->isToggleOn()) {
            return;
        }

        const auto primaryDirection = vrcf::VRControllers.getThumbstickPressedDirection(vrcf::Hand::Primary, 0.8f, 0.5f);
        const auto offhandDirection = vrcf::VRControllers.getThumbstickPressedDirection(vrcf::Hand::Offhand, 0.8f, 0.5f);

        if (primaryDirection.has_value()) {
            switch (primaryDirection.value()) {
            case vrcf::Direction::Up:
                *g_config.flashlightFade = fminf(*g_config.flashlightFade + 0.1f, 4.0f);
                break;
            case vrcf::Direction::Down:
                *g_config.flashlightFade = fmaxf(*g_config.flashlightFade - 0.1f, 0.2f);
                break;
            case vrcf::Direction::Right:
                *g_config.flashlightRadius = min(*g_config.flashlightRadius + 200, 10000);
                break;
            case vrcf::Direction::Left:
                *g_config.flashlightRadius = max(*g_config.flashlightRadius - 200, 1000);
                break;
            }
            Utils::toggleLightRefreshValues();
            _lastValuesChangeNotificationPensing = true;
        }

        if (offhandDirection.has_value()) {
            if (offhandDirection.value() == vrcf::Direction::Up) {
                *g_config.flashlightFov = fminf(*g_config.flashlightFov + 5, 150);
                Utils::toggleLightRefreshValues();
                _lastValuesChangeNotificationPensing = true;
            } else if (offhandDirection.value() == vrcf::Direction::Down) {
                *g_config.flashlightFov = fmaxf(*g_config.flashlightFov - 5, 5);
                Utils::toggleLightRefreshValues();
                _lastValuesChangeNotificationPensing = true;
            }
        }
    }

    /**
     * Show notification with current flashlight values after they were changed.
     * Try not to spam too much.
     */
    void ConfigMode::showBeamCurrentValuesNotification()
    {
        const auto now = nowMillis();
        if (_lastValuesChangeNotificationPensing && now - _lastValuesUpdateNotificationTime > 3000) {
            _lastValuesChangeNotificationPensing = false;
            _lastValuesUpdateNotificationTime = now;
            f4vr::showNotification(std::format("Beam values updated:\nIntensity = {:.1f}\nDistance = {}\nSpread = {:.0f}\xC2\xB0",
                *g_config.flashlightFade, *g_config.flashlightRadius, *g_config.flashlightFov));
        }
    }

    void ConfigMode::switchBeamGobo() {}

    /**
     * Switch the beam color to the next preset option.
     */
    void ConfigMode::switchBeamColor()
    {
        const int nextColorIndex = (findCurrentColorIndex() + 1) % static_cast<int>(COLOR_OPTIONS.size());
        *g_config.flashlightColorRed = COLOR_OPTIONS[nextColorIndex][0];
        *g_config.flashlightColorGreen = COLOR_OPTIONS[nextColorIndex][1];
        *g_config.flashlightColorBlue = COLOR_OPTIONS[nextColorIndex][2];

        Utils::toggleLightRefreshValues();

        f4vr::showNotification(std::format("Beam color updated (Preset: {} out of {}):\nRed = {}\nGreen = {}\nBlue = {}",
            nextColorIndex, COLOR_OPTIONS.size(), *g_config.flashlightColorRed, *g_config.flashlightColorGreen, *g_config.flashlightColorBlue));
    }

    /**
     * Save the flashlight values only for the current selected location.
     */
    void ConfigMode::saveConfig()
    {
        f4vr::showNotification(std::format("{} flashlight beam values saved",
            g_config.flashlightLocation == FlashlightLocation::OnHead ? "On Head" : g_config.flashlightLocation == FlashlightLocation::InOffhand ? "In Hand" : "On Weapon"));
        g_config.saveFlashlightValues();
    }

    /**
     * Reset to default the flashlight values only for the current selected location.
     */
    void ConfigMode::resetConfig()
    {
        f4vr::showNotification(std::format("{} flashlight beam values reset to default",
            g_config.flashlightLocation == FlashlightLocation::OnHead ? "On Head" : g_config.flashlightLocation == FlashlightLocation::InOffhand ? "In Hand" : "On Weapon"));
        g_config.resetFlashlightValuesToDefault();
        Utils::toggleLightRefreshValues();
    }

    /**
     * If to disable player input to prevent movement while in config mode.
     */
    void ConfigMode::disablePlayerInput(const bool disable)
    {
        if (disable) {
            if (!_inputDisabled) {
                _inputDisabled = true;
                logger::info("Player controls - Disabled");
            }
            // always disable in case other code enabled user input (pipboy use, holsters, etc.)
            f4vr::SetActorRestrained(RE::PlayerCharacter::GetSingleton(), true);
        } else if (_inputDisabled) {
            logger::info("Player controls - Enabled");
            _inputDisabled = false;
            f4vr::SetActorRestrained(RE::PlayerCharacter::GetSingleton(), false);
        }
    }

    void ConfigMode::setFlashlightButtonsToggleStateByLocation() const
    {
        switch (g_config.flashlightLocation) {
        case FlashlightLocation::OnHead:
            _onHeadFLBtn->setToggleState(true);
            break;
        case FlashlightLocation::InOffhand:
            _inHandFLBtn->setToggleState(true);
            break;
        case FlashlightLocation::InPrimaryHand:
            _onWeaponFLBtn->setToggleState(true);
            break;
        }
    }

    /**
     * Create all the main config UI elements.
     */
    void ConfigMode::createMainConfigUI()
    {
        _onHeadFLBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_fl_on_head_1x2.nif");
        _onHeadFLBtn->setOnToggleHandler([this](UIWidget*, bool) { Utils::switchFlashlightLocation(FlashlightLocation::OnHead); });

        _inHandFLBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_fl_in_hand_1x3.nif");
        _inHandFLBtn->setOnToggleHandler([this](UIWidget*, bool) { Utils::switchFlashlightLocation(FlashlightLocation::InOffhand); });

        _onWeaponFLBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_fl_on_weapon_1x4.nif");
        _onWeaponFLBtn->setOnToggleHandler([this](UIWidget*, bool) { Utils::switchFlashlightLocation(FlashlightLocation::InPrimaryHand); });

        const auto row1ToggleContainer = std::make_shared<UIToggleGroupContainer>("Row1", UIContainerLayout::HorizontalCenter, 0.3f);
        row1ToggleContainer->addElement(_onHeadFLBtn);
        row1ToggleContainer->addElement(_inHandFLBtn);
        row1ToggleContainer->addElement(_onWeaponFLBtn);
        setFlashlightButtonsToggleStateByLocation();

        _beamTuningTglBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_beam_tuning_2x2.nif");
        _beamTuningTglBtn->setOnToggleHandler([this](UIWidget*, bool) {});

        const auto switchGoboBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\ui_config_btn_switch_gobo_2x1.nif");
        switchGoboBtn->setOnPressHandler([this](UIWidget*) { switchBeamGobo(); });

        const auto switchColorBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\ui_config_btn_switch_color_1x5.nif");
        switchColorBtn->setOnPressHandler([this](UIWidget*) { switchBeamColor(); });

        const auto row2Container = std::make_shared<UIContainer>("Row2", UIContainerLayout::HorizontalCenter, 0.3f);
        row2Container->addElement(_beamTuningTglBtn);
        row2Container->addElement(switchGoboBtn);
        row2Container->addElement(switchColorBtn);

        const auto saveBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\UI_Common\\btn_save.nif");
        saveBtn->setOnPressHandler([this](UIWidget*) { saveConfig(); });

        const auto resetBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\UI_Common\\btn_reset.nif");
        resetBtn->setOnPressHandler([this](UIWidget*) { resetConfig(); });

        const auto exitBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\UI_Common\\btn_exit.nif");
        exitBtn->setOnPressHandler([this](UIWidget*) { closeConfigMode(); });

        const auto row3Container = std::make_shared<UIContainer>("Row3", UIContainerLayout::HorizontalCenter, 0.3f);
        row3Container->addElement(saveBtn);
        row3Container->addElement(resetBtn);
        row3Container->addElement(exitBtn);

        _configMsg = std::make_shared<UIWidget>("ImmersiveFlashlightVR\\ui_config_msg_2x1.nif");
        _beamTuningMsg = std::make_shared<UIWidget>("ImmersiveFlashlightVR\\ui_config_smg_beam_tuning_5x1.nif");

        const auto row4Container = std::make_shared<UIContainer>("Row4", UIContainerLayout::HorizontalCenter, 0.3f, 0.7f);
        row4Container->addElement(_configMsg);
        row4Container->addElement(_beamTuningMsg);

        const auto header = std::make_shared<UIWidget>("ImmersiveFlashlightVR\\title_config.nif", 0.4f);

        _configUI = std::make_shared<UIContainer>("Config", UIContainerLayout::VerticalUp, 0.4f, 1.8f);
        _configUI->addElement(row4Container);
        _configUI->addElement(row3Container);
        _configUI->addElement(row2Container);
        _configUI->addElement(row1ToggleContainer);
        _configUI->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_configUI, { 0, 0, 0 });
    }

    /**
     * Close.
     */
    void ConfigMode::closeConfigMode()
    {
        // reload config to discard unsaved changes
        g_config.load();

        // unblock player input if needed
        disablePlayerInput(false);

        // release the UI
        g_uiManager->detachElement(_configUI, true);
        _configUI.reset();
        _beamTuningTglBtn.reset();
        _onHeadFLBtn.reset();
        _inHandFLBtn.reset();
        _onWeaponFLBtn.reset();
        _configMsg.reset();
        _beamTuningMsg.reset();
    }
}
