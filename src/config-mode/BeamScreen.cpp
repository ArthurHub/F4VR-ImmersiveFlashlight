#include "BeamScreen.h"

#include "Config.h"
#include "FlashlightState.h"
#include "Utils.h"
#include "api/FRIKApi.h"
#include "f4vr/PlayerNodes.h"
#include "modsconfig/ModsConfigFO4Api.h"
#include "vrcf/VRControllersManager.h"
#include "vrcf/VRControllersSuppressor.h"
#include "vrui/BindingPrompt.h"
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

    // how long the value a thumbstick adjustment just changed stays highlighted in the values panel
    constexpr uint64_t TUNED_VALUE_HIGHLIGHT_MS = 1500;

    // the width of the two preset buttons the values panel replaces plus the padding between them, so the
    // beam tuning toggle beside it stays where it is when they are swapped
    constexpr float BEAM_VALUES_PANEL_WIDTH = 4.3f;

    // as tall as the buttons beside it, so the row stays even
    constexpr float BEAM_VALUES_PANEL_HEIGHT = 2.0f;

    // where a value starts, measured from the start of its row, so the three line up in a column
    constexpr float BEAM_VALUES_TAB_WIDTH = 2.4f;

    // spreads the three rows over the panel's height
    constexpr float BEAM_VALUES_LINE_SPACING = 2.4f;

    // the lines are drawn down from the top edge, so the top padding is what places them in the height
    constexpr UIPanelStyle BEAM_VALUES_PANEL_STYLE = [] {
        auto style = F4VR_PANEL_STYLE;
        style.padding = { .top = 0.2f, .right = 0.3f, .bottom = 0.15f, .left = 0.3f };
        return style;
    }();

    struct ColorOption
    {
        RE::ColorStruct color;

        // what the color button calls the preset, short enough not to be shrunk on a 2-unit button
        std::string_view name;
    };

    constexpr std::array<ColorOption, 8> COLOR_OPTIONS{ {
        { .color = { 255, 255, 255 }, .name = "FULL WHITE" },
        { .color = { 240, 230, 225 }, .name = "MOSTLY WHITE" },
        { .color = { 235, 224, 190 }, .name = "WARM WHITE" },
        { .color = { 255, 200, 150 }, .name = "WARM DIM" },
        { .color = { 255, 210, 210 }, .name = "RED" },
        { .color = { 210, 255, 210 }, .name = "GREEN" },
        { .color = { 190, 190, 255 }, .name = "BLUE" },
        { .color = { 190, 100, 255 }, .name = "PURPLE" },
    } };

    int findCurrentColorIndex()
    {
        const auto& current = *ImFl::FlashlightState::flashlightColor;
        for (std::size_t i = 0; i < COLOR_OPTIONS.size(); ++i) {
            const auto& c = COLOR_OPTIONS[i].color;
            if (c.red == current.red && c.green == current.green && c.blue == current.blue) {
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

    // the player was told this opening of the screen that the menu refuses its edits, so it is told once
    bool refusedEditsNotified = false;

    /**
     * Hand beam values of the tuned location to Mods Config FO4 VR's menu as pending edits, which its Save writes and
     * its Discard takes back: the screen writes no file. All are read before the first is handed over, as each comes
     * back as a preview whose override reloads the config, which drops a value not handed over yet. One the menu
     * refuses, as when it no longer shows the mod, is dropped as the screen closes, which the player is told once.
     */
    void handOverBeamValues(const std::initializer_list<ImFl::FlashlightBeamValue> values)
    {
        std::vector<std::pair<std::string, mods_config::api::Value>> settings;
        for (const auto value : values) {
            settings.push_back(ImFl::g_config.getFlashlightValueSetting(ImFl::FlashlightState::flashlightLocation, value));
        }
        for (const auto& [id, value] : settings) {
            if (!mods_config::api::ModsConfigFO4Client::get().setPendingValue(id, value)) {
                logger::warn("Beam value '{}' is not taken by the config menu, it is dropped as the beam tuning closes", id);
                if (!refusedEditsNotified) {
                    refusedEditsNotified = true;
                    f4vr::showNotification("Beam changes can't be kept:\nopen the beam tuning from Mods Config");
                }
            }
        }
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
        _closeRequested = false;
        refusedEditsNotified = false;
        FlashlightState::refreshFlashlightLocation();
        createUI();
        Utils::turnFlashlightOn();
    }

    /**
     * Reload the config, clear the preview override, release controller suppression, and detach the UI. The
     * reload keeps the tuned values the menu took, as its previews are overrides of the config, and drops
     * any it refused.
     */
    void BeamScreen::close()
    {
        if (!isOpen()) {
            return;
        }
        _closeRequested = false;

        FlashlightState::setFlashlightRuntimeLocationOverride(std::nullopt);
        g_config.load();
        if (Utils::isFlashlightOn()) {
            FlashlightState::refreshLightValues();
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
        _switchGoboBtn.reset();
        _switchColorBtn.reset();
        _beamValuesPanel.reset();
        _shownGoboPath.clear();
        _shownColorTint.reset();
    }

    /**
     * Handle the beam config screen on every frame update: close it if Exit was pressed or FRIK's config opened.
     */
    void BeamScreen::onFrameUpdate()
    {
        F4CF_PERF_FUNCTION();
        if (!isOpen()) {
            return;
        }

        if (_closeRequested || (frik::api::FRIKApi::inst && frik::api::FRIKApi::inst->isConfigOpen())) {
            close();
            return;
        }

        _ui->setPosition(0, 0, f4vr::isNodeVisible(f4vr::getWeaponNode()) ? 6.0f : 0.0f);

        vrcf::VRControllersSuppress.setAllSuppressed(CONTROLLERS_SUPRESS_KEY, _beamTuningTglBtn->isToggleOn());

        setFlashlightButtonsToggleStateByLocation();

        refreshPresetButtons();

        updateBeamTuningPanelVisibility();

        handleBeamTuningAdjustments();
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
            vrcf::VRHaptics.trigger(vrcf::Hand::Primary, vrcf::HapticPattern::Tick);
            switch (primaryDirection.value()) {
            case vrcf::Direction::Up:
                *FlashlightState::flashlightFade = fminf(*FlashlightState::flashlightFade + 0.1f, 4.0f);
                markValueTuned(TunedValue::Intensity);
                handOverBeamValues({ FlashlightBeamValue::Fade });
                break;
            case vrcf::Direction::Down:
                *FlashlightState::flashlightFade = fmaxf(*FlashlightState::flashlightFade - 0.1f, 0.2f);
                markValueTuned(TunedValue::Intensity);
                handOverBeamValues({ FlashlightBeamValue::Fade });
                break;
            case vrcf::Direction::Right:
                *FlashlightState::flashlightRadius = min(*FlashlightState::flashlightRadius + 200, 10000);
                markValueTuned(TunedValue::Distance);
                handOverBeamValues({ FlashlightBeamValue::Radius });
                break;
            case vrcf::Direction::Left:
                *FlashlightState::flashlightRadius = max(*FlashlightState::flashlightRadius - 200, 1000);
                markValueTuned(TunedValue::Distance);
                handOverBeamValues({ FlashlightBeamValue::Radius });
                break;
            }
            FlashlightState::refreshLightValues(LightRefreshMode::InPlace);
        }

        if (offhandDirection.has_value()) {
            if (offhandDirection.value() == vrcf::Direction::Up) {
                vrcf::VRHaptics.trigger(vrcf::Hand::Offhand, vrcf::HapticPattern::Tick);
                *FlashlightState::flashlightFov = fminf(*FlashlightState::flashlightFov + 5, 150);
                markValueTuned(TunedValue::Spread);
                handOverBeamValues({ FlashlightBeamValue::Fov });
                FlashlightState::refreshLightValues(LightRefreshMode::InPlace);
            } else if (offhandDirection.value() == vrcf::Direction::Down) {
                vrcf::VRHaptics.trigger(vrcf::Hand::Offhand, vrcf::HapticPattern::Tick);
                *FlashlightState::flashlightFov = fmaxf(*FlashlightState::flashlightFov - 5, 5);
                markValueTuned(TunedValue::Spread);
                handOverBeamValues({ FlashlightBeamValue::Fov });
                FlashlightState::refreshLightValues(LightRefreshMode::InPlace);
            }
        }
    }

    /**
     * Remember which value a thumbstick adjustment just changed, so the values panel can highlight it.
     */
    void BeamScreen::markValueTuned(const TunedValue value)
    {
        _lastTunedValue = value;
        _lastTunedValueTime = nowMillis();
    }

    /**
     * One row of the values panel: the value's name, then the value itself at the tab stop so the three
     * line up, highlighted while it is the one the last thumbstick adjustment changed.
     */
    vrui::TextRow BeamScreen::beamValueRow(const std::string_view label, const std::string& value, const TunedValue tunedValue) const
    {
        const auto highlighted = tunedValue == _lastTunedValue && nowMillis() - _lastTunedValueTime < TUNED_VALUE_HIGHLIGHT_MS;
        return { .spans = { { .text = std::format("{}\t", label) }, { .text = value, .color = highlighted ? std::optional(render::colors::Yellow) : std::nullopt } } };
    }

    /**
     * Paint the preset buttons with the preset they are on: the gobo texture itself as the gobo button's
     * image, the beam color as the color button's tint, each naming its preset below. Runs every frame,
     * since a location switch, a reset or a config reload changes the preset as much as a press does, but
     * touches a button only when what it shows has actually changed - the gobo one loads a texture.
     */
    void BeamScreen::refreshPresetButtons()
    {
        if (const auto& goboPath = *FlashlightState::flashlightGoboPath; goboPath != _shownGoboPath) {
            _shownGoboPath = goboPath;
            _switchGoboBtn->setImage(goboPath);
            const auto goboIndex = findCurrentGoboPathIndex();
            _switchGoboBtn->setBottomText(goboIndex < 0 ? "GOBO" : std::format("GOBO {}/{}", goboIndex + 1, goboTextureFilePaths.size()));
        }

        const auto color = render::Color::rgba(FlashlightState::flashlightColor->red, FlashlightState::flashlightColor->green, FlashlightState::flashlightColor->blue);
        if (_shownColorTint != color) {
            _shownColorTint = color;
            _switchColorBtn->setImageTint(color);
            const auto colorIndex = findCurrentColorIndex();
            _switchColorBtn->setBottomText(colorIndex < 0 ? "CUSTOM" : std::string(COLOR_OPTIONS[colorIndex].name));
        }
    }

    /**
     * While beam tuning is on the gobo and color buttons give their place in the row to the values panel:
     * the three tuned values are what the thumbsticks are changing, and the presets are not. The thumbstick
     * prompts show under that row only then, and take no room otherwise.
     */
    void BeamScreen::updateBeamTuningPanelVisibility() const
    {
        const auto tuning = _beamTuningTglBtn->isToggleOn();
        _switchGoboBtn->setVisibility(!tuning);
        _switchColorBtn->setVisibility(!tuning);
        _beamValuesPanel->setVisibility(tuning);
        _tuningPromptsRow->setVisibility(tuning);
    }

    /**
     * Switch the beam gobo to the next preset option.
     */
    void BeamScreen::switchBeamGobo()
    {
        const int nextGoboIndex = (findCurrentGoboPathIndex() + 1) % static_cast<int>(goboTextureFilePaths.size());
        *FlashlightState::flashlightGoboPath = goboTextureFilePaths[nextGoboIndex];
        handOverBeamValues({ FlashlightBeamValue::GoboPath });

        FlashlightState::refreshLightValues(LightRefreshMode::InPlace);
    }

    /**
     * Switch the beam color to the next preset option.
     */
    void BeamScreen::switchBeamColor()
    {
        const int nextColorIndex = (findCurrentColorIndex() + 1) % static_cast<int>(COLOR_OPTIONS.size());
        *FlashlightState::flashlightColor = COLOR_OPTIONS[nextColorIndex].color;
        handOverBeamValues({ FlashlightBeamValue::Color });

        FlashlightState::refreshLightValues(LightRefreshMode::InPlace);
    }

    /**
     * Reset to default the flashlight values only for the current selected location, as pending edits of the menu.
     */
    void BeamScreen::resetConfig()
    {
        f4vr::showNotification(std::format("{} flashlight beam values reset to default", getFlashlightLocationLabel(FlashlightState::flashlightLocation)));
        g_config.resetFlashlightValuesToDefault(FlashlightState::flashlightLocation);
        handOverBeamValues({ FlashlightBeamValue::Fade, FlashlightBeamValue::Radius, FlashlightBeamValue::Fov, FlashlightBeamValue::Color, FlashlightBeamValue::GoboPath });
        FlashlightState::refreshLightValues(LightRefreshMode::InPlace);
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

        // the image and the bottom line of both preset buttons are the preset itself - see refreshPresetButtons
        _switchGoboBtn = std::make_shared<UIButtonPanel>("ImFl_SwitchGoboButton");
        _switchGoboBtn->setTopText("SWITCH");
        _switchGoboBtn->setOnPressHandler([this](UIButtonPanel*) { switchBeamGobo(); });

        _switchColorBtn = std::make_shared<UIButtonPanel>("ImFl_SwitchColorButton");
        _switchColorBtn->setTopText("SWITCH");
        _switchColorBtn->setImage("vrui\\switch-color.DDS");
        _switchColorBtn->setOnPressHandler([this](UIButtonPanel*) { switchBeamColor(); });

        _beamValuesPanel = std::make_shared<UITextPanel>("ImFl_BeamValues", BEAM_VALUES_PANEL_WIDTH, BEAM_VALUES_PANEL_HEIGHT);
        _beamValuesPanel->setStyle(BEAM_VALUES_PANEL_STYLE);
        _beamValuesPanel->setTabWidth(BEAM_VALUES_TAB_WIDTH);
        _beamValuesPanel->setLineSpacing(BEAM_VALUES_LINE_SPACING);
        _beamValuesPanel->setContent([this](std::vector<TextRow>& rows) {
            rows.push_back(beamValueRow("INTENSITY", std::format("{:.1f}", *FlashlightState::flashlightFade), TunedValue::Intensity));
            rows.push_back(beamValueRow("DISTANCE", std::format("{}", *FlashlightState::flashlightRadius), TunedValue::Distance));
            rows.push_back(beamValueRow("SPREAD", std::format("{:.0f}\xC2\xB0", *FlashlightState::flashlightFov), TunedValue::Spread));
        });

        // the preset buttons and the values panel share the middle of the row, one pair or the other showing
        const auto row2Container = std::make_shared<UIContainer>("Row2", UIContainerLayout::HorizontalCenter, 0.3f);
        row2Container->addElement(_beamTuningTglBtn);
        row2Container->addElement(_switchGoboBtn);
        row2Container->addElement(_switchColorBtn);
        row2Container->addElement(_beamValuesPanel);

        const auto resetBtn = std::make_shared<UIButtonPanel>("ImFl_ResetButton");
        resetBtn->setImage("f4cf\\vrui\\reset.DDS");
        resetBtn->setBottomText("RESET");
        resetBtn->setOnPressHandler([this](UIButtonPanel*) { resetConfig(); });

        const auto exitBtn = std::make_shared<UIButtonPanel>("ImFl_ExitButton");
        exitBtn->setImage("f4cf\\vrui\\exit.DDS");
        exitBtn->setBottomText("EXIT");
        exitBtn->setOnPressHandler([this](UIButtonPanel*) { _closeRequested = true; });

        const auto row3Container = std::make_shared<UIContainer>("Row3", UIContainerLayout::HorizontalCenter, 0.3f);
        row3Container->addElement(resetBtn);
        row3Container->addElement(exitBtn);

        // the thumbstick prompts of the beam tuning, in one line under the row they tune, shown while it is on
        const auto tuningPrompts = std::make_shared<UITextPanel>("ImFl_BeamTuningPrompts");
        auto tuningPromptsStyle = F4VR_PANEL_STYLE;
        tuningPromptsStyle.padding.left = 0.3f;
        tuningPromptsStyle.padding.right = 0.3f;
        tuningPrompts->setStyle(tuningPromptsStyle);
        tuningPrompts->setTextHeight(0.22f);
        tuningPrompts->setContent([](std::vector<TextRow>& rows) {
            // the tuning reads the thumbsticks directly (see handleBeamTuningAdjustments), so the prompts
            // are built from bindings describing exactly those reads; their icons draw the stick's axis
            constexpr BindingPromptStyle STICK_PROMPT_STYLE{ .imageHeight = 2.8f };
            const auto stick = [](const vrcf::Hand hand, const vrcf::Direction direction) {
                return vrcf::InputBinding{ .hand = hand, .type = vrcf::ActivationType::AxisDirection, .axis = vrcf::Axis::Thumbstick, .direction = direction };
            };
            rows.push_back({});
            for (const auto& [binding, text] : { std::pair{ stick(vrcf::Hand::Primary, vrcf::Direction::UpDown), " INTENSITY    " },
                     std::pair{ stick(vrcf::Hand::Primary, vrcf::Direction::RightLeft), " DISTANCE    " },
                     std::pair{ stick(vrcf::Hand::Offhand, vrcf::Direction::UpDown), " SPREAD" } }) {
                appendBindingPrompt(rows.back().spans, binding, STICK_PROMPT_STYLE);
                rows.back().spans.push_back({ .text = text });
            }
        });

        _tuningPromptsRow = std::make_shared<UIContainer>("RowTuningPrompts", UIContainerLayout::HorizontalCenter, 0.3f);
        _tuningPromptsRow->addElement(tuningPrompts);

        refreshPresetButtons();
        updateBeamTuningPanelVisibility();

        const auto footer = std::make_shared<UITextPanel>("ImFl_BeamFooter");
        footer->setStyle(F4VR_PANEL_STYLE);
        footer->setTextHeight(0.22f);
        footer->setContent([](std::vector<TextRow>& rows) {
            rows.emplace_back("TUNE VALUES SEPARATELY PER LOCATION");
            rows.emplace_back("RESET RESETS TO MOD DEFAULTS");
            rows.emplace_back("SAVE OR DISCARD CHANGES IN THE CONFIG MENU");
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
        _ui->addElement(_tuningPromptsRow);
        _ui->addElement(row2Container);
        _ui->addElement(_row1ToggleContainer);
        _ui->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_ui, { 0, 0, 0 });
        FlashlightState::setFlashlightRuntimeLocationOverride(FlashlightState::flashlightLocation);
    }
}
