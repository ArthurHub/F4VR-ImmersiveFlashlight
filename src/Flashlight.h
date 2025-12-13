#pragma once
#include "Config.h"
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
        static void switchFlashlightLocation(FlashlightLocation location);
        static void adjustFlashlightTransformToHandOrHead();
        static void setLightValues();
        void triggerHapticOnce(vrcf::Hand hand);

        // to stop continuous flashlight haptic feedback
        bool _flashlightHapticActivated = false;
    };
}
