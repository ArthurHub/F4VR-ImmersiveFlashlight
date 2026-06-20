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
          _headSphere("ImFl_HeadActivate"),
          _primaryHandSphere("ImFl_PrimaryHandActivate")
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

        // Stowed-on-body model + grab/return interaction, offhand-near-HMD head activation, and the
        // offhand-near-primary-hand activation. All three may toggle the light, so the on/off state is
        // re-read below. The primary-hand check runs before the early-return because its re-toggle turns
        // the on-weapon light back on from off.
        updateBodyStow();
        checkHeadActivation();
        checkPrimaryHandActivation();

        if (!Utils::isFlashlightOn()) {
            _inHandFlashlightMesh.onFrameUpdate(false);
            return;
        }

        Utils::refreshFlashlightLocation();

        Utils::refreshGripStyle();

        _inHandFlashlightMesh.onFrameUpdate(true);

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
            _primaryHandSphere.detachDebug();
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
     * suppression + haptic and the toggle/switch, and keeps the shared debug sphere in sync. May toggle
     * the flashlight, so the caller re-reads the light state afterwards.
     *
     * While the offhand wand is inside the head zone, suppress its configured button from the game/FRIK and
     * play a one-shot haptic. The tap binding puts the flashlight on the head — turning it on from off, moving
     * it to the head from a hand, or (when already head-mounted) turning it off; the turn-on path mirrors the
     * body grab's on-then-switch order so the head beam values get refreshed. The long-press binding pulls the
     * head-mounted light to the offhand and is fed only while the light is on and head-mounted, so the offhand
     * button is suppressed (and the entry haptic fires) for it only when that gesture is available.
     */
    void Flashlight::checkHeadActivation()
    {
        // Long-press head -> offhand: only available while the light is on and head-mounted.
        const bool headToOffhandActive = Utils::isFlashlightOn() && Utils::isHeadMountedFlashlight();

        _headSphere.onFrameUpdate(
            {
                .node = f4vr::getPlayerNodes()->HmdNode,
                .zone = g_config.flashlightHeadSphereTransform,
                .bindings = {
                    g_config.activateFlashlightOnHeadBinding,
                    headToOffhandActive ? g_config.switchFlashlightFromHeadToOffhandBinding : vrcf::VRControllersManager::DisabledBinding,
                },
                .showDebug = g_config.debugShowGrabSphere,
            },
            [&](const vrcf::InputBinding& binding) {
                // Long-press head -> offhand (only fed while the light is head-mounted).
                if (binding == g_config.switchFlashlightFromHeadToOffhandBinding) {
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
                    return true;
                }
                // Tap binding.
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
     * Drives the offhand-near-primary-hand activation sphere for the current frame. The zone is anchored to
     * the primary-hand wand node; bringing the offhand wand into it and firing the binding moves the on
     * flashlight between the offhand and the primary hand / weapon and toggles the on-weapon light on/off,
     * while a separate long-press binding pulls the on-weapon light back to the offhand.
     *
     * Each binding is fed to the sphere only in the states where it actually does something, so the offhand
     * button is suppressed (and the entry haptic fires) only when the gesture is available — e.g. nothing
     * happens, and the button passes through, when the primary hand holds a melee/unarmed weapon. Runs every
     * frame, before the on/off early-return, because the on-weapon re-toggle turns the light back on from off.
     */
    void Flashlight::checkPrimaryHandActivation()
    {
        const bool on = Utils::isFlashlightOn();
        const auto location = Utils::flashlightLocation;
        const bool weaponDrawn = f4vr::IsWeaponDrawn();
        const bool regularWeapon = weaponDrawn && !f4vr::isMeleeWeaponEquipped() && !f4vr::isUnarmedWeaponEquipped();

        // Tap binding: with the light on, acts whenever it can move or toggle it — on the weapon (toggle off),
        // on the primary hand (move to offhand), or on the offhand with the primary hand empty or holding a
        // regular weapon (a melee/unarmed weapon is inert). With the light off, a drawn regular weapon re-
        // toggles the on-weapon light back on.
        const bool tapActive = on ? (location == FlashlightLocation::OnWeapon || location == FlashlightLocation::InPrimaryHand ||
                                        (location == FlashlightLocation::InOffhand && (!weaponDrawn || regularWeapon)))
                                  : regularWeapon;

        // Long-press binding: only pulls the on-weapon light back to the offhand.
        const bool weaponToOffhandActive = on && location == FlashlightLocation::OnWeapon;

        _primaryHandSphere.onFrameUpdate(
            {
                .node = f4vr::getPrimaryHandWandNode(),
                .zone = g_config.flashlightPrimaryHandSphereTransform,
                .bindings = {
                    tapActive ? g_config.activateFlashlightOnPrimaryHandBinding : vrcf::VRControllersManager::DisabledBinding,
                    weaponToOffhandActive ? g_config.switchFlashlightFromWeaponToOffhandBinding : vrcf::VRControllersManager::DisabledBinding,
                },
                .showDebug = g_config.debugShowGrabSphere,
            },
            [&](const vrcf::InputBinding& binding) {
                // Long-press weapon -> offhand (only fed while the light is on the weapon).
                if (binding == g_config.switchFlashlightFromWeaponToOffhandBinding) {
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
                    return true;
                }
                // Tap binding.
                if (!Utils::isFlashlightOn()) {
                    // Re-toggle the on-weapon light back on (only fed with a regular weapon drawn).
                    Utils::turnFlashlightOn();
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::InPrimaryHand);
                } else if (Utils::flashlightLocation == FlashlightLocation::OnWeapon) {
                    Utils::turnFlashlightOff();
                } else if (Utils::flashlightLocation == FlashlightLocation::InPrimaryHand) {
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::InOffhand);
                } else {
                    // On the offhand: move to the primary hand (empty) or the weapon (regular weapon drawn).
                    Utils::switchFlashlightConfigLocation(FlashlightConfigLocation::InPrimaryHand);
                }
                return true;
            });
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
        _primaryHandSphere.detachDebug();

        logger::info("Disable game Pipboy light control");
        f4vr::getIniSetting("fPipboyLightDelay:Controls", true)->SetFloat(99);
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
