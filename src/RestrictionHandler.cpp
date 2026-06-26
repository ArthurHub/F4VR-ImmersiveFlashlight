#include "RestrictionHandler.h"

#include "Config.h"
#include "FlashlightState.h"
#include "Utils.h"
#include "f4vr/DebugInventory.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    /**
     * Resolve the Immersive head-restriction rule from Config (editor-ID keyword names + (localID, plugin)
     * allow/deny entries) to concrete runtime FormIDs. Keywords are matched by editor ID, which BGSKeyword
     * retains at runtime; allow/deny entries are resolved load-order-independently via LookupForm. Call once
     * after game data is ready, and again on config reload (the lists may have changed). Entries that don't
     * resolve (e.g. a plugin that isn't installed) are simply skipped.
     */
    void RestrictionHandler::resolveForms()
    {
        _headLightKeywordIds.clear();
        _headLightAllowIds.clear();
        _headLightDenyIds.clear();

        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logger::warn("No data handler — cannot resolve headgear requirement forms");
            return;
        }

        if (!g_config.headLightKeywordNames.empty()) {
            for (const auto* keyword : dataHandler->GetFormArray<RE::BGSKeyword>()) {
                const char* editorId = keyword ? keyword->formEditorID.c_str() : nullptr;
                if (!editorId || !editorId[0]) {
                    continue;
                }
                for (const auto& name : g_config.headLightKeywordNames) {
                    if (name == editorId) {
                        _headLightKeywordIds.insert(keyword->formID);
                        break;
                    }
                }
            }
        }

        for (const auto& [localId, plugin] : g_config.headLightAllowList) {
            if (const auto* form = dataHandler->LookupForm(localId, plugin)) {
                _headLightAllowIds.insert(form->formID);
            }
        }
        for (const auto& [localId, plugin] : g_config.headLightDenyList) {
            if (const auto* form = dataHandler->LookupForm(localId, plugin)) {
                _headLightDenyIds.insert(form->formID);
            }
        }

        logger::info("Resolved headgear requirement forms: {} keyword(s), {} allow, {} deny", _headLightKeywordIds.size(), _headLightAllowIds.size(), _headLightDenyIds.size());
    }

    /**
     * The armor worn in the head slot (slot 30 / biped index 0 = kHairTop, where hats/helmets always sit),
     * or nullptr if nothing armor-typed is worn there. Mirrors the biped-object read used by f4vr::isInPowerArmor().
     */
    const RE::TESObjectARMO* RestrictionHandler::getWornHeadgear()
    {
        const auto player = f4vr::getPlayer();
        if (!player) {
            return nullptr;
        }
        const auto biped = player->biped.get();
        if (!biped) {
            return nullptr;
        }
        const auto* equippedForm = biped->object[0].parent.object;
        if (!equippedForm || equippedForm->formType != RE::ENUM_FORM_ID::kARMO) {
            return nullptr;
        }
        return static_cast<const RE::TESObjectARMO*>(equippedForm);
    }

    /**
     * Immersive rule for a specific headgear: "light-capable". Deny list wins, then allow list, then the keyword
     * set (matched against the item's keywords). nullptr (nothing worn) -> not capable.
     */
    bool RestrictionHandler::isLightCapableHeadgear(const RE::TESObjectARMO* armor)
    {
        if (!armor) {
            return false;
        }
        const auto id = armor->formID;
        if (_headLightDenyIds.contains(id)) {
            return false;
        }
        if (_headLightAllowIds.contains(id)) {
            return true;
        }
        for (const auto keywordId : _headLightKeywordIds) {
            if (f4vr::hasKeyword(armor, keywordId)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Immersive rule applied to the currently worn headgear.
     */
    bool RestrictionHandler::isLightCapableHeadgearWorn()
    {
        return isLightCapableHeadgear(getWornHeadgear());
    }

    /**
     * Whether the flashlight is currently allowed on the head. Power armor is always exempt (the PA helmet
     * lamp applies regardless). Out of PA the configured requirement decides: None always allows, AnyHeadGear
     * needs any slot-30 headgear, Immersive needs light-capable headgear.
     */
    bool RestrictionHandler::isHeadFlashlightAllowed()
    {
        if (f4vr::isInPowerArmor()) {
            return true;
        }
        switch (g_config.flashlightHeadgearRequirement) {
        case FlashlightHeadgearRequirement::AnyHeadGear:
            return getWornHeadgear() != nullptr;
        case FlashlightHeadgearRequirement::Immersive:
            return isLightCapableHeadgearWorn();
        case FlashlightHeadgearRequirement::None:
        default:
            return true;
        }
    }

    /**
     * Is there is physically equipped weapon.
     */
    bool RestrictionHandler::isWeaponEquipped()
    {
        return _currentWeapon || f4vr::isUnarmedWeaponDrawn();
    }

    /**
     * Whether the flashlight may mount on the equipped weapon.
     * A drawn melee/unarmed weapon is disallowed.
     * If weapon-flashlight requirement is on the weapon must have found flashlight node.
     */
    bool RestrictionHandler::isWeaponFlashlightAllowed()
    {
        if (!_currentWeapon || _currentWeaponMelee || f4vr::isUnarmedWeaponDrawn()) {
            return false;
        }
        return !g_config.weaponFlashlightRequired || weaponFlashlightNode() != nullptr;
    }

    /**
     * Per-frame driver (called from Flashlight::onFrameUpdate): refresh the weapon-flashlight detection cache,
     * then enforce the active restrictions.
     */
    void RestrictionHandler::onFrameUpdate()
    {
        checkWeaponChangeForFlashlightOnWeaponDetection();

        enforceRestrictions();
    }

    /**
     * Refresh the weapon-flashlight detection cache; a no-op while the requirement is off. The recursive walk
     * of the weapon 3D for a flashlight mesh runs only when the equipped weapon changes, caching the weapon,
     * its melee/unarmed flags, and the found node (or nullptr). Clears the cache when the weapon is holstered
     * or unequipped.
     */
    void RestrictionHandler::checkWeaponChangeForFlashlightOnWeaponDetection()
    {
        if (!g_config.weaponFlashlightRequired) {
            return;
        }

        auto* weaponNode = f4vr::getWeaponNode();
        if (weaponNode && f4vr::isNodeVisible(weaponNode)) {
            auto* equippedWeapon = f4vr::getEquippedWeapon();
            if (_currentWeapon != equippedWeapon) {
                logger::info("Equipped weapon changed to {}, re-scanning for flashlight node.", equippedWeapon ? equippedWeapon->GetFormEditorID() : "<none>");
                _currentWeapon = equippedWeapon;
                _currentWeaponMelee = f4vr::isMeleeWeaponDrawn();
                _weaponFlashlightNode = findWeaponFlashlightNode(weaponNode);
            }
        } else if (_currentWeapon) {
            logger::info("Equipped weapon changed to None");
            _currentWeapon = nullptr;
            _currentWeaponMelee = false;
            _weaponFlashlightNode = nullptr;
        }
    }

    /**
     * Search the equipped weapon 3D for the first configured, visible flashlight mesh node (config order is
     * priority). Returns nullptr when the weapon node isn't visible or no configured node is present and
     * visible. The node names are user config (sWeaponFlashlightMeshNodes) — NIF names aren't contractual.
     */
    RE::NiAVObject* RestrictionHandler::findWeaponFlashlightNode(RE::NiAVObject* weaponNode)
    {
        if (!f4vr::isNodeVisible(weaponNode)) {
            return nullptr;
        }
        for (const auto& nodeName : g_config.weaponFlashlightMeshNodes) {
            if (auto* node = f4vr::findAVObject(weaponNode, nodeName)) {
                logger::info("Found visible weapon-mounted flashlight node: {}", nodeName);
                return node;
            }
        }
        logger::debug("No visible weapon-mounted flashlight node found");
        return nullptr;
    }

    /**
     * Force the weapon-flashlight detection to re-scan on the next update() and drop the cached node. Called
     * on save load (the weapon 3D / node pointer is rebuilt, so a held pointer dangles) and on config reload
     * (the restriction may have been toggled on or the node list changed).
     */
    void RestrictionHandler::invalidate()
    {
        _currentWeapon = nullptr;
        _weaponFlashlightNode = nullptr;
        resolveForms();
    }

    /**
     * The cached weapon-mounted flashlight mesh node, or nullptr when none is mounted / the restriction is
     * off. Valid for the frame after update(); when weaponFlashlightMountBeamToMesh is on, the OnWeapon light
     * placement roots the beam at it.
     */
    RE::NiAVObject* RestrictionHandler::weaponFlashlightNode()
    {
        return _weaponFlashlightNode;
    }

    /**
     * Enforce the active restrictions on the passive path. Skipped while a config-mode location override is
     * active so beam tuning isn't interrupted, and a no-op while the light is off. Turns the light off when
     * its current location is no longer permitted:
     *  - Head: on the (non-PA) head with the headgear requirement unmet (a head runtime location already
     *    implies out of PA — PA maps to OnPAHead, always allowed).
     *  - Weapon: on the weapon with isWeaponFlashlightAllowed() false (no detected mesh, or a drawn
     *    melee/unarmed weapon, which resolves to OnWeapon — see FlashlightState::getFlashlightLocation).
     */
    void RestrictionHandler::enforceRestrictions()
    {
        if (FlashlightState::isRuntimeLocationOverrideActive() || !Utils::isFlashlightOn()) {
            return;
        }

        if (FlashlightState::flashlightLocation == FlashlightLocation::OnHead && !isHeadFlashlightAllowed()) {
            logger::info("Headgear requirement not met — turning the head flashlight off");
            Utils::turnFlashlightOff();
            return;
        }

        if (g_config.weaponFlashlightRequired && FlashlightState::flashlightLocation == FlashlightLocation::OnWeapon && !isWeaponFlashlightAllowed()) {
            logger::info("Equipped weapon has no flashlight mesh — turning the weapon flashlight off");
            Utils::turnFlashlightOff();
        }
    }

    /**
     * Dump every head-slot (CK slot 30) armor, split into the Immersive rule's allowed (light-capable) and
     * blocked sets. Reuses DebugInventory's print-all walk with isLightCapableHeadgear() as the filter
     * predicate, so the listing reflects the resolved keyword/allow/deny lists exactly. A tuning aid for the
     * Immersive lists — it always uses the Immersive rule regardless of the active requirement mode or PA.
     */
    void RestrictionHandler::dumpHeadgear()
    {
        using f4vr::DebugInventory;

        DebugInventory::ItemFilter filter;
        filter.slotMask = 1u << 0; // CK biped slot 30 — where hats / helmets sit (see getWornHeadgear)

        logger::info("==== Immersive rule: allowed (light-capable) headgear ====");
        filter.predicate = [](const RE::TESForm* form) { return isLightCapableHeadgear(static_cast<const RE::TESObjectARMO*>(form)); };
        DebugInventory::iterateObjects(DebugInventory::Operation::Print, DebugInventory::ItemCategory::Armor, filter);

        logger::info("==== Immersive rule: blocked headgear ====");
        filter.predicate = [](const RE::TESForm* form) { return !isLightCapableHeadgear(static_cast<const RE::TESObjectARMO*>(form)); };
        DebugInventory::iterateObjects(DebugInventory::Operation::Print, DebugInventory::ItemCategory::Armor, filter);
    }
}
