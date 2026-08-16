#include "Flashlight.h"

#include "Config.h"
#include "FlashlightMod.h"
#include "FlashlightState.h"
#include "NpcDetectionHandler.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"
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
     * Is there a weapon in hand for the light to mount on?
     * The beam config preview can select the on-weapon location with no weapon in hand (the same way it previews
     * the PA head out of power armor). The weapon bone is then hidden and its transform stale, so the preview
     * beam is mounted on the primary hand instead. In gameplay the weapon bone is always used.
     */
    bool hasWeaponToMountLightOn()
    {
        return !ImFl::FlashlightState::isRuntimeLocationOverrideActive() || f4vr::isNodeVisible(f4vr::getWeaponNode());
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

        // refresh flashlight values on config change
        g_config.subscribeForIniChangedEvent("Flashlight", [](const std::string&) {
            RestrictionHandler::invalidate();
            Utils::updateVanillaFlashlightToggleDisabled();
            FlashlightState::refreshFlashlightLocation();
            FlashlightState::toggleLightRefreshValues();
        });
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
        handlePowerArmorTransition();

        updateBodyStow();
        checkHeadActivation();
        checkPrimaryHandActivation();
        checkWeaponFlashlightToggle();

        FlashlightState::refreshFlashlightLocation();

        if (RestrictionHandler::onFrameUpdate()) {
            // weapon changed
            _onWeaponBeamMesh.invalidate();
        }

        if (!Utils::isFlashlightOn()) {
            _inHandFlashlightMesh.onFrameUpdate(false);
            _onWeaponBeamMesh.onFrameUpdate();
            return;
        }

        FlashlightState::refreshGripStyle();

        _inHandFlashlightMesh.onFrameUpdate(true);
        _onWeaponBeamMesh.onFrameUpdate();

        adjustFlashlightTransformToHandOrHead();

        NpcDetectionHandler::onFrameUpdate();

        maybeShowFPSStabilizerModWarning();
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
        _flashlightOnRecentlyFrames = Utils::isFlashlightOn() ? 5 : max(0, _flashlightOnRecentlyFrames - 1);
    }

    /**
     * Keeps the beam-less stowed model attached to the chest bone while the feature is enabled, runs the
     * proximity-gated grab/return interaction, then shows the model except while the light is held in a hand
     * (offhand / primary hand) — the physical unit has left the body. An off, head-mounted, or weapon-mounted
     * light leaves the model stowed and visible (weapons carry their own flashlight mesh). A grab/return may
     * toggle the light mid-frame, so the visibility is set from the resulting state and the caller re-reads it.
     */
    void Flashlight::updateBodyStow()
    {
        const bool enabled = g_config.showFlashlightOnBody && f4vr::getRootNode() != nullptr;
        _bodyFlashlightMesh.onFrameUpdate(enabled);

        // A grab/return may toggle the light, so set the model visibility from the resulting state.
        checkBodyGrab(enabled);

        if (enabled) {
            const bool heldInHand = Utils::isFlashlightOn() && FlashlightState::isHandHeldFlashlight();
            _bodyFlashlightMesh.setVisible(!heldInHand);
        }
    }

    /**
     * Body-stow grab/return for both hands against the chest-stowed model: firing a hand's grab binding
     * returns the light (off) when it is already held in that hand, otherwise grabs it into that hand
     * (turning it on). Each binding is fed only in states where it acts — and the primary hand never while
     * it holds a drawn weapon — so its button is suppressed (with a one-shot entry haptic) only then.
     * @return true if a grab or return toggled the flashlight this frame.
     */
    bool Flashlight::checkBodyGrab(const bool enabled)
    {
        const bool on = Utils::isFlashlightOn();
        const auto location = FlashlightState::flashlightLocation;
        const bool offhandTapActive = location == FlashlightLocation::InOffhand || location == FlashlightLocation::OnWeapon || FlashlightState::isHeadMountedFlashlight() ||
            (location == FlashlightLocation::InPrimaryHand && !on);
        const bool primaryTapActive = !f4vr::isWeaponDrawn() &&
            (location == FlashlightLocation::InPrimaryHand || FlashlightState::isHeadMountedFlashlight() || (location == FlashlightLocation::InOffhand && !on));

        // The grab zone is anchored to the stowed model on the chest bone; mirror it for left-handed players.
        const auto zoneNode = _bodyFlashlightMesh.attachedNode();
        return _bodyGrabSphere.onFrameUpdate(
            {
                .enabled = enabled && zoneNode != nullptr,
                .node = zoneNode,
                .zone = mirrorZoneIfNeeded(_bodyFlashlightMesh.grabZoneTransform()),
                .bindings = {
                    { .binding=offhandTapActive ? g_config.bodyActivation.primary : vrcf::VRControllersManager::DisabledBinding, .activateHaptic=g_config.bodyActivation.primaryHaptic },
                    { .binding=primaryTapActive ? g_config.bodyActivation.secondary : vrcf::VRControllersManager::DisabledBinding, .activateHaptic=g_config.bodyActivation.secondaryHaptic },
                },
                .entryHaptic = g_config.bodyActivation.entryHaptic,
                .showSphere = g_config.showAllActivationSpheres ? f4vr::ActivationSphereVisibility::Always : g_config.bodyActivation.showSphere,
                .sphereNif = g_config.bodyActivation.sphereNif,
                .sphereScale = g_config.bodyActivation.sphereScale,
            },
            [&](const vrcf::InputBinding& binding) {
                const auto newLocation = f4vr::isPrimaryHand(binding.hand) ? FlashlightLocation::InPrimaryHand : FlashlightLocation::InOffhand;
                if (Utils::isFlashlightOn() && FlashlightState::flashlightLocation == newLocation) {
                    logger::info("Returning flashlight to body from {} hand", Utils::getHandLabel(binding.hand));
                    Utils::turnFlashlightOff();
                } else {
                    logger::info("Grabbing flashlight from body into {} hand", Utils::getHandLabel(binding.hand));
                    Utils::turnFlashlightOn();
                    FlashlightState::switchFlashlightConfigLocation(f4vr::isPrimaryHand(binding.hand) ? FlashlightConfigLocation::InPrimaryHand : FlashlightConfigLocation::InOffhand);
                }
                return true;
            });
    }

    /**
     * Offhand-near-HMD head activation. Inside the head zone the offhand's bound button is suppressed (with a
     * one-shot entry haptic): the tap binding puts the light on the head — on from off, switched there from a
     * hand, or off when already head-mounted — and the long-press binding, fed only while the light is on and
     * head-mounted, pulls it to the offhand. May toggle the light, so the caller re-reads its state.
     */
    void Flashlight::checkHeadActivation()
    {
        // Long-press head -> offhand: only available while the light is on and head-mounted.
        const bool headToOffhandActive = Utils::isFlashlightOn() && FlashlightState::isHeadMountedFlashlight();

        // Gate the tap's "put on head" actions behind the headgear requirement so the gesture is inert (button
        // passes through, no entry haptic) when the head isn't allowed. Turning an already-head-mounted light
        // off via the tap stays available regardless.
        const bool headTapActive = RestrictionHandler::isHeadFlashlightAllowed() || (Utils::isFlashlightOn() && FlashlightState::isHeadMountedFlashlight());

        _headSphere.onFrameUpdate(
            {
                .node = f4vr::getPlayerNodes()->HmdNode,
                .zone = g_config.headActivation.zone,
                .bindings = {
                    { .binding=headTapActive ? g_config.headActivation.primary : vrcf::VRControllersManager::DisabledBinding, .activateHaptic=g_config.headActivation.primaryHaptic },
                    { .binding=headToOffhandActive ? g_config.headActivation.secondary : vrcf::VRControllersManager::DisabledBinding, .activateHaptic=g_config.headActivation.secondaryHaptic },
                },
                .entryHaptic = g_config.headActivation.entryHaptic,
                .showSphere = g_config.showAllActivationSpheres ? f4vr::ActivationSphereVisibility::Always : g_config.headActivation.showSphere,
                .sphereNif = g_config.headActivation.sphereNif,
                .sphereScale = g_config.headActivation.sphereScale,
            },
            [&](const vrcf::InputBinding& binding) {
                // Long-press head -> offhand (headActivation.secondary; only fed while the light is head-mounted).
                if (binding == g_config.headActivation.secondary) {
                    logger::info("Switching flashlight from head to offhand");
                    FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
                    return true;
                }
                // Tap binding (headActivation.primary).
                if (!Utils::isFlashlightOn()) {
                    logger::info("Turning flashlight ON on head");
                    Utils::turnFlashlightOn();
                    FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
                } else if (FlashlightState::isHeadMountedFlashlight()) {
                    logger::info("Turning flashlight OFF on head");
                    Utils::turnFlashlightOff();
                } else {
                    logger::info("Switching flashlight to head");
                    FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
                }
                return true;
            });
    }

    /**
     * Offhand-near-primary-hand activation: a zone on the primary-hand wand, tested against the offhand wand.
     * The tap binding moves/toggles a light that is already on among the offhand, primary hand, and weapon (and
     * pulls a head-mounted light onto a drawn weapon); the long-press binding, fed only while the light is on
     * the weapon, pulls it back to the offhand. Each binding is fed only in states where it acts (the light off
     * or a melee/unarmed weapon is inert), so the offhand button is suppressed (with a one-shot entry haptic)
     * only then. May toggle the light off, so the caller re-reads its state.
     */
    void Flashlight::checkPrimaryHandActivation()
    {
        const bool on = Utils::isFlashlightOn();
        const auto location = FlashlightState::flashlightLocation;

        const bool weaponDrawn = f4vr::isWeaponDrawn();
        const bool primaryHandUsable = !RestrictionHandler::isWeaponEquipped() || RestrictionHandler::isWeaponFlashlightAllowed();

        // Tap binding: only moves/toggles a light that is already on. With the light off the gesture is inert
        // (the button passes through) — bringing the hands together never turns the light on.
        const bool tapActive = on && primaryHandUsable &&
            (location == FlashlightLocation::OnWeapon || location == FlashlightLocation::InPrimaryHand || location == FlashlightLocation::InOffhand ||
                (FlashlightState::isHeadMountedFlashlight() && weaponDrawn));

        // Long-press binding: only pulls the on-weapon light back to the offhand.
        const bool weaponToOffhandActive = on && location == FlashlightLocation::OnWeapon;

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
                .node = sphereNode,
                .zone = zone,
                .bindings = {
                    { .binding=tapActive ? g_config.primaryHandActivation.primary : vrcf::VRControllersManager::DisabledBinding, .activateHaptic=g_config.primaryHandActivation.primaryHaptic },
                    { .binding=weaponToOffhandActive ? g_config.primaryHandActivation.secondary : vrcf::VRControllersManager::DisabledBinding, .activateHaptic=g_config.primaryHandActivation.secondaryHaptic },
                },
                .entryHaptic = g_config.primaryHandActivation.entryHaptic,
                .showSphere = g_config.showAllActivationSpheres ? f4vr::ActivationSphereVisibility::Always : g_config.primaryHandActivation.showSphere,
                .sphereNif = g_config.primaryHandActivation.sphereNif,
                .sphereScale = g_config.primaryHandActivation.sphereScale,
            },
            [&](const vrcf::InputBinding& binding) {
                // Long-press weapon -> offhand (primaryHandActivation.secondary; only fed while on the weapon).
                if (binding == g_config.primaryHandActivation.secondary) {
                    logger::info("Switching flashlight from weapon to offhand");
                    FlashlightState::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
                    return true;
                }
                // Tap binding: fed only while the light is on, so there is no turn-on path here.
                if (FlashlightState::flashlightLocation == FlashlightLocation::OnWeapon) {
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
                return true;
            });
    }

    /**
     * Zone-less offhand toggle of the weapon-mounted light, for two-handed weapon holds where the offhand
     * grips the foregrip and can't reach the primary-hand activation sphere. Active only while the offhand is
     * gripping the weapon (FRIK), the light is / would be on the weapon (the runtime location resolves to
     * OnWeapon and the weapon may carry the light), and no proximity zone is already claiming the same offhand
     * input this frame (so a shared button isn't handled twice).
     */
    void Flashlight::checkWeaponFlashlightToggle() const
    {
        if (!frik::api::FRIKApi::inst || !frik::api::FRIKApi::inst->isOffHandGrippingWeapon()) {
            return;
        }

        const auto& binding = g_config.toggleWeaponFlashlightTwoHandedBinding;
        if (!binding.isEnabled()) {
            return;
        }

        if (FlashlightState::flashlightLocation != FlashlightLocation::OnWeapon || !RestrictionHandler::isWeaponFlashlightAllowed()) {
            return;
        }

        if (_bodyGrabSphere.isSuppressing(binding) || _headSphere.isSuppressing(binding) || _primaryHandSphere.isSuppressing(binding)) {
            return;
        }

        if (vrcf::VRControllers.check(binding)) {
            logger::info("Toggle weapon flashlight in two-handed grip");
            f4vr::togglePipboyLight(f4vr::getPlayer());
        }
    }

    /**
     * Adjust the position of the light node to the hand that is holding it or revert to head position.
     * It is safer than moving the node as that can result in game crash.
     *
     * Per-location pose comes from the configured `tFlashlight*Transform` values; no hardcoded
     * tilt/offset constants live here. Offhand and primary hand have independent transforms.
     */
    void Flashlight::adjustFlashlightTransformToHandOrHead()
    {
        const auto lightNode = f4vr::getFirstChild(f4vr::getPlayerNodes()->HeadLightParentNode);
        if (!lightNode) {
            return;
        }

        // revert to original transform
        lightNode->local.rotate = MatrixUtils::getIdentityMatrix();
        lightNode->local.translate = RE::NiPoint3(0, 0, 0);

        if (!FlashlightState::isHeadMountedFlashlight()) {
            // update world transforms after reverting to original
            f4vr::updateTransforms(lightNode);

            RE::NiAVObject* attachNode;
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
            } else {
                // In-hand, or the on-weapon config preview with no weapon in hand (see hasWeaponToMountLightOn).
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
