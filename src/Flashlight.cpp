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
        if (ImFl::Utils::flashlightLocation != ImFl::FlashlightLocation::OnHead) {
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
        // initial setup of flashlight location and values
        Utils::refreshFlashlightLocation();
        Utils::setLightValues();

        // refresh flashlight values on config change
        g_config.subscribeForIniChangedEvent("Flashlight", [](const std::string& key) {
            Utils::refreshFlashlightLocation();
            Utils::toggleLightRefreshValues();
        });
    }

    /**
     * Executed every frame to update to handle flashlight location and moving between hand and head.
     */
    void Flashlight::onFrameUpdate()
    {
        if (!f4vr::isPipboyLightOn(f4vr::getPlayer())) {
            return;
        }

        Utils::refreshFlashlightLocation();

        checkSwitchingFlashlightOnHeadHand();

        adjustFlashlightTransformToHandOrHead();
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

        // switch between head and offhand
        const auto isOffhandCloseToHMD = MatrixUtils::vec3Len(offhandPos - hmdPos) < 12;
        if (isOffhandCloseToHMD && (Utils::flashlightLocation == FlashlightLocation::OnHead || Utils::flashlightLocation == FlashlightLocation::InOffhand)) {
            triggerHapticOnce(vrcf::Hand::Offhand);
            if (vrcf::VRControllers.isReleasedShort(vrcf::Hand::Offhand, g_config.switchTorchButton)) {
                Utils::switchFlashlightConfigLocation(Utils::flashlightLocation == FlashlightLocation::OnHead
                    ? FlashlightConfigLocation::InOffhand
                    : FlashlightConfigLocation::OnHead);
            }
            return;
        }

        // switch between head and primary hand
        const auto isPrimaryHandCloseToHMD = MatrixUtils::vec3Len(primaryHandPos - hmdPos) < 12;
        if (isPrimaryHandCloseToHMD && isAllowedToSwitchHeadAndPrimaryHand()) {
            triggerHapticOnce(vrcf::Hand::Primary);
            if (vrcf::VRControllers.isReleasedShort(vrcf::Hand::Primary, g_config.switchTorchButton)) {
                Utils::switchFlashlightConfigLocation(Utils::flashlightLocation == FlashlightLocation::OnHead
                    ? FlashlightConfigLocation::InPrimaryHand
                    : FlashlightConfigLocation::OnHead);
            }
            return;
        }

        // switch between offhand and primary hand
        const auto isHandsCloseToEachOther = MatrixUtils::vec3Len(primaryHandPos - offhandPos) < 12;
        if (isHandsCloseToEachOther && Utils::flashlightLocation != FlashlightLocation::OnHead) {
            triggerHapticOnce(vrcf::Hand::Left);
            if (vrcf::VRControllers.isReleasedShort(vrcf::Hand::Offhand, g_config.switchTorchButton)) {
                Utils::switchFlashlightConfigLocation(g_config.flashlightConfigLocation == FlashlightConfigLocation::InPrimaryHand
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

        if (Utils::flashlightLocation != FlashlightLocation::OnHead) {
            // update world transforms after reverting to original
            f4vr::updateTransforms(lightNode);

            // use the right arm node
            RE::NiNode* attachNode;
            RE::NiMatrix3 rotationOffset;
            if (Utils::flashlightLocation == FlashlightLocation::OnWeapon) {
                attachNode = f4vr::getWeaponNode();
                rotationOffset = MatrixUtils::getMatrixFromEulerAnglesDegrees(90, 0, -90);
            } else {
                attachNode = Utils::flashlightLocation == FlashlightLocation::InOffhand ? f4vr::getOffhandWandNode() : f4vr::getPrimaryHandWandNode();
                rotationOffset = MatrixUtils::getMatrixFromEulerAnglesDegrees(0, -60, -90);
            }

            // calculate relocation transform and set to local
            lightNode->local = MatrixUtils::calculateRelocationWithOffsets(lightNode, attachNode, RE::NiPoint3::ZERO, rotationOffset);

            // small adjustment to prevent light on the fingers and shadows from them
            const float offsetX = f4vr::isInPowerArmor() ? 16.0f : 12.0f;
            const float offsetY = Utils::flashlightLocation == FlashlightLocation::InOffhand ? -5.0f : 5.0f;
            lightNode->local.translate += RE::NiPoint3(offsetX, offsetY, 5);
        }
    }

    void Flashlight::triggerHapticOnce(const vrcf::Hand hand)
    {
        if (!_flashlightHapticActivated) {
            _flashlightHapticActivated = true;
            triggerStrongHaptic(hand);
        }
    }
}
