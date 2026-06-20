#include "Flashlight.h"

#include "Config.h"
#include "FlashlightMod.h"
#include "Utils.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"
#include "vrcf/VRControllersHaptic.h"
#include "vrcf/VRControllersManager.h"

using namespace common;

namespace
{
    bool isAllowedToSwitchHeadAndPrimaryHand()
    {
        if (!ImFl::Utils::isHeadMountedFlashlight()) {
            // allow switching from primary hand to head with or without a weapon
            return ImFl::Utils::flashlightLocation == ImFl::FlashlightLocation::InPrimaryHand || ImFl::Utils::flashlightLocation == ImFl::FlashlightLocation::OnWeapon;
        }
        // switch to hand is only allowed if either no weapon or NOT melee weapon equipped
        return !f4vr::isNodeVisible(f4vr::getWeaponNode()) || !f4vr::isMeleeWeaponEquipped();
    }

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
}

namespace ImFl
{
    Flashlight::Flashlight()
        : _bodyGrabSphere("ImFl_BodyGrab"),
          _headSphere("ImFl_HeadActivate")
    {
        _wasInPowerArmor = f4vr::isInPowerArmor();

        // initial setup of flashlight location and values
        Utils::refreshFlashlightLocation();
        Utils::setLightValues();

        // refresh flashlight values on config change
        g_config.subscribeForIniChangedEvent("Flashlight", [](const std::string&) {
            Utils::refreshFlashlightLocation();
            Utils::toggleLightRefreshValues();
        });
    }

    /**
     * Executed every frame to update to handle flashlight location and moving between hand and head.
     */
    void Flashlight::onFrameUpdate()
    {
        handlePowerArmorTransition(Utils::isFlashlightOn());

        // Stowed-on-body model + grab/return interaction, and offhand-near-HMD head activation.
        updateBodyStow();
        checkHeadActivation();

        if (!Utils::isFlashlightOn()) {
            _inHandFlashlightMesh.onFrameUpdate(false);
            return;
        }

        Utils::refreshFlashlightLocation();

        Utils::refreshGripStyle();

        _inHandFlashlightMesh.onFrameUpdate(true);

        checkSwitchingFlashlightOnHeadHand();

        adjustFlashlightTransformToHandOrHead();

        maybeShowFPSStabilizerModWarning();
    }

    /**
     * Restore the flashlight once on a power armor enter or exit state change if it was on recently.
     * The recent-on window covers the vanilla behavior where the light may turn off shortly before the PA flag flips.
     */
    void Flashlight::handlePowerArmorTransition(bool isFlashlightOn)
    {
        const bool isInPowerArmor = f4vr::isInPowerArmor();
        if (isInPowerArmor != _wasInPowerArmor) {
            _wasInPowerArmor = isInPowerArmor;
            _bodyFlashlightMesh.invalidate();
            _bodyGrabSphere.detachDebug();
            _headSphere.detachDebug();
            const bool wasFlashlightOnRecently = isFlashlightOn || _flashlightOnRecentlyFrames > 0;
            if (wasFlashlightOnRecently && !isFlashlightOn) {
                logger::info("Restoring flashlight after power armor transition");
                Utils::refreshFlashlightLocation();
                Utils::setLightValues();
                Utils::turnFlashlightOn();
                isFlashlightOn = true;
                _inHandFlashlightMesh.invalidate(); // skeleton changed — force re-attach next frame
            }
        }
        _flashlightOnRecentlyFrames = isFlashlightOn ? 5 : max(0, _flashlightOnRecentlyFrames - 1);
    }

    /**
     * Keeps the beam-less stowed model attached to the chest bone while the feature is enabled, runs the
     * proximity-gated grab/return interaction, then shows the model only while the flashlight is off
     * (holstered). A grab/return may toggle the light mid-frame, so the visibility is set from the
     * resulting state and the caller re-reads the on/off state afterwards.
     */
    void Flashlight::updateBodyStow()
    {
        const bool enabled = g_config.showFlashlightOnBody && f4vr::getRootNode() != nullptr;
        _bodyFlashlightMesh.onFrameUpdate(enabled);

        // A grab/return may toggle the light, so set the model visibility from the resulting state.
        checkBodyGrab(enabled);

        if (enabled) {
            _bodyFlashlightMesh.setVisible(!Utils::isFlashlightOn());
        }
    }

    /**
     * Evaluate the grab (bring the light to this hand) and return (held in this hand -> off) interactions
     * for both hands against the stowed model position. Grabbing pulls the light to the grabbing hand from
     * any other state — off (turns on), head, or the other hand (stays on, switches) — while the same
     * gesture returns it (off) when it is already held in that hand. While a usable hand is within reach
     * its configured grab button is suppressed from the game/FRIK and a one-shot haptic hints the zone.
     * A hand occupied by a drawn weapon (the primary hand) can neither grab nor return the flashlight.
     * @return true if a grab or return toggled the flashlight this frame.
     */
    bool Flashlight::checkBodyGrab(const bool enabled)
    {
        // The grab zone is anchored to the stowed model on the chest bone; mirror it for left-handed players.
        const auto zoneNode = _bodyFlashlightMesh.attachedNode();
        return _bodyGrabSphere.onFrameUpdate(
            {
                .enabled = enabled && zoneNode != nullptr,
                .node = zoneNode,
                .zone = mirrorZoneIfNeeded(_bodyFlashlightMesh.grabZoneTransform()),
                .bindings = {
                    g_config.grabFlashlightByOffhandBinding,
                    !f4vr::IsWeaponDrawn() ? g_config.grabFlashlightByPrimaryHandBinding : vrcf::VRControllersManager::DisabledBinding,
                },
                .showDebug = g_config.debugShowGrabSphere,
            },
            [&](const vrcf::InputBinding& binding) {
                const auto location = f4vr::isPrimaryHand(binding.hand) ? FlashlightLocation::InPrimaryHand : FlashlightLocation::InOffhand;
                if (Utils::isFlashlightOn() && Utils::flashlightLocation == location) {
                    Utils::turnFlashlightOff();
                } else {
                    Utils::turnFlashlightOn();
                    Utils::switchFlashlightConfigLocation(f4vr::isPrimaryHand(binding.hand) ? FlashlightConfigLocation::InPrimaryHand : FlashlightConfigLocation::InOffhand);
                }
                return true;
            });
    }

    /**
     * Drives the offhand-near-HMD head activation for the current frame: runs the proximity-gated input
     * suppression + haptic and the on/switch/off toggle, and keeps the shared debug sphere in sync. May
     * toggle the flashlight, so the caller re-reads the light state afterwards.
     *
     * While the configured hand's wand is inside the head zone, suppress its configured button from the
     * game/FRIK and play a one-shot haptic; firing the binding puts the flashlight on the head — turning
     * it on from off, moving it to the head from a hand, or (when already head-mounted) turning it off.
     * The turn-on path mirrors the body grab's on-then-switch order so the head beam values get refreshed.
     */
    void Flashlight::checkHeadActivation()
    {
        _headSphere.onFrameUpdate(
            {
                .node = f4vr::getPlayerNodes()->HmdNode,
                .zone = g_config.flashlightHeadSphereTransform,
                .bindings = { g_config.activateFlashlightOnHeadBinding },
                .showDebug = g_config.debugShowGrabSphere,
            },
            [&](const vrcf::InputBinding&) {
                if (!Utils::isFlashlightOn()) {
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
                    Utils::turnFlashlightOn();
                } else if (Utils::isHeadMountedFlashlight()) {
                    Utils::turnFlashlightOff();
                } else {
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::OnHead);
                }
                return true;
            });
    }

    /**
     * Grip-based switching of the on flashlight's location. Bring a hand near the head and fire its binding
     * to swap that hand <-> head, or bring the two hands together and fire the hands binding to swap the
     * light between offhand and primary hand. Each gesture is independent and gated only by its own binding
     * (`switchFlashlightHead{Offhand,PrimaryHand}Binding`, `switchFlashlightBetweenHandsBinding`) — a disabled ("none") binding fires nothing and
     * stays silent (no proximity haptic). Runs every frame the light is on.
     */
    void Flashlight::checkSwitchingFlashlightOnHeadHand()
    {
        // check a bit higher than the HMD to allow hand close to the lower part of the face
        const auto& hmdPos = f4vr::getPlayerNodes()->HmdNode->world.translate + RE::NiPoint3(0, 0, 4);
        const auto& offhandPos = f4vr::getOffhandWandNode()->world.translate;
        const auto& primaryHandPos = f4vr::getPrimaryHandWandNode()->world.translate;

        // switch between head and offhand
        const auto isOffhandCloseToHMD = MatrixUtils::vec3Len(offhandPos - hmdPos) < 12;
        if (isOffhandCloseToHMD && g_config.switchFlashlightHeadOffhandBinding.isEnabled() &&
            (Utils::isHeadMountedFlashlight() || Utils::flashlightLocation == FlashlightLocation::InOffhand)) {
            triggerHapticOnce(vrcf::Hand::Offhand);
            if (vrcf::VRControllers.check(g_config.switchFlashlightHeadOffhandBinding)) {
                Utils::switchFlashlightConfigLocation(Utils::isHeadMountedFlashlight() ? FlashlightConfigLocation::InOffhand : FlashlightConfigLocation::OnHead);
                vrcf::VRHaptics.trigger(vrcf::Hand::Offhand, vrcf::HapticPattern::DoubleClick);
            }
            return;
        }

        // switch between head and primary hand
        const auto isPrimaryHandCloseToHMD = MatrixUtils::vec3Len(primaryHandPos - hmdPos) < 12;
        if (isPrimaryHandCloseToHMD && g_config.switchFlashlightHeadPrimaryHandBinding.isEnabled() && isAllowedToSwitchHeadAndPrimaryHand()) {
            triggerHapticOnce(vrcf::Hand::Primary);
            if (vrcf::VRControllers.check(g_config.switchFlashlightHeadPrimaryHandBinding)) {
                Utils::switchFlashlightConfigLocation(Utils::isHeadMountedFlashlight() ? FlashlightConfigLocation::InPrimaryHand : FlashlightConfigLocation::OnHead);
                vrcf::VRHaptics.trigger(vrcf::Hand::Primary, vrcf::HapticPattern::DoubleClick);
            }
            return;
        }

        // switch between offhand and primary hand
        const auto isHandsCloseToEachOther = MatrixUtils::vec3Len(primaryHandPos - offhandPos) < 12;
        if (isHandsCloseToEachOther && g_config.switchFlashlightBetweenHandsBinding.isEnabled() && !Utils::isHeadMountedFlashlight()) {
            triggerHapticOnce(vrcf::Hand::Left);
            if (vrcf::VRControllers.check(g_config.switchFlashlightBetweenHandsBinding)) {
                Utils::switchFlashlightConfigLocation(Utils::getActiveFlashlightConfigLocation() == FlashlightConfigLocation::InPrimaryHand
                        ? FlashlightConfigLocation::InOffhand
                        : FlashlightConfigLocation::InPrimaryHand);
                vrcf::VRHaptics.trigger(vrcf::Hand::Left, vrcf::HapticPattern::DoubleClick);
            }
            return;
        }

        _flashlightHapticActivated = false;
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

        if (!Utils::isHeadMountedFlashlight()) {
            // update world transforms after reverting to original
            f4vr::updateTransforms(lightNode);

            RE::NiNode* attachNode;
            RE::NiMatrix3 rotationOffset;
            RE::NiPoint3 positionOffset;
            if (Utils::flashlightLocation == FlashlightLocation::OnWeapon) {
                attachNode = f4vr::getWeaponNode();
                rotationOffset = MatrixUtils::getMatrixFromEulerAnglesDegrees(90, 0, -90);
                positionOffset = RE::NiPoint3(15.0f, 4.0f, -4.0f);
            } else {
                const bool isOffhand = Utils::flashlightLocation == FlashlightLocation::InOffhand;
                attachNode = isOffhand ? f4vr::getOffhandWandNode() : f4vr::getPrimaryHandWandNode();
                const auto& handTransform = g_config.getFlashlightInHandLightTransform(isOffhand, Utils::flashlightGripStyle, f4vr::isInPowerArmor());
                rotationOffset = handTransform.rotate;
                // not clear to me why I need to manipulate the offset this way, but it works (need to dig into it)
                positionOffset = (rotationOffset * attachNode->world.rotate).Transpose() * handTransform.translate;
            }

            // calculate relocation transform and set to local
            lightNode->local = MatrixUtils::calculateRelocation(lightNode, attachNode, positionOffset, rotationOffset);
        } else {
            const auto& headTransform = Utils::flashlightLocation == FlashlightLocation::OnPAHead ? g_config.flashlightOnPAHeadTransform : g_config.flashlightOnHeadTransform;
            lightNode->local.rotate = headTransform.rotate;
            lightNode->local.translate = headTransform.translate;
        }
    }

    void Flashlight::onGameSessionLoaded()
    {
        _inHandFlashlightMesh.invalidate(); // skeleton pointers may have changed on save load
        _bodyFlashlightMesh.invalidate();
        _bodyGrabSphere.detachDebug();
        _headSphere.detachDebug();

        logger::info("Disable game Pipboy light control");
        f4vr::getIniSetting("fPipboyLightDelay:Controls", true)->SetFloat(99);
    }

    /**
     * Fire a one-shot proximity hint when a hand enters a zone where the flashlight location can be
     * switched. Gated by `_flashlightHapticActivated` so it pulses once per zone entry, not every
     * frame.
     */
    void Flashlight::triggerHapticOnce(const vrcf::Hand hand)
    {
        if (!_flashlightHapticActivated) {
            _flashlightHapticActivated = true;
            logger::debug("Haptic triggered on hand: {}", Utils::getHandLabel(hand));
            vrcf::VRHaptics.trigger(hand, vrcf::HapticPattern::Tick);
        }
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
