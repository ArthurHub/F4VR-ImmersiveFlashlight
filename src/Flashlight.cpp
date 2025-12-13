#include "Flashlight.h"

#include "Config.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VROffsets.h"
#include "f4vr/PlayerNodes.h"
#include "vrcf/VRControllersManager.h"

using namespace common;

namespace
{
    void triggerStrongHaptic(const vrcf::Hand hand)
    {
        vrcf::VRControllers.triggerHaptic(hand, 0.05f, 0.5f);
    }
}

namespace ImFl
{
    Flashlight::Flashlight()
    {
        setLightValues();
    }

    /**
     * Executed every frame to update to handle flashlight location and moving between hand and head.
     */
    void Flashlight::onFrameUpdate()
    {
        if (!f4vr::isPipboyLightOn(f4vr::getPlayer())) {
            return;
        }

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
        if (isOffhandCloseToHMD && (g_config.flashlightLocation == FlashlightLocation::OnHead || g_config.flashlightLocation == FlashlightLocation::InOffhand)) {
            triggerHapticOnce(vrcf::Hand::Offhand);
            if (vrcf::VRControllers.isReleasedShort(vrcf::Hand::Offhand, g_config.switchTorchButton)) {
                switchFlashlightLocation(g_config.flashlightLocation == FlashlightLocation::OnHead ? FlashlightLocation::InOffhand : FlashlightLocation::OnHead);
            }
            return;
        }

        // switch between head and primary hand
        const auto isPrimaryHandCloseToHMD = MatrixUtils::vec3Len(primaryHandPos - hmdPos) < 12;
        if (isPrimaryHandCloseToHMD && (g_config.flashlightLocation == FlashlightLocation::OnHead || g_config.flashlightLocation == FlashlightLocation::InPrimaryHand)) {
            triggerHapticOnce(vrcf::Hand::Primary);
            if (vrcf::VRControllers.isReleasedShort(vrcf::Hand::Primary, g_config.switchTorchButton)) {
                switchFlashlightLocation(g_config.flashlightLocation == FlashlightLocation::OnHead ? FlashlightLocation::InPrimaryHand : FlashlightLocation::OnHead);
            }
            return;
        }

        // switch between offhand and primary hand
        const auto isHandsCloseToEachOther = MatrixUtils::vec3Len(primaryHandPos - offhandPos) < g_config.debugFlowFlag1;
        if (isHandsCloseToEachOther && g_config.flashlightLocation != FlashlightLocation::OnHead) {
            triggerHapticOnce(vrcf::Hand::Left);
            if (vrcf::VRControllers.isReleasedShort(vrcf::Hand::Offhand, g_config.switchTorchButton)) {
                switchFlashlightLocation(g_config.flashlightLocation == FlashlightLocation::InPrimaryHand
                    ? FlashlightLocation::InOffhand
                    : FlashlightLocation::InPrimaryHand);
            }
            return;
        }

        _flashlightHapticActivated = false;
    }

    /**
     * Switch the flashlight location to the given location, update the light values, and toggle the light to apply the changes.
     */
    void Flashlight::switchFlashlightLocation(const FlashlightLocation location)
    {
        if (g_config.flashlightLocation == location) {
            return;
        }

        g_config.setFlashlightLocation(location);

        // toggle the flashlight to reload the light values
        const auto player = f4vr::getPlayer();
        f4vr::togglePipboyLight(player);
        setLightValues();
        f4vr::togglePipboyLight(player);
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

        if (g_config.flashlightLocation != FlashlightLocation::OnHead) {
            // update world transforms after reverting to original
            f4vr::updateTransforms(lightNode);

            // use the right arm node
            const auto armNode = g_config.flashlightLocation == FlashlightLocation::InOffhand ? f4vr::getOffhandWandNode() : f4vr::getPrimaryHandWandNode();

            // calculate relocation transform and set to local
            lightNode->local = MatrixUtils::calculateRelocationWithOffsets(lightNode, armNode, RE::NiPoint3::ZERO, MatrixUtils::getMatrixFromEulerAnglesDegrees(0, -60, -90));

            // small adjustment to prevent light on the fingers and shadows from them
            const float offsetX = f4vr::isInPowerArmor() ? 16.0f : 12.0f;
            const float offsetY = g_config.flashlightLocation == FlashlightLocation::InOffhand ? -5.0f : 5.0f;
            lightNode->local.translate += RE::NiPoint3(offsetX, offsetY, 5);
        }
    }

    /**
     * Set the light values to config depending if the flashlight is in hand or on head.
     * The light object is the standard PA light.
     */
    void Flashlight::setLightValues()
    {
        auto* light = RE::TESForm::GetFormByID<RE::TESObjectLIGH>(0xB48A0);
        if (!light) {
            logger::warn("Failed to find light object to set flashlight values");
            return;
        }

        switch (g_config.flashlightLocation) {
        case FlashlightLocation::OnHead:
            light->fade = g_config.flashlightOnHeadFade;
            light->data.radius = g_config.flashlightOnHeadRadius;
            light->data.fov = g_config.flashlightOnHeadFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightOnHeadColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightOnHeadColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightOnHeadColorBlue);
            break;
        case FlashlightLocation::InOffhand:
            light->fade = g_config.flashlightInOffhandFade;
            light->data.radius = g_config.flashlightInOffhandRadius;
            light->data.fov = g_config.flashlightInOffhandFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightInOffhandColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightInOffhandColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightInOffhandColorBlue);
            break;
        case FlashlightLocation::InPrimaryHand:
            light->fade = g_config.flashlightInPrimaryHandFade;
            light->data.radius = g_config.flashlightInPrimaryHandRadius;
            light->data.fov = g_config.flashlightInPrimaryHandFov;
            light->data.color.red = static_cast<std::uint8_t>(g_config.flashlightInPrimaryHandColorRed);
            light->data.color.green = static_cast<std::uint8_t>(g_config.flashlightInPrimaryHandColorGreen);
            light->data.color.blue = static_cast<std::uint8_t>(g_config.flashlightInPrimaryHandColorBlue);
            break;
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
