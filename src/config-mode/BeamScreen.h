#pragma once

#include <functional>
#include <memory>

#include "vrui/UIContainer.h"
#include "vrui/UIToggleButton.h"
#include "vrui/UIToggleGroupContainer.h"
#include "vrui/UIWidget.h"

namespace ImFl::config
{
    /**
     * Beam config screen: pick the mount location (head / PA head / in-hand / on-weapon) and tune its
     * beam - intensity/radius/FOV via thumbstick, plus gobo and color presets - with Save/Reset for the
     * selected location. Reached from the main config screen; a Back button returns to it. The beam
     * shadows toggle lives on the misc screen.
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
        void handleBeamTuningAdjustments();
        void showBeamCurrentValuesNotification();
        static void switchBeamGobo();
        static void switchBeamColor();
        static void saveConfig();
        static void resetConfig();
        static void switchingToOnHeadConfig();
        static void switchingToOnPAHeadConfig();
        static void switchingToInHandConfig();
        void trySwitchingToOnWeaponConfig() const;
        void setFlashlightButtonsToggleStateByLocation() const;
        void createUI();

        // configuration UI
        std::shared_ptr<vrui::UIContainer> _ui;
        std::shared_ptr<vrui::UIToggleButton> _beamTuningTglBtn;
        std::shared_ptr<vrui::UIToggleButton> _onHeadFLBtn;
        std::shared_ptr<vrui::UIToggleButton> _onPAHeadFLBtn;
        std::shared_ptr<vrui::UIToggleButton> _inHandFLBtn;
        std::shared_ptr<vrui::UIToggleButton> _onWeaponFLBtn;
        std::shared_ptr<vrui::UIToggleGroupContainer> _row1ToggleContainer;
        std::shared_ptr<vrui::UIWidget> _configMsg;
        std::shared_ptr<vrui::UIWidget> _beamTuningMsg;

        std::function<void()> _onBack;

        // used to limit how often we notify about last changed values
        uint64_t _lastValuesUpdateNotificationTime = 0;
        bool _lastValuesChangeNotificationPensing = false;
    };
}
