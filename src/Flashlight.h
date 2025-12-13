#pragma once

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
        static void setLightValues();

        // to stop continuous flashlight haptic feedback
        bool _flashlightHapticActivated = false;
    };
}
