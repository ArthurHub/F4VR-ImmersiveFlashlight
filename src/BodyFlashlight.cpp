#include "BodyFlashlight.h"

#include "Config.h"
#include "Utils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"
#include "vrcf/VRControllersHaptic.h"
#include "vrcf/VRControllersManager.h"
#include "vrcf/VRControllersSuppressor.h"

using namespace common;

namespace
{
    // Owner key for body-grab input suppression and the post-toggle cooldown that prevents a single
    // grip tap from immediately grabbing-then-returning while the hand lingers at the stow point.
    constexpr auto BODY_GRAB_KEY = "ImFl_BodyGrab";
    constexpr uint64_t BODY_GRAB_COOLDOWN_MS = 400;

    /** Release every body-grab input suppression this mod owns (both hands). */
    void releaseGrabSuppression()
    {
        vrcf::VRControllersSuppress.release(BODY_GRAB_KEY);
    }
}

namespace ImFl
{
    /**
     * Keeps the (beam-less) stowed model attached to the configured body bone whenever the feature is
     * enabled, runs the proximity-gated grab/return logic, then shows the model only while the flashlight
     * is off (stowed). A short cooldown after a toggle suppresses an accidental immediate re-trigger.
     */
    void BodyFlashlight::onFrameUpdate()
    {
        const bool isFlashlightOn = f4vr::isPipboyLightOn(f4vr::getPlayer());
        const bool enabled = g_config.showFlashlightOnBody && f4vr::getRootNode() != nullptr;
        _mesh.onFrameUpdate(enabled);

        if (!enabled) {
            releaseGrabSuppression();
            _hapticActivated = false;
            return;
        }

        // A grab/return may toggle the light, so track the resulting state to set the model visibility.
        bool lightOnNow = isFlashlightOn;
        if (_mesh.isAttached() && isNowTimePassed(_lastToggleTime, BODY_GRAB_COOLDOWN_MS)) {
            if (checkGrab(isFlashlightOn)) {
                lightOnNow = f4vr::isPipboyLightOn(f4vr::getPlayer());
            }
        } else {
            releaseGrabSuppression();
            _hapticActivated = false;
        }

        // Show the stowed model only while the flashlight is off (holstered).
        _mesh.setVisible(!lightOnNow);
    }

    /** Forces the stowed model to re-attach to fresh skeleton nodes next frame. */
    void BodyFlashlight::invalidate()
    {
        _mesh.invalidate();
    }

    /**
     * Evaluate the grab (light off -> on into a hand) and return (held -> off + re-stow) interactions for
     * both hands against the stowed model position. While a usable hand is within reach its configured
     * grab button is suppressed from the game/FRIK and a one-shot haptic hints the zone; firing the
     * binding toggles the flashlight. Only the hand currently holding the flashlight can return it.
     * A hand occupied by a drawn weapon (the primary hand) can neither grab nor return the flashlight.
     * @return true if a grab or return toggled the flashlight this frame.
     */
    bool BodyFlashlight::checkGrab(const bool isFlashlightOn)
    {
        bool anyHandNear = false;
        bool toggled = false;

        const auto processHand = [&](const vrcf::Hand hand,
                                     const vrcf::InputBinding& binding,
                                     const RE::NiNode* wand,
                                     const FlashlightLocation heldLocation,
                                     const FlashlightConfigLocation grabTarget,
                                     const bool blockedByWeapon) {
            if (toggled || binding.type == vrcf::ActivationType::Disabled || !wand) {
                return;
            }

            // A hand holding a drawn weapon is occupied; it can't grab the flashlight or put it away.
            if (blockedByWeapon) {
                vrcf::VRControllersSuppress.release(BODY_GRAB_KEY, hand, binding.button);
                return;
            }

            const bool canGrab = !isFlashlightOn; // stowed -> grab it on into this hand
            const bool canReturn = isFlashlightOn && Utils::flashlightLocation == heldLocation; // held in this hand -> put it back off
            if (!canGrab && !canReturn) {
                vrcf::VRControllersSuppress.release(BODY_GRAB_KEY, hand, binding.button);
                return;
            }

            if (!_mesh.isWithinGrabSphere(wand->world.translate)) {
                vrcf::VRControllersSuppress.release(BODY_GRAB_KEY, hand, binding.button);
                return;
            }

            anyHandNear = true;
            // Hide the configured grab button from the game/FRIK while the hand is in range.
            vrcf::VRControllersSuppress.suppress(BODY_GRAB_KEY, hand, binding.button);
            triggerHapticOnce(hand);

            // Evaluate the binding on this specific hand; the config hand value is overridden by proximity.
            vrcf::InputBinding handBinding = binding;
            handBinding.hand = hand;
            if (vrcf::VRControllers.check(handBinding)) {
                if (canGrab) {
                    Utils::turnFlashlightOn();
                    Utils::switchFlashlightConfigLocation(grabTarget);
                } else {
                    Utils::turnFlashlightOff();
                }
                vrcf::VRHaptics.trigger(hand, vrcf::HapticPattern::DoubleClick);
                _lastToggleTime = nowMillis();
                toggled = true;
            }
        };

        processHand(vrcf::Hand::Offhand,
            g_config.grabFlashlightBindingOffhand,
            f4vr::getOffhandWandNode(),
            FlashlightLocation::InOffhand,
            FlashlightConfigLocation::InOffhand,
            false);

        processHand(vrcf::Hand::Primary,
            g_config.grabFlashlightBindingPrimary,
            f4vr::getPrimaryHandWandNode(),
            FlashlightLocation::InPrimaryHand,
            FlashlightConfigLocation::InPrimaryHand,
            f4vr::IsWeaponDrawn());

        if (!anyHandNear) {
            _hapticActivated = false;
        }
        return toggled;
    }

    /**
     * Fire a one-shot haptic hint the first frame a hand enters the body-grab zone (re-armed once no
     * hand is in range), so the pulse plays on zone entry rather than every frame.
     */
    void BodyFlashlight::triggerHapticOnce(const vrcf::Hand hand)
    {
        if (!_hapticActivated) {
            _hapticActivated = true;
            vrcf::VRHaptics.trigger(hand, vrcf::HapticPattern::Tick);
        }
    }
}
