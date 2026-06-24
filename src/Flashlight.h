#pragma once

#include "BodyFlashlightMesh.h"
#include "FlashlightMesh.h"
#include "f4vr/WandActivationSphere.h"
#include "vrcf/VRControllersManager.h"

namespace ImFl
{
    /**
     * Drives the VR flashlight: per-frame location tracking, on/off detection, the power-armor transition
     * fix, the stowed body model, and the three proximity gestures that move the light between locations.
     * The light is either off or on at one location — head (OnHead / OnPAHead), offhand, primary hand, or
     * weapon (the primary-hand location resolved with a drawn regular weapon). Each gesture suppresses its
     * bound button (with a one-shot entry haptic) only while it is actually actionable in the current state.
     *
     * Available transitions:
     *
     * - Body grab (offhand / primary-hand button at the chest stow): grabs the light into that hand — on
     *   from off, or moved there from the head (and, for the offhand only, from the weapon). Firing it again
     *   while the light is already held in that hand returns it (off). The primary hand can neither grab nor
     *   return while it holds a drawn weapon. Hand-to-hand moves are not done here — use the primary-hand
     *   gesture.
     *
     * - Head activation (offhand wand near the HMD): a tap puts the light on the head — on from off, switched
     *   there from a hand/weapon, or off when already head-mounted. A long-press pulls a head-mounted light to
     *   the offhand.
     *
     * - Primary-hand activation (offhand wand near the primary-hand wand): a tap moves/toggles the on light
     *   among the offhand, primary hand, and weapon — offhand -> primary hand (empty) or -> weapon (regular
     *   weapon drawn), primary hand -> offhand, head -> weapon (regular weapon drawn), weapon -> off — and,
     *   from off, turns it on at the weapon (regular weapon drawn, regardless of stored location). A
     *   melee/unarmed weapon is inert. A long-press pulls an on-weapon light back to the offhand.
     *
     * - Global toggle (a single binding, no proximity zone): turns the light on at its current location or
     *   off from anywhere. It defers to the three proximity gestures above whenever one is claiming the same
     *   input that frame, so a press inside an activation zone runs that gesture instead of double-toggling.
     *
     * Outside the gestures, entering/exiting power armor re-resolves the location (and restores a recently-on
     * light), and the in-game config UI can set the location directly.
     */
    class Flashlight
    {
    public:
        explicit Flashlight();

        void onFrameUpdate();
        void onGameSessionLoaded();

    private:
        void handlePowerArmorTransition();
        void updateBodyStow();
        bool checkBodyGrab(bool enabled);
        void checkHeadActivation();
        void checkPrimaryHandActivation();
        void checkGlobalToggle() const;
        static void adjustFlashlightTransformToHandOrHead();
        void maybeShowFPSStabilizerModWarning();

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
