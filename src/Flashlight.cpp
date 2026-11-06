#include "Flashlight.h"

#include "Config.h"
#include "FlashlightMod.h"
#include "FlashlightState.h"
#include "LiveLight.h"
#include "NpcDetectionHandler.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "WeaponGripHandler.h"
#include "common/MatrixUtils.h"
#include "devbench/DevBench.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"
#include "f4vr/WandMovement.h"
#include "vrcf/VRControllersManager.h"
#include "vrcf/VRControllersSuppressor.h"

using namespace common;

namespace
{
    /**
     * Mirrors the grab zone's Z translate for left-handed players (a sphere is rotation-invariant and
     * uniformly scaled, so its rotation/scale are unaffected), letting one authored value read for both
     * handedness modes.
     */
    RE::NiTransform mirrorZoneIfNeeded(const RE::NiTransform& zone)
    {
        RE::NiTransform mirroredZone = zone;
        const float sign = f4vr::isLeftHandedMode() ? -1.0f : 1.0f;
        mirroredZone.translate = RE::NiPoint3(zone.translate.x, zone.translate.y, sign * zone.translate.z);
        return mirroredZone;
    }

    /**
     * The other logical/physical hand (primary <-> offhand, right <-> left).
     */
    vrcf::Hand otherHand(const vrcf::Hand hand)
    {
        switch (hand) {
        case vrcf::Hand::Primary:
            return vrcf::Hand::Offhand;
        case vrcf::Hand::Offhand:
            return vrcf::Hand::Primary;
        case vrcf::Hand::Right:
            return vrcf::Hand::Left;
        case vrcf::Hand::Left:
        default:
            return vrcf::Hand::Right;
        }
    }

    /**
     * The same binding on the other hand, including a modifier pinned to a specific hand.
     */
    vrcf::InputBinding onOtherHand(const vrcf::InputBinding& binding)
    {
        auto mirrored = binding;
        mirrored.hand = otherHand(binding.hand);
        if (mirrored.modifier && mirrored.modifier->hand) {
            mirrored.modifier->hand = otherHand(*mirrored.modifier->hand);
        }
        return mirrored;
    }

    /**
     * Is there a weapon in hand for the light to mount on?
     * The beam config preview can select the on-weapon location with no weapon in hand (the same way it previews
     * the PA head out of power armor). The weapon bone is then hidden and its transform stale, so the preview
     * beam is mounted on the primary hand instead. In gameplay the weapon bone is always used.
     */
    bool hasWeaponToMountLightOn()
    {
        return !ImFl::FlashlightState::isRuntimeLocationOverrideActive() || f4vr::isNodeVisible(f4vr::getWeaponNode());
    }

    /**
     * Publish a fired gesture as the devbench "gesture" event, with where it left the light.
     */
    void emitGestureEvent(const char* gesture, const vrcf::Hand hand)
    {
        f4cf::devbench::emit("gesture", [&] {
            return nlohmann::json{
                { "gesture", gesture },
                { "hand", ImFl::Utils::getHandLabel(hand) },
                { "lightOn", ImFl::Utils::isFlashlightOn() },
                { "location", ImFl::FlashlightState::getFlashlightLocationLabel(ImFl::FlashlightState::flashlightLocation) },
            };
        });
    }
}

namespace ImFl
{
    Flashlight::Flashlight()
        : _bodyGrabSphere("ImFl_BodyGrab"),
          _headSphere("ImFl_HeadActivate"),
          _primaryHandSphere("ImFl_PrimaryHandActivate")
    {
        _wasInPowerArmor = f4vr::isInPowerArmor();

        // initial setup of flashlight location and values
        FlashlightState::refreshFlashlightLocation();
        FlashlightState::setLightValues();
        Utils::loadConfiguredGoboTextures();
        Utils::updateKeepFlashlightOnInPipboy();

        // applied above, as is every config load so far; later ones are applied by onFrameUpdate()
        g_config.consumeValuesReloaded();

        WeaponGripHandler::setWeaponTransformFinalizedListener(onWeaponTransformFinalized);
    }

    /**
     * Executed every frame to update to handle flashlight location and moving between hand and head.
     *
     * Stowed-on-body model + grab/return interaction, offhand-near-HMD head activation, the
     * offhand-near-primary-hand activation, and the zone-less two-handed weapon toggle. All may toggle the
     * light, so the on/off state is re-read below. They run before the early-return because their re-toggle
     * turns the light back on from off. The two-handed toggle runs last so it can defer to the proximity
     * gestures' suppression for the same offhand button this frame.
     */
    void Flashlight::onFrameUpdate()
    {
        F4CF_PERF_FUNCTION();

        // The config values were loaded again (a hot-reload on the file watcher's thread, or a session override):
        // applied here, on the game thread.
        if (g_config.consumeValuesReloaded()) {
            applyConfigChange();
        }
        LiveLight::onFrameUpdate();

        handlePowerArmorTransition();

        WeaponGripHandler::onFrameUpdate();

        // A game menu (workbench, container, dialogue, ...) takes the controller input, so the gestures stand down
        // and a hand resting in a zone doesn't swallow its buttons. The light itself keeps tracking its location.
        _gesturesBlockedByMenu = Utils::findOpenGestureBlockingMenu() != nullptr;

        // Before the Pip-Boy is picked up there is no flashlight: the gestures stand down and the body model is hidden.
        const bool flashlightUnavailable = !RestrictionHandler::isFlashlightAvailable();
        if (flashlightUnavailable != _flashlightUnavailable) {
            logger::info("Flashlight {}", flashlightUnavailable ? "unavailable: no Pip-Boy worn" : "available: Pip-Boy worn");
            _flashlightUnavailable = flashlightUnavailable;
        }

        updateBodyStow();
        checkHeadActivation();
        checkPrimaryHandActivation();
        checkWeaponFlashlightToggle();

        FlashlightState::refreshFlashlightLocation();

        if (RestrictionHandler::onFrameUpdate()) {
            // weapon changed
            _onWeaponBeamMesh.invalidate();
        }

        alignWandMovementToBeam();

        if (!Utils::isFlashlightOn()) {
            _inHandFlashlightMesh.onFrameUpdate(false);
            _onWeaponBeamMesh.onFrameUpdate();
            return;
        }

        FlashlightState::refreshGripStyle();

        _inHandFlashlightMesh.onFrameUpdate(true);
        _onWeaponBeamMesh.onFrameUpdate();

        adjustFlashlightTransformToHandOrHead(_inHandFlashlightMesh.getLightAnchorNode());

        NpcDetectionHandler::onFrameUpdate();

        maybeShowFPSStabilizerModWarning();
    }

    /**
     * Apply a config reload (a hot-reload from disk, a session override). Runs from onFrameUpdate() on the game thread rather than from the file
     * watcher's thread, since refreshing the light and patching the game call into the engine.
     */
    void Flashlight::applyConfigChange()
    {
        F4CF_PERF_FUNCTION();
        RestrictionHandler::invalidate();
        Utils::updateVanillaFlashlightToggleDisabled();
        Utils::updateKeepFlashlightOnInPipboy();
        FlashlightState::refreshFlashlightLocation();
        FlashlightState::refreshLightValues();
    }

    /**
     * Restore the flashlight once on a power armor enter or exit state change if it was on recently.
     * The recent-on window covers the vanilla behavior where the light may turn off shortly before the PA flag flips.
     */
    void Flashlight::handlePowerArmorTransition()
    {
        const bool isInPowerArmor = f4vr::isInPowerArmor();
        if (isInPowerArmor != _wasInPowerArmor) {
            _wasInPowerArmor = isInPowerArmor;
            _bodyFlashlightMesh.invalidate();
            RestrictionHandler::invalidate();
            _onWeaponBeamMesh.invalidate();
            _bodyGrabSphere.detachVisual();
            _headSphere.detachVisual();
            _primaryHandSphere.detachVisual();
            const bool isFlashlightOn = Utils::isFlashlightOn();
            const bool wasFlashlightOnRecently = isFlashlightOn || _flashlightOnRecentlyFrames > 0;
            if (wasFlashlightOnRecently && !isFlashlightOn) {
                logger::info("Restoring flashlight after power armor transition");
                FlashlightState::refreshFlashlightLocation();
                FlashlightState::setLightValues();
                Utils::turnFlashlightOn();
                _inHandFlashlightMesh.invalidate(); // skeleton changed — force re-attach next frame
            }
        }
        _flashlightOnRecentlyFrames = Utils::isFlashlightOn() ? 5 : (std::max)(0, _flashlightOnRecentlyFrames - 1);
    }

    /**
     * While the light is held in the offhand, has controller-direction movement follow the beam (light +X) rather than the
     * offhand controller, which an overhand grip can point backward. Not called means not corrected: the framework drops
     * the correction a frame later, so a skipped update can't leave the stick inverted. Off in gesture-blocking menus,
     * which take the stick; the Pip-Boy isn't one, as the player walks with it open.
     */
    void Flashlight::alignWandMovementToBeam() const
    {
        if (!g_config.flipReversedWandMovement || _gesturesBlockedByMenu || !Utils::isFlashlightOn() || FlashlightState::flashlightLocation != FlashlightLocation::InOffhand) {
            return;
        }
        if (const auto lightNode = f4vr::getFirstChild(f4vr::getVRPlayerNodes()->headLightParentNode)) {
            // this codebase keeps world rotations transposed: row 0 of `rotate` is where local +X (the beam) points
            const auto& rotation = lightNode->world.rotate;
            f4vr::WandMovement::alignTo("ImFl_WandMovement", RE::NiPoint3(rotation.entry[0][0], rotation.entry[0][1], rotation.entry[0][2]));
        }
    }

    /**
     * Keeps the beam-less stowed model placed at the chest bone while the feature is enabled, runs the
     * proximity-gated grab/return interaction, then shows the model except while the light is held in a hand
     * (offhand / primary hand) — the physical unit has left the body. An off, head-mounted, or weapon-mounted
     * light leaves the model stowed and visible (weapons carry their own flashlight mesh). A grab/return may
     * toggle the light mid-frame, so the visibility is set from the resulting state and the caller re-reads it.
     */
    void Flashlight::updateBodyStow()
    {
        F4CF_PERF_FUNCTION();
        const bool enabled = isBodyStowEnabled();
        _bodyFlashlightMesh.onFrameUpdate(enabled);

        // A grab/return may toggle the light, so set the model visibility from the resulting state.
        checkBodyGrab(enabled && !_gesturesBlockedByMenu);

        if (enabled) {
            const bool heldInHand = Utils::isFlashlightOn() && FlashlightState::isHandHeldFlashlight();
            _bodyFlashlightMesh.setVisible(!heldInHand);
        }
    }

    /**
     * Is the stowed body model shown, and its grab zone live? Off by config, before the Pip-Boy is worn, and without the player's 3D.
     */
    bool Flashlight::isBodyStowEnabled() const
    {
        return g_config.showFlashlightOnBody && !_flashlightUnavailable && f4vr::getRootNode() != nullptr;
    }

    /**
     * Body-stow grab/return for both hands against the belt-stowed model (see bodyGrab()). Each binding is fed
     * only in states where it acts (getBodyGrabBindings()), so its button is suppressed (with a one-shot entry
     * haptic) only then.
     * @return true if a grab or return toggled the flashlight this frame.
     */
    bool Flashlight::checkBodyGrab(const bool enabled)
    {
        const auto bindings = getBodyGrabBindings();

        // The grab zone is anchored to the stowed model on the chest bone; mirror it for left-handed players.
        const auto zoneNode = _bodyFlashlightMesh.stowBoneNode();
        return _bodyGrabSphere.onFrameUpdate(
            {
                .enabled = enabled && zoneNode != nullptr,
                .node = zoneNode,
                .zone = mirrorZoneIfNeeded(_bodyFlashlightMesh.grabZoneTransform()),
                .bindings = {
                    { .binding=bindings.primary, .activateHaptic=g_config.bodyActivation.primaryHaptic },
                    { .binding=bindings.secondary, .activateHaptic=g_config.bodyActivation.secondaryHaptic },
                },
                .entryHaptic = g_config.bodyActivation.entryHaptic,
                .showSphere = g_config.bodyActivation.showSphere,
                .showZone = g_config.showAllActivationSpheres,
                .sphereStyle = &g_config.bodyActivation.sphereStyle,
                .sphereOrientation = g_config.bodyActivation.sphereOrientation,
                .showIcon = g_config.bodyActivation.showIcon,
                .iconStyle = &g_config.bodyActivation.iconStyle,
            },
            [](const vrcf::InputBinding& binding) {
                bodyGrab(binding.hand);
                return true;
            });
    }

    /**
     * The body grab's bindings this frame: primary for the offhand, secondary for the primary hand. Never fed for
     * the hand that holds the drawn weapon. They go through WeaponGripHandler::adjustBindingForInputRemap(), so
     * the free primary hand works while the weapon rides in the offhand.
     */
    Flashlight::GestureBindings Flashlight::getBodyGrabBindings()
    {
        const bool on = Utils::isFlashlightOn();
        const auto location = FlashlightState::flashlightLocation;
        const bool weaponInPrimaryHand = f4vr::isWeaponDrawn() && !WeaponGripHandler::isPrimaryHandFreeOfWeapon();
        const bool offhandTapActive = !WeaponGripHandler::isOffhandHoldingWeapon() &&
            (location == FlashlightLocation::InOffhand || location == FlashlightLocation::OnWeapon || FlashlightState::isHeadMountedFlashlight() ||
                (location == FlashlightLocation::InPrimaryHand && !on));
        // OnWeapon is only reachable here with the primary hand freed by the offhand holding the weapon.
        const bool primaryTapActive = !weaponInPrimaryHand &&
            (location == FlashlightLocation::InPrimaryHand || location == FlashlightLocation::OnWeapon || FlashlightState::isHeadMountedFlashlight() ||
                (location == FlashlightLocation::InOffhand && !on));
        return {
            .primary = offhandTapActive ? WeaponGripHandler::adjustBindingForInputRemap(g_config.bodyActivation.primary) : vrcf::VRControllersManager::DisabledBinding,
            .secondary = primaryTapActive ? WeaponGripHandler::adjustBindingForInputRemap(g_config.bodyActivation.secondary) : vrcf::VRControllersManager::DisabledBinding,
        };
    }

    /**
     * Body grab fired by a hand: returns the light (off) when it is already held in that hand, otherwise grabs
     * it into that hand (turning it on).
     */
    void Flashlight::bodyGrab(const vrcf::Hand hand)
    {
        const auto newLocation = f4vr::isPrimaryHand(hand) ? FlashlightLocation::InPrimaryHand : FlashlightLocation::InOffhand;
        if (Utils::isFlashlightOn() && FlashlightState::flashlightLocation == newLocation) {
            logger::info("Returning flashlight to body from {} hand", Utils::getHandLabel(hand));
            Utils::turnFlashlightOff();
        } else {
            logger::info("Grabbing flashlight from body into {} hand (was {} {})",
                Utils::getHandLabel(hand),
                Utils::isFlashlightOn() ? "on" : "off",
                FlashlightState::getFlashlightLocationLabel(FlashlightState::flashlightLocation));
            FlashlightState::switchFlashlightConfigLocation(f4vr::isPrimaryHand(hand) ? FlashlightConfigLocation::InPrimaryHand : FlashlightConfigLocation::InOffhand);
            Utils::turnFlashlightOn();
        }
        emitGestureEvent("bodyGrab", hand);
    }

    /**
     * Offhand-near-HMD head activation. Inside the head zone the offhand's bound button is suppressed (with a
     * one-shot entry haptic): the tap binding puts the light on the head (headTap()) and the long-press binding
     * pulls a head-mounted light to the hand (headToHand()), each fed only in states where it acts
     * (getHeadBindings()). May toggle the light, so the caller re-reads its state.
     */
    void Flashlight::checkHeadActivation()
    {
        F4CF_PERF_FUNCTION();
        const auto bindings = getHeadBindings();

        _headSphere.onFrameUpdate(
            {
                .enabled = !_gesturesBlockedByMenu && !_flashlightUnavailable,
                .node = f4vr::getVRPlayerNodes()->hmdNode,
                .zone = g_config.headActivation.zone,
                .bindings = {
                    { .binding=bindings.primary, .activateHaptic=g_config.headActivation.primaryHaptic },
                    { .binding=bindings.secondary, .activateHaptic=g_config.headActivation.secondaryHaptic },
                },
                .entryHaptic = g_config.headActivation.entryHaptic,
                .showSphere = g_config.headActivation.showSphere,
                .showZone = g_config.showAllActivationSpheres,
                .sphereStyle = &g_config.headActivation.sphereStyle,
                .sphereOrientation = g_config.headActivation.sphereOrientation,
                .showIcon = g_config.headActivation.showIcon,
                .iconStyle = &g_config.headActivation.iconStyle,
            },
            [&](const vrcf::InputBinding& binding) {
                if (binding == bindings.secondary) {
                    headToHand(binding.hand);
                } else {
                    headTap(binding.hand);
                }
                return true;
            });
    }

    /**
     * The head activation's bindings this frame: primary the tap, secondary the long-press to the hand.
     * While the offhand holds the weapon alone (one-handed, or carrying it), the free primary hand takes the
     * gesture over with the same bindings, and the long-press lands the light in that hand.
     */
    Flashlight::GestureBindings Flashlight::getHeadBindings()
    {
        const auto gestureBinding = [](const vrcf::InputBinding& binding) {
            const auto handBinding = WeaponGripHandler::isPrimaryHandFreeOfWeapon() ? onOtherHand(binding) : binding;
            return WeaponGripHandler::adjustBindingForInputRemap(handBinding);
        };

        // Long-press head -> hand: only available while the light is on and head-mounted.
        const bool headToOffhandActive = Utils::isFlashlightOn() && FlashlightState::isHeadMountedFlashlight();

        // Gate the tap's "put on head" actions behind the headgear requirement so the gesture is inert (button
        // passes through, no entry haptic) when the head isn't allowed. Turning an already-head-mounted light
        // off via the tap stays available regardless.
        const bool headTapActive = RestrictionHandler::isHeadFlashlightAllowed() || (Utils::isFlashlightOn() && FlashlightState::isHeadMountedFlashlight());

        return {
            .primary = headTapActive ? gestureBinding(g_config.headActivation.primary) : vrcf::VRControllersManager::DisabledBinding,
            .secondary = headToOffhandActive ? gestureBinding(g_config.headActivation.secondary) : vrcf::VRControllersManager::DisabledBinding,
        };
    }

    /**
     * Head tap: puts the light on the head - on from off, switched there from a hand, or off when already head-mounted.
     */
    void Flashlight::headTap(const vrcf::Hand hand)
    {
        if (!Utils::isFlashlightOn()) {
            logger::info("Turning flashlight ON on head");
            FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
            Utils::turnFlashlightOn();
        } else if (FlashlightState::isHeadMountedFlashlight()) {
            logger::info("Turning flashlight OFF on head");
            Utils::turnFlashlightOff();
        } else {
            logger::info("Switching flashlight to head");
            FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
        }
        emitGestureEvent("head", hand);
    }

    /**
     * Head long-press: pulls the head-mounted light into the hand that pressed.
     */
    void Flashlight::headToHand(const vrcf::Hand hand)
    {
        logger::info("Switching flashlight from head to {} hand", Utils::getHandLabel(hand));
        FlashlightState::switchFlashlightConfigLocation(f4vr::isPrimaryHand(hand) ? FlashlightConfigLocation::InPrimaryHand : FlashlightConfigLocation::InOffhand);
        emitGestureEvent("head", hand);
    }

    /**
     * Offhand-near-primary-hand activation: a zone on the primary-hand wand, tested against the offhand wand.
     * The tap binding moves/toggles the light among the offhand, primary hand and weapon (primaryHandTap()), and
     * the long-press binding pulls an on-weapon light back to the offhand (weaponToOffhand()). Each binding is fed
     * only in states where it acts (getPrimaryHandBindings()), so the offhand button is suppressed (with a one-shot
     * entry haptic) only then. Runs before the on/off early-return so the on-weapon turn-on works from off.
     */
    void Flashlight::checkPrimaryHandActivation()
    {
        F4CF_PERF_FUNCTION();
        const auto bindings = getPrimaryHandBindings();

        auto zone = g_config.primaryHandActivation.zone;

        // Anchor the sphere to the weapon's flashlight mesh when configured and one is mounted (the node is
        // non-null only while the weapon-flashlight requirement is on), so the gesture is reached at the gun
        // lamp; otherwise it sits on the primary-hand wand. A non-node mesh falls back to the wand.
        auto sphereNode = f4vr::getPrimaryHandWandNode();
        if (g_config.weaponFlashlightAnchorPrimaryHandSphereToMesh) {
            const auto [meshNode, meshTransform] = RestrictionHandler::getOnWeaponFlashlightMeshNode();
            if (meshNode) {
                sphereNode = meshNode->parent;
                zone = meshTransform;
                zone.scale = g_config.primaryHandActivation.zone.scale;
            }
        }

        _primaryHandSphere.onFrameUpdate(
            {
                .enabled = !_gesturesBlockedByMenu && !_flashlightUnavailable,
                .node = sphereNode,
                .zone = zone,
                .bindings = {
                    { .binding=bindings.primary, .activateHaptic=g_config.primaryHandActivation.primaryHaptic },
                    { .binding=bindings.secondary, .activateHaptic=g_config.primaryHandActivation.secondaryHaptic },
                },
                .entryHaptic = g_config.primaryHandActivation.entryHaptic,
                .showSphere = g_config.primaryHandActivation.showSphere,
                .showZone = g_config.showAllActivationSpheres,
                .sphereStyle = &g_config.primaryHandActivation.sphereStyle,
                .sphereOrientation = g_config.primaryHandActivation.sphereOrientation,
                .showIcon = g_config.primaryHandActivation.showIcon,
                .iconStyle = &g_config.primaryHandActivation.iconStyle,
            },
            [&](const vrcf::InputBinding& binding) {
                if (binding == bindings.secondary) {
                    weaponToOffhand(binding.hand);
                } else {
                    primaryHandTap(binding.hand);
                }
                return true;
            });
    }

    /**
     * The primary-hand activation's bindings this frame: primary the tap, secondary the long-press.
     * A melee/unarmed weapon, or the light off with no weapon to carry it, feeds neither. Neither is fed while
     * the offhand is on the weapon - holding it (firing or carrying) or supporting it in a two-handed grip: the
     * bindings are then that hand's, and a sphere anchored to the weapon's lamp would always contain the offhand
     * and swallow its button. A two-handed grip has its own zone-less toggle (checkWeaponFlashlightToggle()).
     */
    Flashlight::GestureBindings Flashlight::getPrimaryHandBindings()
    {
        const bool offhandOnWeapon = WeaponGripHandler::isOffhandHoldingWeapon() || WeaponGripHandler::isTwoHandedGripActive();
        const bool on = Utils::isFlashlightOn();
        const auto location = FlashlightState::flashlightLocation;

        const bool weaponDrawn = f4vr::isWeaponDrawn();
        const bool weaponCanHoldLight = RestrictionHandler::isWeaponFlashlightAllowed();
        const bool primaryHandUsable = !RestrictionHandler::isWeaponEquipped() || weaponCanHoldLight;

        // Tap binding, in two halves: with the light on it moves/toggles it among the offhand, primary hand,
        // and weapon (a head-mounted light needs a drawn weapon to move to); with the light off it only turns
        // on at the weapon, so hands-together with no weapon to carry the light is inert (button passes
        // through) and never lights an empty hand.
        const bool tapMoveActive = on && primaryHandUsable &&
            (location == FlashlightLocation::OnWeapon || location == FlashlightLocation::InPrimaryHand || location == FlashlightLocation::InOffhand ||
                (FlashlightState::isHeadMountedFlashlight() && weaponDrawn));
        const bool tapTurnOnActive = !on && weaponCanHoldLight;
        const bool tapActive = !offhandOnWeapon && (tapMoveActive || tapTurnOnActive);

        // Long-press binding: only pulls the on-weapon light back to the offhand.
        const bool weaponToOffhandActive = !offhandOnWeapon && on && location == FlashlightLocation::OnWeapon;

        return {
            .primary = tapActive ? WeaponGripHandler::adjustBindingForInputRemap(g_config.primaryHandActivation.primary) : vrcf::VRControllersManager::DisabledBinding,
            .secondary =
                weaponToOffhandActive ? WeaponGripHandler::adjustBindingForInputRemap(g_config.primaryHandActivation.secondary) : vrcf::VRControllersManager::DisabledBinding,
        };
    }

    /**
     * Primary-hand tap: offhand -> primary hand (empty) or -> weapon (weapon drawn), primary hand -> offhand,
     * head -> weapon, weapon -> off. From off it turns the light on at the weapon, the only place it is fed from off.
     */
    void Flashlight::primaryHandTap(const vrcf::Hand hand)
    {
        if (!Utils::isFlashlightOn()) {
            logger::info("Turning flashlight ON on weapon");
            FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InPrimaryHand);
            Utils::turnFlashlightOn();
        } else if (FlashlightState::flashlightLocation == FlashlightLocation::OnWeapon) {
            logger::info("Turning flashlight OFF on weapon");
            Utils::turnFlashlightOff();
        } else if (FlashlightState::flashlightLocation == FlashlightLocation::InPrimaryHand) {
            logger::info("Switching flashlight from primary hand to offhand");
            FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
        } else {
            // On the offhand or head: move to the primary hand (empty) or the weapon (weapon drawn).
            logger::info("Switching flashlight to primary hand");
            FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InPrimaryHand);
        }
        emitGestureEvent("primaryHand", hand);
    }

    /**
     * Primary-hand long-press: pulls the on-weapon light back to the offhand.
     */
    void Flashlight::weaponToOffhand(const vrcf::Hand hand)
    {
        logger::info("Switching flashlight from weapon to offhand");
        FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
        emitGestureEvent("primaryHand", hand);
    }

    /**
     * Is the two-handed weapon toggle live this frame? While the offhand supports a two-handed weapon (see
     * WeaponGripHandler) - not while it is the firing hand, whose button would toggle the light on every shot -
     * and the light is / would be on the weapon (the runtime location resolves to OnWeapon and the weapon may
     * carry the light).
     */
    bool Flashlight::isWeaponToggleActive() const
    {
        return !_gesturesBlockedByMenu && !_flashlightUnavailable && WeaponGripHandler::isTwoHandedGripActive() && !WeaponGripHandler::isWeaponInOffhand() &&
            g_config.toggleWeaponFlashlightTwoHandedBinding.isEnabled() && FlashlightState::flashlightLocation == FlashlightLocation::OnWeapon &&
            RestrictionHandler::isWeaponFlashlightAllowed();
    }

    /**
     * Zone-less offhand toggle of the weapon-mounted light, for two-handed weapon holds where the offhand
     * grips the foregrip and can't reach the primary-hand activation sphere. Fires while isWeaponToggleActive()
     * and no proximity zone is already claiming the same offhand input this frame (so a shared button isn't
     * handled twice).
     */
    void Flashlight::checkWeaponFlashlightToggle() const
    {
        F4CF_PERF_FUNCTION();
        if (!isWeaponToggleActive()) {
            return;
        }

        const auto& binding = g_config.toggleWeaponFlashlightTwoHandedBinding;
        if (_bodyGrabSphere.isSuppressing(binding) || _headSphere.isSuppressing(binding) || _primaryHandSphere.isSuppressing(binding)) {
            return;
        }

        if (vrcf::VRControllers.check(binding)) {
            weaponToggle(binding.hand);
        }
    }

    /**
     * Two-handed toggle: turns the weapon light on or off.
     */
    void Flashlight::weaponToggle(const vrcf::Hand hand)
    {
        logger::info("Toggle weapon flashlight in two-handed grip");
        f4vr::togglePipboyLight(f4vr::getPlayer());
        emitGestureEvent("weaponToggle", hand);
    }

    /**
     * Fire a gesture for devbench as if its binding were pressed inside its zone: the same state gating and
     * action as the frame's own detection, without the zone, the button or a haptic. Game thread.
     * @return nullptr when it fired, otherwise why it is inert in the current state.
     */
    const char* Flashlight::simulateGesture(const SimulatedGesture gesture)
    {
        if (_flashlightUnavailable) {
            return "no flashlight: the Pip-Boy isn't worn";
        }
        if (_gesturesBlockedByMenu) {
            return "a game menu blocks the gestures";
        }
        constexpr auto NOT_FED = "not fed in this state, or its binding is unset";

        // the binding the frame would feed for this gesture, and the action it fires
        auto binding = vrcf::VRControllersManager::DisabledBinding;
        void (*action)(vrcf::Hand) = nullptr;
        switch (gesture) {
        case SimulatedGesture::BodyGrabOffhand:
        case SimulatedGesture::BodyGrabPrimaryHand: {
            if (!isBodyStowEnabled() || !_bodyFlashlightMesh.stowBoneNode()) {
                return "the body stow is off (bShowFlashlightOnBody) or not placed yet";
            }
            const auto bindings = getBodyGrabBindings();
            binding = gesture == SimulatedGesture::BodyGrabOffhand ? bindings.primary : bindings.secondary;
            action = &bodyGrab;
            break;
        }
        case SimulatedGesture::HeadTap:
            binding = getHeadBindings().primary;
            action = &headTap;
            break;
        case SimulatedGesture::HeadLongPress:
            binding = getHeadBindings().secondary;
            action = &headToHand;
            break;
        case SimulatedGesture::PrimaryHandTap:
            binding = getPrimaryHandBindings().primary;
            action = &primaryHandTap;
            break;
        case SimulatedGesture::PrimaryHandLongPress:
            binding = getPrimaryHandBindings().secondary;
            action = &weaponToOffhand;
            break;
        case SimulatedGesture::WeaponToggle:
            if (isWeaponToggleActive()) {
                binding = g_config.toggleWeaponFlashlightTwoHandedBinding;
            }
            action = &weaponToggle;
            break;
        }

        if (!binding.isEnabled()) {
            return NOT_FED;
        }
        action(binding.hand);
        return nullptr;
    }

    /**
     * Adjust the position of the light node to the hand that is holding it or revert to head position.
     * It is safer than moving the node as that can result in game crash.
     *
     * Per-location pose comes from the configured `tFlashlight*Transform` values; no hardcoded
     * tilt/offset constants live here. Offhand and primary hand have independent transforms.
     *
     * While the hand-held model is shown, `inHandLightAnchor` is its lens node and the light is rooted there, so it
     * leaves the model along its beam glow; the configured in-hand light transforms (set against the wand, not the
     * model) apply only while the model isn't shown. The weapon lamp is rooted the same way.
     */
    void Flashlight::adjustFlashlightTransformToHandOrHead(const RE::NiAVObject* inHandLightAnchor)
    {
        F4CF_PERF_FUNCTION();
        const auto lightNode = f4vr::getFirstChild(f4vr::getVRPlayerNodes()->headLightParentNode);
        if (!lightNode) {
            return;
        }

        // revert to original transform
        lightNode->local.rotate = MatrixUtils::getIdentityMatrix();
        lightNode->local.translate = RE::NiPoint3(0, 0, 0);

        if (!FlashlightState::isHeadMountedFlashlight()) {
            // update world transforms after reverting to original
            f4vr::updateTransforms(lightNode);

            const RE::NiAVObject* attachNode;
            RE::NiMatrix3 rotationOffset;
            RE::NiPoint3 positionOffset;
            if (FlashlightState::flashlightLocation == FlashlightLocation::OnWeapon && hasWeaponToMountLightOn()) {
                if (auto* meshNode = RestrictionHandler::getOnWeaponFlashlightMeshNode().first) {
                    // Beam-to-mesh rooting on and a modeled flashlight found: root the beam at that mesh node
                    // with the configured mount offset instead of the generic tuned barrel guess.
                    attachNode = meshNode;
                    rotationOffset = g_config.weaponFlashlightMountTransform.rotate;
                    positionOffset = g_config.weaponFlashlightMountTransform.translate;
                } else {
                    attachNode = f4vr::getWeaponNode();
                    rotationOffset = MatrixUtils::getMatrixFromEulerAnglesDegrees(90, 0, -90);
                    positionOffset = RE::NiPoint3(15.0f, 4.0f, -4.0f);
                }
            } else if (inHandLightAnchor) {
                // The hand-held model is shown: its lens node's +X is the beam axis, as the light's is.
                attachNode = inHandLightAnchor;
                rotationOffset = MatrixUtils::getIdentityMatrix();
                positionOffset = RE::NiPoint3(0, 0, 0);
            } else {
                // In-hand without the model, or the on-weapon config preview with no weapon in hand (see hasWeaponToMountLightOn).
                const bool isOffhand = FlashlightState::flashlightLocation == FlashlightLocation::InOffhand;
                attachNode = isOffhand ? f4vr::getOffhandWandNode() : f4vr::getPrimaryHandWandNode();
                const auto& handTransform = g_config.getFlashlightInHandLightTransform(isOffhand, FlashlightState::flashlightGripStyle);
                rotationOffset = handTransform.rotate;
                // not clear to me why I need to manipulate the offset this way, but it works (need to dig into it)
                positionOffset = (rotationOffset * attachNode->world.rotate).Transpose() * handTransform.translate;
            }

            // calculate relocation transform and set to local
            lightNode->local = MatrixUtils::calculateRelocation(lightNode, attachNode, positionOffset, rotationOffset);
        } else {
            const auto& headTransform =
                FlashlightState::flashlightLocation == FlashlightLocation::OnPAHead ? g_config.flashlightOnPAHeadTransform : g_config.flashlightOnHeadTransform;
            lightNode->local.rotate = headTransform.rotate;
            lightNode->local.translate = headTransform.translate;
        }
    }

    /**
     * A weapon-handling mod wrote the final weapon transform after onFrameUpdate() ran (see WeaponGripHandler).
     * Re-apply the on-weapon light on that pose, otherwise it follows the pre-solve weapon (the primary hand)
     * while e.g. a two-handed rifle points between both hands. Other locations aren't anchored to the weapon.
     * The light's world transform is pushed right away: this runs late in the frame, closer to rendering.
     */
    void Flashlight::onWeaponTransformFinalized()
    {
        F4CF_PERF_FUNCTION();
        if (FlashlightState::flashlightLocation != FlashlightLocation::OnWeapon || !Utils::isFlashlightOn() || !f4vr::getWeaponNode()) {
            return;
        }
        // on-weapon only, so no in-hand anchor
        adjustFlashlightTransformToHandOrHead(nullptr);
        if (const auto lightNode = f4vr::getFirstChild(f4vr::getVRPlayerNodes()->headLightParentNode)) {
            f4vr::updateTransforms(lightNode);
        }
    }

    void Flashlight::onGameSessionLoaded()
    {
        _inHandFlashlightMesh.invalidate();
        _onWeaponBeamMesh.invalidate();
        _bodyFlashlightMesh.invalidate();
        _bodyGrabSphere.detachVisual();
        _headSphere.detachVisual();
        _primaryHandSphere.detachVisual();
        // The framework reset all input suppression before this hook, so forget what we believed we suppressed.
        RestrictionHandler::invalidate();
        Utils::updateVanillaFlashlightToggleDisabled();
    }

    /**
     * VR FPS Stabilizer mod lowers the quality of shadows. It causes the flashlight shadows to be funky, which can be confusing for users as it looks
     * like a bug or conflict with this mod. Warn the user about it if they have the mod installed and flashlight shadows enabled in config.
     */
    void Flashlight::maybeShowFPSStabilizerModWarning()
    {
        if (!g_config.warnAboutFPSStabilizerMod || !Utils::areFlashlightShadowsEnabled()) {
            return;
        }
        if (!isNowTimePassed(_lastVRFPSStabilizerWarningTime, 5 * 60 * 1000)) {
            return;
        }

        if (Utils::isVRFPSStabilizerModInstalled()) {
            f4vr::showNotification(
                "Warning: VR FPS Stabilizer mod detected.\nIt conflicts with flashlight shadows.\nDisable the mod or turn shadows off in Immersive Flashlight VR config.");
            _lastVRFPSStabilizerWarningTime = nowMillis();
        }
    }
}
