#pragma once

#include "Config.h"
#include "vrui/UIContainer.h"
#include "vrui/UIToggleButton.h"
#include "vrui/UIToggleGroupContainer.h"

namespace ImFl
{
    class FlashlightConfigMode
    {
    public :
        int isOpen() const;
        void openConfigMode();
        void closeConfigMode();
        void showBeamCurrentValuesNotification();
        void onFrameUpdate();

    private:
        void handleBeamTuningAdjustments();
        static void switchBeamGobo();
        static void switchBeamColor();
        static void saveConfig();
        static void resetConfig();
        static void switchingToOnHeadConfig();
        static void switchingToInHandConfig();
        void trySwitchingToOnWeaponConfig() const;
        void disablePlayerInput(bool disable);
        void setFlashlightButtonsToggleStateByLocation() const;
        void createMainConfigUI();

        // configuration UI
        std::shared_ptr<vrui::UIContainer> _configUI;
        std::shared_ptr<vrui::UIToggleButton> _beamTuningTglBtn;
        std::shared_ptr<vrui::UIToggleButton> _onHeadFLBtn;
        std::shared_ptr<vrui::UIToggleButton> _inHandFLBtn;
        std::shared_ptr<vrui::UIToggleButton> _onWeaponFLBtn;
        std::shared_ptr<vrui::UIToggleGroupContainer> _row1ToggleContainer;
        std::shared_ptr<vrui::UIWidget> _configMsg;
        std::shared_ptr<vrui::UIWidget> _beamTuningMsg;

        bool _inputDisabled = false;

        // used to limit how often we notify about last changed values
        uint64_t _lastValuesUpdateNotificationTime = 0;
        bool _lastValuesChangeNotificationPensing = false;
    };
}
