#pragma once

#include "vrui/UIContainer.h"
#include "vrui/UIToggleButton.h"

namespace ImFl
{
    class ConfigMode
    {
    public :
        int isOpen() const;
        void openConfigMode();
        void showFlahlightCurrentValuesNotification();
        void onFrameUpdate();

    private:
        void handleValuesAdjustment();
        void switchFlashlightGobo();
        void switchFlashlightColor();
        void saveConfig();
        void resetConfig();
        void disablePlayerInput(bool disable);
        void createMainConfigUI();
        void closeConfigMode();

        // configuration UI
        std::shared_ptr<vrui::UIContainer> _configUI;
        std::shared_ptr<vrui::UIToggleButton> _flashlightValuesTglBtn;

        bool _disabledInput = false;

        // used to limit how often we notify about last changed values
        uint64_t _lastValuesUpdateNotificationTime = 0;
        bool _lastValuesUpdateTime = false;
    };
}
