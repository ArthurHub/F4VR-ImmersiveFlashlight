#pragma once

#include <functional>
#include <memory>

#include "vrui/UIContainer.h"
#include "vrui/UIToggleButtonPanel.h"
#include "vrui/UIToggleGroupContainer.h"

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

        std::function<void()> _onBack;

        // used to limit how often we notify about last changed values
        uint64_t _lastValuesUpdateNotificationTime = 0;
        bool _lastValuesChangeNotificationPensing = false;
    };
}
