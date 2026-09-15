#include "BeamScreen.h"

#include "Config.h"
#include "FlashlightState.h"
#include "Utils.h"
#include "f4vr/PlayerNodes.h"
#include "vrcf/VRControllersManager.h"
#include "vrcf/VRControllersSuppressor.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIManager.h"
#include "vrui/UITextPanel.h"
#include "vrui/UIToggleButtonPanel.h"
#include "vrui/UIToggleGroupContainer.h"

using namespace vrui;
using namespace common;

namespace
{
    const char* CONTROLLERS_SUPRESS_KEY = "ImFl_BeamConfig";

    struct ColorOption
    {
        std::array<int, 3> rgb;
        std::string_view name;
    };

    constexpr std::array<ColorOption, 8> COLOR_OPTIONS{ {
        { .rgb = { 255, 255, 255 }, .name = "Full White" },
        { .rgb = { 240, 230, 225 }, .name = "Mostly White" },
        { .rgb = { 235, 224, 190 }, .name = "Warm White" },
        { .rgb = { 255, 200, 150 }, .name = "Warm Dim White (yellowish)" },
        { .rgb = { 255, 210, 210 }, .name = "Red" },
        { .rgb = { 210, 255, 210 }, .name = "Green" },
        { .rgb = { 190, 190, 255 }, .name = "Blue" },
        { .rgb = { 190, 100, 255 }, .name = "Purple" },
    } };

    int findCurrentColorIndex()
    {
        const int r = *ImFl::FlashlightState::flashlightColorRed;
        const int g = *ImFl::FlashlightState::flashlightColorGreen;
        const int b = *ImFl::FlashlightState::flashlightColorBlue;
        for (std::size_t i = 0; i < COLOR_OPTIONS.size(); ++i) {
            const auto& c = COLOR_OPTIONS[i].rgb;
            if (c[0] == r && c[1] == g && c[2] == b) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::vector<std::string> goboTextureFilePaths;

    std::string_view getFlashlightLocationLabel(const ImFl::FlashlightLocation location)
    {
        switch (location) {
        case ImFl::FlashlightLocation::OnHead:
            return "On Head";
        case ImFl::FlashlightLocation::OnPAHead:
            return "On PA Head";
        case ImFl::FlashlightLocation::OnWeapon:
            return "On Weapon";
        case ImFl::FlashlightLocation::InOffhand:
        case ImFl::FlashlightLocation::InPrimaryHand:
            return "In Hand";
        }

        return "Unknown";
    }

    void setConfigModeFlashlightLocation(const ImFl::FlashlightLocation location)
    {
        ImFl::FlashlightState::setFlashlightRuntimeLocationOverride(location);
        ImFl::Utils::turnFlashlightOn();
    }

    void loadGoboTextureFiles()
    {
        const fs::path pathBase{ R"(data\Textures\ImmersiveFlashlightVR\Gobos)" };
        goboTextureFilePaths.clear();

        try {
            if (!fs::exists(pathBase) || !fs::is_directory(pathBase)) {
                logger::warn("Gobo texture directory not found: {}", pathBase.string());
            } else {
                for (const auto& entry : fs::directory_iterator(pathBase)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }

                    auto extension = entry.path().extension().string();
                    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                    if (extension != ".dds") {
                        continue;
                    }

                    const auto fullPath = pathBase / entry.path().filename().string();
                    goboTextureFilePaths.emplace_back(fullPath.string());
                }
            }
        } catch (const fs::filesystem_error& e) {
            logger::warn("Failed to enumerate gobo texture files in {}: {}", pathBase.string(), e.what());
        }

        if (goboTextureFilePaths.empty()) {
            logger::warn("No gobo texture files found.");
            goboTextureFilePaths.emplace_back(R"(data\Textures\Effects\Gobos\FlashlightGobo01.DDS)");
        } else {
            logger::info("Loaded {} gobo texture files", goboTextureFilePaths.size());
        }
    }

    int findCurrentGoboPathIndex()
    {
        if (goboTextureFilePaths.empty()) {
            loadGoboTextureFiles();
        }
        for (std::size_t i = 0; i < goboTextureFilePaths.size(); ++i) {
            if (goboTextureFilePaths[i] == *ImFl::FlashlightState::flashlightGoboPath) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
}

namespace ImFl::config
{
    bool BeamScreen::isOpen() const
    {
        return _ui != nullptr;
    }

    void BeamScreen::setOnBackHandler(std::function<void()> handler)
    {
        _onBack = std::move(handler);
    }

    /**
     * Resolve the runtime location, build the UI (which sets the location preview override), and turn the
     * light on so the tuned beam is visible.
     */
    void BeamScreen::open()
    {
        if (isOpen()) {
            return;
        }
        logger::info("Open beam config screen...");
        FlashlightState::refreshFlashlightLocation();
        createUI();
        Utils::turnFlashlightOn();
    }

    /**
     * Discard unsaved beam changes (reload config), clear the preview override, release controller
     * suppression, and detach the UI.
     */
    void BeamScreen::close()
    {
        if (!isOpen()) {
            return;
        }

        // reload config to discard unsaved changes
        FlashlightState::setFlashlightRuntimeLocationOverride(std::nullopt);
        g_config.load();
        if (Utils::isFlashlightOn()) {
            FlashlightState::toggleLightRefreshValues();
        }

        // unblock player input if needed
        vrcf::VRControllersSuppress.release(CONTROLLERS_SUPRESS_KEY);

        // release the UI
        g_uiManager->detachElement(_ui, true);
        _ui.reset();
        _beamTuningTglBtn.reset();
        _onHeadFLBtn.reset();
        _onPAHeadFLBtn.reset();
        _inHandFLBtn.reset();
        _onWeaponFLBtn.reset();
        _row1ToggleContainer.reset();
    }

    /**
     * Handle the beam config screen on every frame update.
     */
    void BeamScreen::onFrameUpdate()
    {
        if (!isOpen()) {
            return;
        }

        _ui->setPosition(0, 0, f4vr::isNodeVisible(f4vr::getWeaponNode()) ? 6.0f : 0.0f);

        vrcf::VRControllersSuppress.setAllSuppressed(CONTROLLERS_SUPRESS_KEY, _beamTuningTglBtn->isToggleOn());

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
    void BeamScreen::handleBeamTuningAdjustments()
    {
        if (!_beamTuningTglBtn->isToggleOn()) {
            return;
        }

        const auto primaryDirection = vrcf::VRControllers.getThumbstickPressedDirection(vrcf::Hand::Primary, 0.8f, 0.5f);
        const auto offhandDirection = vrcf::VRControllers.getThumbstickPressedDirection(vrcf::Hand::Offhand, 0.8f, 0.5f);

        if (primaryDirection.has_value()) {
            switch (primaryDirection.value()) {
            case vrcf::Direction::Up:
                *FlashlightState::flashlightFade = fminf(*FlashlightState::flashlightFade + 0.1f, 4.0f);
                break;
            case vrcf::Direction::Down:
                *FlashlightState::flashlightFade = fmaxf(*FlashlightState::flashlightFade - 0.1f, 0.2f);
                break;
            case vrcf::Direction::Right:
                *FlashlightState::flashlightRadius = min(*FlashlightState::flashlightRadius + 200, 10000);
                break;
            case vrcf::Direction::Left:
                *FlashlightState::flashlightRadius = max(*FlashlightState::flashlightRadius - 200, 1000);
                break;
            }
            FlashlightState::toggleLightRefreshValues();
            _lastValuesChangeNotificationPensing = true;
        }

        if (offhandDirection.has_value()) {
            if (offhandDirection.value() == vrcf::Direction::Up) {
                *FlashlightState::flashlightFov = fminf(*FlashlightState::flashlightFov + 5, 150);
                FlashlightState::toggleLightRefreshValues();
                _lastValuesChangeNotificationPensing = true;
            } else if (offhandDirection.value() == vrcf::Direction::Down) {
                *FlashlightState::flashlightFov = fmaxf(*FlashlightState::flashlightFov - 5, 5);
                FlashlightState::toggleLightRefreshValues();
                _lastValuesChangeNotificationPensing = true;
            }
        }
    }

    /**
     * Show notification with current flashlight values after they were changed.
     * Try not to spam too much.
     */
    void BeamScreen::showBeamCurrentValuesNotification()
    {
        const auto now = nowMillis();
        if (_lastValuesChangeNotificationPensing && now - _lastValuesUpdateNotificationTime > 3000) {
            _lastValuesChangeNotificationPensing = false;
            _lastValuesUpdateNotificationTime = now;
            f4vr::showNotification(std::format("Beam values updated:\nIntensity = {:.1f}\nDistance = {}\nSpread = {:.0f}\xC2\xB0",
                *FlashlightState::flashlightFade,
                *FlashlightState::flashlightRadius,
                *FlashlightState::flashlightFov));
        }
    }

    /**
     * Switch the beam gobo to the next preset option.
     */
    void BeamScreen::switchBeamGobo()
    {
        const int nextGoboIndex = (findCurrentGoboPathIndex() + 1) % static_cast<int>(goboTextureFilePaths.size());
        *FlashlightState::flashlightGoboPath = goboTextureFilePaths[nextGoboIndex];

        FlashlightState::toggleLightRefreshValues();

        auto goboFileName = std::filesystem::path(*FlashlightState::flashlightGoboPath).stem().string();
        std::ranges::replace(goboFileName, '_', ' ');
        f4vr::showNotification(std::format("Beam Gobo: {}\nPreset: {} out of {}", goboFileName, nextGoboIndex + 1, goboTextureFilePaths.size()));
    }

    /**
     * Switch the beam color to the next preset option.
     */
    void BeamScreen::switchBeamColor()
    {
        const int nextColorIndex = (findCurrentColorIndex() + 1) % static_cast<int>(COLOR_OPTIONS.size());
        *FlashlightState::flashlightColorRed = COLOR_OPTIONS[nextColorIndex].rgb[0];
        *FlashlightState::flashlightColorGreen = COLOR_OPTIONS[nextColorIndex].rgb[1];
        *FlashlightState::flashlightColorBlue = COLOR_OPTIONS[nextColorIndex].rgb[2];

        FlashlightState::toggleLightRefreshValues();

        f4vr::showNotification(std::format("Beam Color: {}\nPreset: {} out of {}", COLOR_OPTIONS[nextColorIndex].name, nextColorIndex + 1, COLOR_OPTIONS.size()));
    }

    /**
     * Save the flashlight values only for the current selected location.
     */
    void BeamScreen::saveConfig()
    {
        f4vr::showNotification(std::format("{} flashlight beam values saved", getFlashlightLocationLabel(FlashlightState::flashlightLocation)));
        g_config.saveFlashlightValues(FlashlightState::flashlightLocation);
    }

    /**
     * Reset to default the flashlight values only for the current selected location.
     */
    void BeamScreen::resetConfig()
    {
        f4vr::showNotification(std::format("{} flashlight beam values reset to default", getFlashlightLocationLabel(FlashlightState::flashlightLocation)));
        g_config.resetFlashlightValuesToDefault(FlashlightState::flashlightLocation);
        FlashlightState::toggleLightRefreshValues();
    }

    void BeamScreen::switchingToOnHeadConfig()
    {
        setConfigModeFlashlightLocation(FlashlightLocation::OnHead);
    }

    void BeamScreen::switchingToOnPAHeadConfig()
    {
        setConfigModeFlashlightLocation(FlashlightLocation::OnPAHead);
    }

    void BeamScreen::switchingToInHandConfig()
    {
        setConfigModeFlashlightLocation(FlashlightLocation::InOffhand);
    }

    /**
     * Switch to on-weapon config regardless of what is in hand, the same way the PA head values can be tuned
     * out of power armor. With no weapon in hand the beam is shown from the primary hand instead of the weapon
     * (see Flashlight::adjustFlashlightTransformToHandOrHead), so the values can still be tuned.
     */
    void BeamScreen::switchingToOnWeaponConfig()
    {
        if (!f4vr::isNodeVisible(f4vr::getWeaponNode())) {
            f4vr::showNotification("No weapon in hand,\nshowing the on-weapon beam from the primary hand");
        }
        setConfigModeFlashlightLocation(FlashlightLocation::OnWeapon);
    }

    void BeamScreen::setFlashlightButtonsToggleStateByLocation() const
    {
        if (!Utils::isFlashlightOn()) {
            _row1ToggleContainer->clearToggleState();
            return;
        }
        switch (FlashlightState::flashlightLocation) {
        case FlashlightLocation::OnHead:
            _onHeadFLBtn->setToggleState(true);
            break;
        case FlashlightLocation::OnPAHead:
            _onPAHeadFLBtn->setToggleState(true);
            break;
        case FlashlightLocation::InOffhand:
        case FlashlightLocation::InPrimaryHand:
            _inHandFLBtn->setToggleState(true);
            break;
        case FlashlightLocation::OnWeapon:
            _onWeaponFLBtn->setToggleState(true);
            break;
        }
    }

    /**
     * Create all the beam config UI elements.
     */
    void BeamScreen::createUI()
    {
        _onHeadFLBtn = std::make_shared<UIToggleButtonPanel>("ImFl_OnHeadToggle");
        _onHeadFLBtn->setTopText("FLASHLIGHT");
        _onHeadFLBtn->setImage("vrui\\flashlight-on-head.DDS");
        _onHeadFLBtn->setBottomText("ON HEAD");
        _onHeadFLBtn->setOnToggleHandler([this](UIToggleButtonPanel*, bool) { switchingToOnHeadConfig(); });

        _onPAHeadFLBtn = std::make_shared<UIToggleButtonPanel>("ImFl_OnPAHeadToggle");
        _onPAHeadFLBtn->setTopText("FLASHLIGHT");
        _onPAHeadFLBtn->setImage("vrui\\flashlight-on-pa-head.DDS");
        _onPAHeadFLBtn->setBottomText("ON PA HEAD");
        _onPAHeadFLBtn->setOnToggleHandler([this](UIToggleButtonPanel*, bool) { switchingToOnPAHeadConfig(); });

        _inHandFLBtn = std::make_shared<UIToggleButtonPanel>("ImFl_InHandToggle");
        _inHandFLBtn->setTopText("FLASHLIGHT");
        _inHandFLBtn->setImage("vrui\\flashlight-in-hand.DDS");
        _inHandFLBtn->setBottomText("IN HAND");
        _inHandFLBtn->setOnToggleHandler([this](UIToggleButtonPanel*, bool) { switchingToInHandConfig(); });

        _onWeaponFLBtn = std::make_shared<UIToggleButtonPanel>("ImFl_OnWeaponToggle");
        _onWeaponFLBtn->setTopText("FLASHLIGHT");
        _onWeaponFLBtn->setImage("vrui\\flashlight-on-weapon.DDS");
        _onWeaponFLBtn->setBottomText("ON WEAPON");
        _onWeaponFLBtn->setOnToggleHandler([](UIToggleButtonPanel*, bool) { switchingToOnWeaponConfig(); });

        _row1ToggleContainer = std::make_shared<UIToggleGroupContainer>("Row1", UIContainerLayout::HorizontalCenter, 0.3f);
        _row1ToggleContainer->addElement(_onHeadFLBtn);
        _row1ToggleContainer->addElement(_onPAHeadFLBtn);
        _row1ToggleContainer->addElement(_inHandFLBtn);
        _row1ToggleContainer->addElement(_onWeaponFLBtn);
        setFlashlightButtonsToggleStateByLocation();

        _beamTuningTglBtn = std::make_shared<UIToggleButtonPanel>("ImFl_BeamTuningToggle");
        _beamTuningTglBtn->setTopText("BEAM");
        _beamTuningTglBtn->setImage("vrui\\beam-tuning.DDS");
        _beamTuningTglBtn->setBottomText("TUNING");
        _beamTuningTglBtn->setOnToggleHandler([this](UIToggleButtonPanel*, bool) {});

        const auto switchGoboBtn = std::make_shared<UIButtonPanel>("ImFl_SwitchGoboButton");
        switchGoboBtn->setTopText("SWITCH");
        switchGoboBtn->setImage("vrui\\switch-gobo.DDS");
        switchGoboBtn->setBottomText("GOBO");
        switchGoboBtn->setOnPressHandler([this](UIButtonPanel*) { switchBeamGobo(); });

        const auto switchColorBtn = std::make_shared<UIButtonPanel>("ImFl_SwitchColorButton");
        switchColorBtn->setTopText("SWITCH");
        switchColorBtn->setImage("vrui\\switch-color.DDS");
        switchColorBtn->setBottomText("COLOR");
        switchColorBtn->setOnPressHandler([this](UIButtonPanel*) { switchBeamColor(); });

        const auto row2Container = std::make_shared<UIContainer>("Row2", UIContainerLayout::HorizontalCenter, 0.3f);
        row2Container->addElement(_beamTuningTglBtn);
        row2Container->addElement(switchGoboBtn);
        row2Container->addElement(switchColorBtn);

        const auto saveBtn = std::make_shared<UIButtonPanel>("ImFl_SaveButton");
        saveBtn->setImage("vrui\\save.DDS");
        saveBtn->setBottomText("SAVE");
        saveBtn->setOnPressHandler([this](UIButtonPanel*) { saveConfig(); });

        const auto resetBtn = std::make_shared<UIButtonPanel>("ImFl_ResetButton");
        resetBtn->setImage("vrui\\reset.DDS");
        resetBtn->setBottomText("RESET");
        resetBtn->setOnPressHandler([this](UIButtonPanel*) { resetConfig(); });

        const auto backBtn = std::make_shared<UIButtonPanel>("ImFl_BackButton");
        backBtn->setImage("vrui\\exit.DDS");
        backBtn->setBottomText("BACK");
        backBtn->setOnPressHandler([this](UIButtonPanel*) {
            if (_onBack) {
                _onBack();
            }
        });

        const auto row3Container = std::make_shared<UIContainer>("Row3", UIContainerLayout::HorizontalCenter, 0.3f);
        row3Container->addElement(saveBtn);
        row3Container->addElement(resetBtn);
        row3Container->addElement(backBtn);

        // one footer whose text follows the beam tuning toggle
        const auto footer = std::make_shared<UITextPanel>("ImFl_BeamFooter");
        footer->setStyle(F4VR_PANEL_STYLE);
        footer->setTextHeight(0.2f);
        footer->setContent([beamTuningTglBtn = std::weak_ptr(_beamTuningTglBtn)](std::vector<TextRow>& rows) {
            const auto tuningBtn = beamTuningTglBtn.lock();
            if (tuningBtn && tuningBtn->isToggleOn()) {
                rows.emplace_back("PRIMARY STICK UP/DOWN: INTENSITY");
                rows.emplace_back("PRIMARY STICK LEFT/RIGHT: DISTANCE");
                rows.emplace_back("OFFHAND STICK UP/DOWN: SPREAD");
            } else {
                rows.emplace_back("TUNE VALUES SEPARATELY PER LOCATION");
                rows.emplace_back("SAVE TO PERSIST CHANGES BEFORE EXIT");
                rows.emplace_back("RESET RESETS TO MOD DEFAULTS");
            }
        });

        const auto row4Container = std::make_shared<UIContainer>("Row4", UIContainerLayout::HorizontalCenter, 0.3f);
        row4Container->addElement(footer);

        const auto header = std::make_shared<UITextPanel>("ImFl_Title");
        header->setStyle(UIPanelStyle{ .color = F4VR_PANEL_STYLE.color });
        header->setTextHeight(0.5f);
        header->setContent([](std::vector<TextRow>& rows) { rows.emplace_back("IMMERSIVE FLASHLIGHT: CONFIG", std::nullopt, f4cf::render::TextDecoration::Underline); });

        _ui = std::make_shared<UIContainer>("BeamConfig", UIContainerLayout::VerticalUp, 0.35f, 1.6f);
        _ui->addElement(row4Container);
        _ui->addElement(row3Container);
        _ui->addElement(row2Container);
        _ui->addElement(_row1ToggleContainer);
        _ui->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_ui, { 0, 0, 0 });
        FlashlightState::setFlashlightRuntimeLocationOverride(FlashlightState::flashlightLocation);
    }
}
