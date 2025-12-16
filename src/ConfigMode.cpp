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

        disablePlayerInput(_flashlightValuesTglBtn->isToggleOn());

        handleValuesAdjustment();
    }

    void ConfigMode::handleValuesAdjustment() const
    {
        if (!_flashlightValuesTglBtn->isToggleOn()) {
            return;
        }

        const auto primaryDirection = vrcf::VRControllers.getThumbstickPressedDirection(vrcf::Hand::Primary, 0.8f, 0.5f);
        const auto offhandDirection = vrcf::VRControllers.getThumbstickPressedDirection(vrcf::Hand::Offhand, 0.8f, 0.5f);

        if (primaryDirection.has_value()) {
            switch (primaryDirection.value()) {
            case vrcf::Direction::Up:
                g_config.flashlightInHandFade += 0.1f;
                break;
            case vrcf::Direction::Down:
                g_config.flashlightInHandFade -= 0.1f;
                break;
            case vrcf::Direction::Right:
                g_config.flashlightInHandRadius += 200;
                break;
            case vrcf::Direction::Left:
                g_config.flashlightInHandRadius -= 200;
                break;
            }
            Utils::toggleLightsRefreshValues();
        }

        if (offhandDirection.has_value()) {
            if (offhandDirection.value() == vrcf::Direction::Up) {
                g_config.flashlightInHandFov += 5;
                Utils::toggleLightsRefreshValues();
            } else if (offhandDirection.value() == vrcf::Direction::Down) {
                g_config.flashlightInHandFov -= 5;
                Utils::toggleLightsRefreshValues();
            }
        }
    }

    void ConfigMode::switchFlashlightGobo() {}

    void ConfigMode::switchFlashlightColor() {}

    void ConfigMode::saveConfig() {}

    void ConfigMode::resetConfig() {}

    /**
     * If to disable player input to prevent movement while in config mode.
     */
    void ConfigMode::disablePlayerInput(const bool disable)
    {
        if (_disabledInput == disable) {
            return;
        }
        _disabledInput = disable;

        logger::info("Player controls - {}", _disabledInput ? "Disabled" : "Enabled");
        f4vr::SetActorRestrained(RE::PlayerCharacter::GetSingleton(), _disabledInput);
    }

    /**
     * Create all the main config UI elements.
     */
    void ConfigMode::createMainConfigUI()
    {
        const auto onHeadFLBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_flashlight.nif");
        onHeadFLBtn->setToggleState(g_config.flashlightLocation == FlashlightLocation::OnHead);
        onHeadFLBtn->setOnToggleHandler([this](UIWidget*, bool) { g_config.setFlashlightLocation(FlashlightLocation::OnHead); });

        const auto inHandFLBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_flashlight.nif");
        inHandFLBtn->setToggleState(g_config.flashlightLocation == FlashlightLocation::InOffhand);
        inHandFLBtn->setOnToggleHandler([this](UIWidget*, bool) { g_config.setFlashlightLocation(FlashlightLocation::InOffhand); });

        const auto onWeaponFLBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_flashlight.nif");
        onWeaponFLBtn->setToggleState(g_config.flashlightLocation == FlashlightLocation::InPrimaryHand);
        onWeaponFLBtn->setOnToggleHandler([this](UIWidget*, bool) { g_config.setFlashlightLocation(FlashlightLocation::InPrimaryHand); });

        const auto row1ToggleContainer = std::make_shared<UIToggleGroupContainer>("Row1", UIContainerLayout::HorizontalCenter, 0.3f);
        row1ToggleContainer->addElement(onHeadFLBtn);
        row1ToggleContainer->addElement(inHandFLBtn);
        row1ToggleContainer->addElement(onWeaponFLBtn);

        _flashlightValuesTglBtn = std::make_shared<UIToggleButton>("ImmersiveFlashlightVR\\ui_config_btn_flashlight.nif");
        _flashlightValuesTglBtn->setOnToggleHandler([this](UIWidget*, bool) {});

        const auto switchGoboBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\ui_config_btn_flashlight.nif");
        switchGoboBtn->setOnPressHandler([this](UIWidget*) { switchFlashlightGobo(); });

        const auto switchColorBtn = std::make_shared<UIButton>("ImmersiveFlashlightVR\\ui_config_btn_flashlight.nif");
        switchColorBtn->setOnPressHandler([this](UIWidget*) { switchFlashlightColor(); });

        const auto row2Container = std::make_shared<UIContainer>("Row2", UIContainerLayout::HorizontalCenter, 0.3f);
        row2Container->addElement(_flashlightValuesTglBtn);
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

        const auto configMsg = std::make_shared<UIWidget>("ImmersiveFlashlightVR\\msg_config.nif");

        const auto row4Container = std::make_shared<UIContainer>("Row4", UIContainerLayout::HorizontalCenter, 0.3f, 0.7f);
        row4Container->addElement(configMsg);

        const auto header = std::make_shared<UIWidget>("ImmersiveFlashlightVR\\title_config.nif", 0.4f);

        _configUI = std::make_shared<UIContainer>("Config", UIContainerLayout::VerticalUp, 0.4f, 1.8f);
        _configUI->addElement(row4Container);
        _configUI->addElement(row3Container);
        _configUI->addElement(row2Container);
        _configUI->addElement(row1ToggleContainer);
        _configUI->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_configUI, { 0, 0, 0 });
    }

    void ConfigMode::closeConfigMode()
    {
        disablePlayerInput(false);
        g_uiManager->detachElement(_configUI, true);
        _configUI.reset();
    }
}
