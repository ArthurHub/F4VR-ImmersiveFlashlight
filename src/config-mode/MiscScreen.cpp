#include "MiscScreen.h"

#include <map>

#include "Config.h"
#include "FlashlightState.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "f4vr/PlayerNodes.h"
#include "vrui/UIButton.h"
#include "vrui/UIManager.h"
#include "vrui/UIMultiStateToggleButton.h"

using namespace vrui;

namespace ImFl::config
{
    bool MiscScreen::isOpen() const
    {
        return _ui != nullptr;
    }

    void MiscScreen::setOnBackHandler(std::function<void()> handler)
    {
        _onBack = std::move(handler);
    }

    /**
     * Build and attach the misc screen UI.
     */
    void MiscScreen::open()
    {
        if (isOpen()) {
            return;
        }
        createUI();
    }

    /**
     * Detach and release the misc screen UI. The toggles persist themselves, so there is nothing to discard.
     */
    void MiscScreen::close()
    {
        if (!isOpen()) {
            return;
        }
        g_uiManager->detachElement(_ui, true);
        _ui.reset();
    }

    void MiscScreen::onFrameUpdate() const
    {
        if (!isOpen()) {
            return;
        }
        _ui->setPosition(0, 0, f4vr::isNodeVisible(f4vr::getWeaponNode()) ? 6.0f : 0.0f);
    }

    /**
     * Toggle the shadows on/off for the flashlight beam. Global setting that affects all flashlight locations.
     */
    void MiscScreen::toggleShadows(const bool shadowsEnabled)
    {
        g_config.setFlashlightFlagsBitmask(shadowsEnabled ? Utils::FLASHLIGHT_FLAGS_WITH_SHADOWS : Utils::FLASHLIGHT_FLAGS_NO_SHADOWS);
        FlashlightState::toggleLightRefreshValues();
        if (shadowsEnabled) {
            f4vr::showNotification(std::format("Flashlight Shadows: On\nMake sure Shadow Quality is set to HIGH in settings"));
        } else {
            f4vr::showNotification(std::format("Flashlight Shadows: Off"));
        }
    }

    /**
     * Toggle rendering of the grab/activation debug spheres. Read live by Flashlight each frame, so the
     * spheres appear/disappear on the next frame; persisted so the choice survives a restart.
     */
    void MiscScreen::toggleDebugSpheres(const bool enabled)
    {
        g_config.setDebugShowGrabSphere(enabled);
        if (enabled) {
            f4vr::showNotification("Immersive activation areas spheres shown on the body/weapon");
        }
    }

    /**
     * Toggle the stowed-on-body flashlight model (grab/return gesture source). Read live by Flashlight.
     */
    void MiscScreen::toggleShowOnBody(const bool enabled)
    {
        g_config.setShowFlashlightOnBody(enabled);
        f4vr::showNotification(enabled ? "Flashlight mesh is shown on player belt\nUse it as immersive activation area to grab/return the flashlight in hand"
                                       : "No flashlight mesh shown on player belt\nUse other controls to move flashlight to the hand");
    }

    /**
     * Cycle the head-mounted-light headgear requirement (None -> any headgear -> immersive). Read live by
     * RestrictionHandler, which turns an on-head light off if the new requirement is no longer met.
     */
    void MiscScreen::onHeadgearRequirementChanged(const FlashlightHeadgearRequirement requirement)
    {
        g_config.setFlashlightHeadgearRequirement(requirement);
        switch (requirement) {
        case FlashlightHeadgearRequirement::None:
            f4vr::showNotification("No restriction on flashlight on the head");
            break;
        case FlashlightHeadgearRequirement::AnyHeadGear:
            f4vr::showNotification("Player has to wear ANY headgear for the flashlight to be on the head");
            break;
        case FlashlightHeadgearRequirement::Immersive:
            f4vr::showNotification("Player has to wear appropriate headgear (full helmets) for the flashlight to be on the head\nSee ini for controlling allowed headgear");
            break;
        }
    }

    /**
     * Cycle the weapon-flashlight-mesh requirement (Disabled -> Enabled -> AutoDetect). Invalidates the
     * restriction cache so the mesh detection and resolved effective state re-scan for the new value (the
     * programmatic save does not fire the config hot-reload).
     */
    void MiscScreen::onWeaponMeshRequirementChanged(const FlashlightWeaponMeshRequirement requirement)
    {
        g_config.setWeaponFlashlightMeshRequirement(requirement);
        RestrictionHandler::invalidate();
        switch (requirement) {
        case FlashlightWeaponMeshRequirement::Disabled:
            f4vr::showNotification("No restriction on flashlight on weapon");
            break;
        case FlashlightWeaponMeshRequirement::Enabled:
            f4vr::showNotification("Require a modeled flashlight mesh on the weapon");
            break;
        case FlashlightWeaponMeshRequirement::AutoDetect:
            f4vr::showNotification(RestrictionHandler::isWeaponFlashlightMeshRequired()
                    ? "Auto detect mods with flashlight support\nSupported mod detected, weapon flashlight restriction is ON!"
                    : "Auto detect mods with flashlight support\nNo supported mod detected, weapon flashlight restriction is OFF!");
            break;
        }
    }

    /**
     * Toggle disabling the vanilla global flashlight toggle, applying/restoring the game setting live.
     */
    void MiscScreen::toggleDisableVanillaToggle(const bool enabled)
    {
        g_config.setDisableVanillaFlashlightToggle(!enabled);
        Utils::updateVanillaFlashlightToggleDisabled();
        f4vr::showNotification(enabled ? "Vanilla toggle binding enabled\nOffhand trigger long press to toggle ON/OFF"
                                       : "Vanilla toggle binding disabled\nUse immersive activation areas (belt, head, weapon) to turn ON/OFF");
    }

    /**
     * Create all the misc config UI elements.
     */
    void MiscScreen::createUI()
    {
        // headgear requirement cycles through its 3 states; each state needs its own nif (all stubbed for now)
        const std::map<FlashlightHeadgearRequirement, std::string> headgearNifs{
            { FlashlightHeadgearRequirement::None, "ui-config-main\\btn-restrict-on-head-none.nif" },
            { FlashlightHeadgearRequirement::AnyHeadGear, "ui-config-main\\btn-restrict-on-head-any.nif" },
            { FlashlightHeadgearRequirement::Immersive, "ui-config-main\\btn-restrict-on-head-immersive.nif" },
        };
        const auto headgearReqBtn = std::make_shared<UIMultiStateToggleButton<FlashlightHeadgearRequirement>>(headgearNifs);
        headgearReqBtn->setState(g_config.flashlightHeadgearRequirement);
        headgearReqBtn->setOnStateChangedHandler(
            [](UIMultiStateToggleButton<FlashlightHeadgearRequirement>*, const FlashlightHeadgearRequirement state) { onHeadgearRequirementChanged(state); });

        // weapon-mesh requirement cycles through its 3 states; each state has its own nif
        const std::map<FlashlightWeaponMeshRequirement, std::string> weaponReqNifs{
            { FlashlightWeaponMeshRequirement::Disabled, "ui-config-main\\btn-restrict-on-weapon-none.nif" },
            { FlashlightWeaponMeshRequirement::Enabled, "ui-config-main\\btn-restrict-on-weapon-on.nif" },
            { FlashlightWeaponMeshRequirement::AutoDetect, "ui-config-main\\btn-restrict-on-weapon-auto.nif" },
        };
        const auto weaponReqBtn = std::make_shared<UIMultiStateToggleButton<FlashlightWeaponMeshRequirement>>(weaponReqNifs);
        weaponReqBtn->setState(g_config.weaponFlashlightMeshRequirement);
        weaponReqBtn->setOnStateChangedHandler(
            [](UIMultiStateToggleButton<FlashlightWeaponMeshRequirement>*, const FlashlightWeaponMeshRequirement state) { onWeaponMeshRequirementChanged(state); });

        const auto showOnBodyTglBtn = std::make_shared<UIToggleButton>("ui-config-main\\btn-mesh-on-body.nif");
        showOnBodyTglBtn->setToggleState(g_config.showFlashlightOnBody);
        showOnBodyTglBtn->setOnToggleHandler([](UIWidget*, const bool enabled) { toggleShowOnBody(enabled); });

        const auto disableVanillaTglBtn = std::make_shared<UIToggleButton>("ui-config-main\\btn-disable-global-toggle.nif");
        disableVanillaTglBtn->setToggleState(g_config.disableVanillaFlashlightToggle);
        disableVanillaTglBtn->setOnToggleHandler([](UIWidget*, const bool enabled) { toggleDisableVanillaToggle(enabled); });

        const auto row1 = std::make_shared<UIContainer>("MiscRow1", UIContainerLayout::HorizontalCenter, 0.3f);
        row1->addElement(headgearReqBtn);
        row1->addElement(weaponReqBtn);
        row1->addElement(showOnBodyTglBtn);
        row1->addElement(disableVanillaTglBtn);

        const auto shadowsTglBtn = std::make_shared<UIToggleButton>("ui-config-main\\btn-flashlight-shadows.nif");
        shadowsTglBtn->setToggleState(Utils::areFlashlightShadowsEnabled());
        shadowsTglBtn->setOnToggleHandler([](UIWidget*, const bool shadowsEnabled) { toggleShadows(shadowsEnabled); });

        const auto debugSpheresTglBtn = std::make_shared<UIToggleButton>("ui-common\\btn-debug-spheres.nif");
        debugSpheresTglBtn->setToggleState(g_config.debugShowGrabSphere);
        debugSpheresTglBtn->setOnToggleHandler([](UIWidget*, const bool enabled) { toggleDebugSpheres(enabled); });

        const auto backBtn = std::make_shared<UIButton>("ui-common\\btn-back.nif");
        backBtn->setOnPressHandler([this](UIWidget*) {
            if (_onBack) {
                _onBack();
            }
        });

        const auto row2 = std::make_shared<UIContainer>("MiscNav", UIContainerLayout::HorizontalCenter, 0.3f);
        row2->addElement(shadowsTglBtn);
        row2->addElement(debugSpheresTglBtn);
        row2->addElement(backBtn);

        const auto header = std::make_shared<UIWidget>("ui-config-main\\title.nif", 1.5f);

        _ui = std::make_shared<UIContainer>("MiscConfig", UIContainerLayout::VerticalUp, 0.35f, 1.6f);
        _ui->addElement(row2);
        _ui->addElement(row1);
        _ui->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_ui, { 0, 0, 0 });
    }
}
