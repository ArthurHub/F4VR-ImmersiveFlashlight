#pragma once

#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    class Flashlight
    {
    public:
        explicit Flashlight();

        void onFrameUpdate();

    private:
        void checkSwitchingFlashlightOnHeadHand();
        static void adjustFlashlightTransformToHandOrHead();
        void triggerHapticOnce(vrcf::Hand hand);

        // to stop continuous flashlight haptic feedback
        bool _flashlightHapticActivated = false;
    };
}
