#pragma once

#include "BodyFlashlight.h"
#include "FlashlightMesh.h"
#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    class Flashlight
    {
    public:
        explicit Flashlight();

        void onFrameUpdate();
        void onGameSessionLoaded();

    private:
        void handlePowerArmorTransition(bool isFlashlightOn);
        void checkSwitchingFlashlightOnHeadHand();
        static void adjustFlashlightTransformToHandOrHead();
        void triggerHapticOnce(vrcf::Hand hand);
        void maybeShowFPSStabilizerModWarning();

        // to stop continuous flashlight haptic feedback
        bool _flashlightHapticActivated = false;
        bool _wasInPowerArmor = false;
        int _flashlightOnRecentlyFrames = 0;
        uint64_t _lastVRFPSStabilizerWarningTime = 0;

        FlashlightMesh _flashlightMesh;
        BodyFlashlight _bodyFlashlight;
    };
}
