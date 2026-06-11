#include "Flashlight.h"

#include "Config.h"
#include "ImmersiveFlashlight.h"
#include "Utils.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"
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

    void triggerStrongHaptic(const vrcf::Hand hand)
    {
        vrcf::VRControllers.triggerHaptic(hand, 0.05f, 0.5f);
    }
}

namespace ImFl
{
    Flashlight::Flashlight()
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
        const bool isFlashlightOn = f4vr::isPipboyLightOn(f4vr::getPlayer());

        handlePowerArmorTransition(isFlashlightOn);

        if (!isFlashlightOn) {
            _flashlightMesh.onFrameUpdate(false);
            return;
        }

        Utils::refreshFlashlightLocation();

        Utils::refreshGripStyle();

        _flashlightMesh.onFrameUpdate(true);

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
            const bool wasFlashlightOnRecently = isFlashlightOn || _flashlightOnRecentlyFrames > 0;
            if (wasFlashlightOnRecently && !isFlashlightOn) {
                logger::info("Restoring flashlight after power armor transition");
                Utils::refreshFlashlightLocation();
                Utils::setLightValues();
                Utils::turnFlashlightOn();
                isFlashlightOn = true;
                _flashlightMesh.invalidate(); // skeleton changed — force re-attach next frame
            }
        }
        _flashlightOnRecentlyFrames = isFlashlightOn ? 5 : max(0, _flashlightOnRecentlyFrames - 1);
    }

    /**
     * Switch between Pipboy flashlight on head or right/left hand based if player switches using button press of the hand near head.
     */
    void Flashlight::checkSwitchingFlashlightOnHeadHand()
    {
        // check a bit higher than the HMD to allow hand close to the lower part of the face
        const auto& hmdPos = f4vr::getPlayerNodes()->HmdNode->world.translate + RE::NiPoint3(0, 0, 4);
        const auto& offhandPos = f4vr::getOffhandWandNode()->world.translate;
        const auto& primaryHandPos = f4vr::getPrimaryHandWandNode()->world.translate;

        // debug hand position to understand why a player doesn't have ability to switch from hand to head
        if (logger::isTraceEnabled()) {
            const auto offhandToHmdDiff = offhandPos - hmdPos;
            const auto primaryHandToHmdDiff = primaryHandPos - hmdPos;
            const auto primaryHandToOffhandDiff = primaryHandPos - offhandPos;
            logger::sample(
                "Head/hand debug:\n"
                " HMD=({:.2f}, {:.2f}, {:.2f})\n"
                " Offhand=({:.2f}, {:.2f}, {:.2f})\n"
                " Offhand-HMD=({:.2f}, {:.2f}, {:.2f}), isOffhandCloseToHMD={}\n"
                " Primary=({:.2f}, {:.2f}, {:.2f})"
                " Primary-HMD=({:.2f}, {:.2f}, {:.2f}), isPrimaryHandCloseToHMD={}\n"
                " Primary-Offhand=({:.2f}, {:.2f}, {:.2f}), isHandsCloseToEachOther={}",
                hmdPos.x,
                hmdPos.y,
                hmdPos.z,
                offhandPos.x,
                offhandPos.y,
                offhandPos.z,
                offhandToHmdDiff.x,
                offhandToHmdDiff.y,
                offhandToHmdDiff.z,
                MatrixUtils::vec3Len(offhandPos - hmdPos) < 12,
                primaryHandPos.x,
                primaryHandPos.y,
                primaryHandPos.z,
                primaryHandToHmdDiff.x,
                primaryHandToHmdDiff.y,
                primaryHandToHmdDiff.z,
                MatrixUtils::vec3Len(primaryHandPos - hmdPos) < 12,
                primaryHandToOffhandDiff.x,
                primaryHandToOffhandDiff.y,
                primaryHandToOffhandDiff.z,
                MatrixUtils::vec3Len(primaryHandPos - offhandPos) < 12);
        }

        // switch between head and offhand
        const auto isOffhandCloseToHMD = MatrixUtils::vec3Len(offhandPos - hmdPos) < 12;
        if (isOffhandCloseToHMD && (Utils::isHeadMountedFlashlight() || Utils::flashlightLocation == FlashlightLocation::InOffhand)) {
            triggerHapticOnce(vrcf::Hand::Offhand);
            if (vrcf::VRControllers.check(g_config.switchFlashlightBindingOffhand)) {
                Utils::switchFlashlightConfigLocation(Utils::isHeadMountedFlashlight() ? FlashlightConfigLocation::InOffhand : FlashlightConfigLocation::OnHead);
            }
            return;
        }

        // switch between head and primary hand
        const auto isPrimaryHandCloseToHMD = MatrixUtils::vec3Len(primaryHandPos - hmdPos) < 12;
        if (isPrimaryHandCloseToHMD && isAllowedToSwitchHeadAndPrimaryHand()) {
            triggerHapticOnce(vrcf::Hand::Primary);
            if (vrcf::VRControllers.check(g_config.switchFlashlightBindingPrimary)) {
                Utils::switchFlashlightConfigLocation(Utils::isHeadMountedFlashlight() ? FlashlightConfigLocation::InPrimaryHand : FlashlightConfigLocation::OnHead);
            }
            return;
        }

        // switch between offhand and primary hand
        const auto isHandsCloseToEachOther = MatrixUtils::vec3Len(primaryHandPos - offhandPos) < 12;
        if (isHandsCloseToEachOther && !Utils::isHeadMountedFlashlight()) {
            triggerHapticOnce(vrcf::Hand::Left);
            if (vrcf::VRControllers.check(g_config.switchFlashlightBindingOffhand)) {
                Utils::switchFlashlightConfigLocation(Utils::getActiveFlashlightConfigLocation() == FlashlightConfigLocation::InPrimaryHand
                        ? FlashlightConfigLocation::InOffhand
                        : FlashlightConfigLocation::InPrimaryHand);
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
        _flashlightMesh.invalidate(); // skeleton pointers may have changed on save load
    }

    void Flashlight::triggerHapticOnce(const vrcf::Hand hand)
    {
        if (!_flashlightHapticActivated) {
            _flashlightHapticActivated = true;
            logger::debug("Haptic triggered on hand: {}", Utils::getHandLabel(hand));
            triggerStrongHaptic(hand);
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
