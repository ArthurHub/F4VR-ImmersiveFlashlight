#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "vrui/UIButtonPanel.h"
#include "vrui/UIContainer.h"
#include "vrui/UITextPanel.h"
#include "vrui/UIToggleButtonPanel.h"
#include "vrui/UIToggleGroupContainer.h"

namespace ImFl::config
{
    /**
     * Beam config screen: pick the mount location (head / PA head / in-hand / on-weapon) and tune its
     * beam - intensity/radius/FOV via thumbstick, plus gobo and color presets - with Save/Reset for the
     * selected location. Reached from the main config screen; a Back button returns to it. The beam
     * shadows toggle lives on the misc screen.
     *
     * The two preset buttons show the preset they are on: the gobo button draws the gobo texture itself
     * and the color button is tinted with the beam color. While beam tuning is on they give their place
     * in the row to a panel with the three tuned values, which is where the thumbstick adjustments are
     * read off - the value just changed is highlighted for a moment.
     */
    class BeamScreen
    {
    public:
        bool isOpen() const;
        void open();
        void close();
        void onFrameUpdate();

        void setOnBackHandler(std::function<void()> handler);

    private:
        /**
         * One of the three thumbstick-tuned beam values, to highlight the one last changed.
         */
        enum class TunedValue : uint8_t
        {
            None,
            Intensity,
            Distance,
            Spread,
        };

        void handleBeamTuningAdjustments();
        void markValueTuned(TunedValue value);
        vrui::TextRow beamValueRow(std::string_view label, const std::string& value, TunedValue tunedValue) const;
        void refreshPresetButtons();
        void updateBeamTuningPanelVisibility() const;
        static void switchBeamGobo();
        static void switchBeamColor();
        static void saveConfig();
        static void resetConfig();
        static void switchingToOnHeadConfig();
        static void switchingToOnPAHeadConfig();
        static void switchingToInHandConfig();
        static void switchingToOnWeaponConfig();
        void setFlashlightButtonsToggleStateByLocation() const;
        void createUI();

        // configuration UI
        std::shared_ptr<vrui::UIContainer> _ui;
        std::shared_ptr<vrui::UIToggleButtonPanel> _beamTuningTglBtn;
        std::shared_ptr<vrui::UIToggleButtonPanel> _onHeadFLBtn;
        std::shared_ptr<vrui::UIToggleButtonPanel> _onPAHeadFLBtn;
        std::shared_ptr<vrui::UIToggleButtonPanel> _inHandFLBtn;
        std::shared_ptr<vrui::UIToggleButtonPanel> _onWeaponFLBtn;
        std::shared_ptr<vrui::UIToggleGroupContainer> _row1ToggleContainer;
        std::shared_ptr<vrui::UIButtonPanel> _switchGoboBtn;
        std::shared_ptr<vrui::UIButtonPanel> _switchColorBtn;
        std::shared_ptr<vrui::UITextPanel> _beamValuesPanel;

        std::function<void()> _onBack;

        // what the preset buttons are currently painted with, so the gobo texture is only reloaded and the
        // labels only rewritten when the preset actually changes (a press, a location switch, a reset...)
        std::string _shownGoboPath;
        std::optional<render::Color> _shownColorTint;

        // the value the last thumbstick adjustment changed, and when, for the values panel highlight
        TunedValue _lastTunedValue = TunedValue::None;
        uint64_t _lastTunedValueTime = 0;
    };
}
