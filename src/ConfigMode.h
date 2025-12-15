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
        void onFrameUpdate();

    private:
        void handleValuesAdjustment() const;
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
    };
}
