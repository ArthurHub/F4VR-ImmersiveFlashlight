#pragma once

#include "BodyFlashlightMesh.h"
#include "FlashlightMesh.h"
#include "f4vr/WandActivationSphere.h"
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
        void updateBodyStow();
        bool checkBodyGrab(bool enabled);
        void checkHeadActivation();
        void checkPrimaryHandActivation();
        void checkSwitchingFlashlightOnHeadHand();
        static void adjustFlashlightTransformToHandOrHead();
        void triggerHapticOnce(vrcf::Hand hand);
        void maybeShowFPSStabilizerModWarning();

        // to stop continuous flashlight haptic feedback
        bool _flashlightHapticActivated = false;
        bool _wasInPowerArmor = false;
        int _flashlightOnRecentlyFrames = 0;
        uint64_t _lastVRFPSStabilizerWarningTime = 0;

        FlashlightMesh _inHandFlashlightMesh;
        BodyFlashlightMesh _bodyFlashlightMesh;
        f4vr::WandActivationSphere _bodyGrabSphere;
        f4vr::WandActivationSphere _headSphere;
        f4vr::WandActivationSphere _primaryHandSphere;
    };
}
