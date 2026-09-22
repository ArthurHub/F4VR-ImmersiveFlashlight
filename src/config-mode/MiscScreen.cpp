#include "MiscScreen.h"

#include <map>

#include "Config.h"
#include "FlashlightState.h"
#include "RestrictionHandler.h"
#include "Utils.h"
#include "f4vr/PlayerNodes.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIManager.h"
#include "vrui/UIMultiStateToggleButtonPanel.h"
#include "vrui/UITextPanel.h"
#include "vrui/UIToggleButtonPanel.h"

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
        FlashlightState::refreshLightValues();
        if (shadowsEnabled) {
            f4vr::showNotification(std::format("Flashlight Shadows: On\nMake sure Shadow Quality is set to HIGH in settings"));
        } else {
            f4vr::showNotification(std::format("Flashlight Shadows: Off"));
        }
    }

    /**
     * Toggle rendering of every activation sphere at its true size. Read live by Flashlight each frame, so the
     * spheres appear/disappear on the next frame; persisted so the choice survives a restart.
     */
    void MiscScreen::toggleDebugSpheres(const bool enabled)
    {
        g_config.setShowAllActivationSpheres(enabled);
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
     * Toggle the NPC light-detection feature (NPCs noticing the beam). Read live by NpcDetectionHandler
     * on each detection tick, so it takes effect immediately.
     */
    void MiscScreen::toggleNpcDetection(const bool enabled)
    {
        g_config.setNpcDetectionEnabled(enabled);
        f4vr::showNotification(enabled ? "NPCs notice the flashlight beam\nShining the light on someone can give away your position"
                                       : "NPCs ignore the flashlight beam\nVanilla behavior, the light never gives you away");
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
        // headgear requirement cycles through its 3 states
        const std::map<FlashlightHeadgearRequirement, UIButtonPanelContent> headgearStates{
            { FlashlightHeadgearRequirement::None, { .topText = "HEADGEAR", .middleText = "RESTRICT:", .bottomText = "NONE" } },
            { FlashlightHeadgearRequirement::AnyHeadGear, { .topText = "HEADGEAR", .middleText = "RESTRICT:", .bottomText = "ANY" } },
            { FlashlightHeadgearRequirement::Immersive, { .topText = "HEADGEAR", .middleText = "RESTRICT:", .bottomText = "IMMERSIVE" } },
        };
        const auto headgearReqBtn = std::make_shared<UIMultiStateToggleButtonPanel<FlashlightHeadgearRequirement>>("ImFl_HeadgearRequirementButton", headgearStates);
        headgearReqBtn->setState(g_config.flashlightHeadgearRequirement);
        headgearReqBtn->setOnStateChangedHandler(
            [](UIMultiStateToggleButtonPanel<FlashlightHeadgearRequirement>*, const FlashlightHeadgearRequirement state) { onHeadgearRequirementChanged(state); });

        // weapon-mesh requirement cycles through its 3 states
        const std::map<FlashlightWeaponMeshRequirement, UIButtonPanelContent> weaponReqStates{
            { FlashlightWeaponMeshRequirement::Disabled, { .topText = "WEAPON", .middleText = "RESTRICT:", .bottomText = "NONE" } },
            { FlashlightWeaponMeshRequirement::Enabled, { .topText = "WEAPON", .middleText = "RESTRICT:", .bottomText = "ON" } },
            { FlashlightWeaponMeshRequirement::AutoDetect, { .topText = "WEAPON", .middleText = "RESTRICT:", .bottomText = "AUTO" } },
        };
        const auto weaponReqBtn = std::make_shared<UIMultiStateToggleButtonPanel<FlashlightWeaponMeshRequirement>>("ImFl_WeaponRequirementButton", weaponReqStates);
        weaponReqBtn->setState(g_config.weaponFlashlightMeshRequirement);
        weaponReqBtn->setOnStateChangedHandler(
            [](UIMultiStateToggleButtonPanel<FlashlightWeaponMeshRequirement>*, const FlashlightWeaponMeshRequirement state) { onWeaponMeshRequirementChanged(state); });

        const auto npcDetectionTglBtn = std::make_shared<UIToggleButtonPanel>("ImFl_NpcDetectionToggle");
        npcDetectionTglBtn->setTopText("NPC");
        npcDetectionTglBtn->setMiddleText("ENEMY");
        npcDetectionTglBtn->setBottomText("DETECTION");
        npcDetectionTglBtn->setToggleState(g_config.npcDetectionEnabled);
        npcDetectionTglBtn->setOnToggleHandler([](UIToggleButtonPanel*, const bool enabled) { toggleNpcDetection(enabled); });

        const auto showOnBodyTglBtn = std::make_shared<UIToggleButtonPanel>("ImFl_ShowOnBodyToggle");
        showOnBodyTglBtn->setTopText("GRAB");
        showOnBodyTglBtn->setMiddleText("FLASHLIGHT");
        showOnBodyTglBtn->setBottomText("ON BELT");
        showOnBodyTglBtn->setToggleState(g_config.showFlashlightOnBody);
        showOnBodyTglBtn->setOnToggleHandler([](UIToggleButtonPanel*, const bool enabled) { toggleShowOnBody(enabled); });

        // the button is the vanilla binding itself, so it is ON when the disable config flag is OFF
        const auto disableVanillaTglBtn = std::make_shared<UIToggleButtonPanel>("ImFl_VanillaToggleToggle");
        disableVanillaTglBtn->setTopText("VANILLA");
        disableVanillaTglBtn->setMiddleText("GLOBAL");
        disableVanillaTglBtn->setBottomText("TOGGLE");
        disableVanillaTglBtn->setToggleState(!g_config.disableVanillaFlashlightToggle);
        disableVanillaTglBtn->setOnToggleHandler([](UIToggleButtonPanel*, const bool enabled) { toggleDisableVanillaToggle(enabled); });

        const auto row1 = std::make_shared<UIContainer>("MiscRow1", UIContainerLayout::HorizontalCenter, 0.3f);
        row1->addElement(headgearReqBtn);
        row1->addElement(weaponReqBtn);
        row1->addElement(npcDetectionTglBtn);
        row1->addElement(showOnBodyTglBtn);
        row1->addElement(disableVanillaTglBtn);

        const auto shadowsTglBtn = std::make_shared<UIToggleButtonPanel>("ImFl_ShadowsToggle");
        shadowsTglBtn->setTopText("SHADOWS");
        shadowsTglBtn->setImage("vrui\\flashlight-shadows.DDS");
        shadowsTglBtn->setBottomText("ON / OFF");
        shadowsTglBtn->setToggleState(Utils::areFlashlightShadowsEnabled());
        shadowsTglBtn->setOnToggleHandler([](UIToggleButtonPanel*, const bool shadowsEnabled) { toggleShadows(shadowsEnabled); });

        const auto debugSpheresTglBtn = std::make_shared<UIToggleButtonPanel>("ImFl_DebugSpheresToggle");
        debugSpheresTglBtn->setTopText("DEBUG");
        debugSpheresTglBtn->setImage("f4cf\\vrui\\debug-spheres.DDS");
        debugSpheresTglBtn->setBottomText("SPHERES");
        debugSpheresTglBtn->setToggleState(g_config.showAllActivationSpheres);
        debugSpheresTglBtn->setOnToggleHandler([](UIToggleButtonPanel*, const bool enabled) { toggleDebugSpheres(enabled); });

        const auto backBtn = std::make_shared<UIButtonPanel>("ImFl_BackButton");
        backBtn->setImage("f4cf\\vrui\\exit.DDS");
        backBtn->setBottomText("BACK");
        backBtn->setOnPressHandler([this](UIButtonPanel*) {
            if (_onBack) {
                _onBack();
            }
        });

        const auto row2 = std::make_shared<UIContainer>("MiscNav", UIContainerLayout::HorizontalCenter, 0.3f);
        row2->addElement(shadowsTglBtn);
        row2->addElement(debugSpheresTglBtn);
        row2->addElement(backBtn);

        const auto header = std::make_shared<UITextPanel>("ImFl_Title");
        header->setStyle(UIPanelStyle{ .color = F4VR_PANEL_STYLE.color });
        header->setTextHeight(0.45f);
        header->setContent([](std::vector<TextRow>& rows) { rows.emplace_back("IMMERSIVE FLASHLIGHT: CONFIG", std::nullopt, render::TextDecoration::Underline); });

        _ui = std::make_shared<UIContainer>("MiscConfig", UIContainerLayout::VerticalUp, 0.35f, 1.6f);
        _ui->addElement(row2);
        _ui->addElement(row1);
        _ui->addElement(header);

        g_uiManager->attachPresetToPrimaryWandTop(_ui, { 0, 0, 0 });
    }
}
