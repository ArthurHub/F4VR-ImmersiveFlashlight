#pragma once

#include "BodyFlashlightMesh.h"
#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    /**
     * Controls the flashlight stowed on the player's body: owns the beam-less BodyFlashlightMesh and the
     * grab/return interaction. While the flashlight is off the model is shown on a body bone; a hand
     * brought within reach can grab it (turns the light on and into that hand) or, while holding it, put
     * it back (turns the light off). The configured grab button is suppressed from the game/FRIK while a
     * hand is in range so it can't also trigger its normal action.
     */
    class BodyFlashlight
    {
    public:
        explicit BodyFlashlight() = default;

        /**
         * Drives the stowed model and its grab/return interaction for the current frame. May toggle the
         * flashlight on/off (grab/return), so the caller should re-read the light state afterwards.
         */
        void onFrameUpdate();

        /**
         * Forces the stowed model to re-attach next frame. Call on power armor transition and game
         * session load since the skeleton pointers may have changed.
         */
        void invalidate();

    private:
        bool checkGrab(bool isFlashlightOn);
        void triggerHapticOnce(vrcf::Hand hand);

        BodyFlashlightMesh _mesh;
        bool _hapticActivated = false;
        uint64_t _lastToggleTime = 0;
    };
}
