#include "FlashlightMesh.h"

#include "Config.h"
#include "Utils.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"

namespace ImFl
{
    /**
     * Called every frame. Attaches the mesh when the flashlight is on in a hand mode,
     * detaches it otherwise, and re-attaches if the parent node or location changed.
     */
    void FlashlightMesh::onFrameUpdate(const bool isFlashlightOn)
    {
        if (!g_config.showFlashlightMesh) {
            return;
        }

        if (!isFlashlightOn || !isMeshLocation()) {
            detach();
            return;
        }

        const auto parent = resolveParentNode();
        if (!parent) {
            detach();
            return;
        }

        // re-attach if the skeleton pointer changed (e.g. after PA transition) or location changed
        if (_meshNode && (parent != _attachedTo || Utils::flashlightLocation != _attachedForLocation)) {
            detach();
        }

        if (!_meshNode) {
            attach(parent);
        }
    }

    /**
     * Detaches the mesh and clears tracking state so the next onFrameUpdate call
     * re-attaches from scratch. Call after power armor transitions and game session loads
     * since the skeleton node pointers may have changed.
     */
    void FlashlightMesh::invalidate()
    {
        if (!g_config.showFlashlightMesh) {
            return;
        }
        detach();
    }

    /**
     * Loads and clones the flashlight NIF, sets the configured position offset,
     * attaches it as a child of parentNode, and refreshes the bone tree caches.
     * Sets _loadFailed on any error to prevent repeated log spam.
     */
    void FlashlightMesh::attach(RE::NiNode* parentNode)
    {
        const auto clone = f4vr::getClonedNiNodeForNifFileSetName(NIF_PATH, MESH_NODE_NAME);

        clone->local.translate = RE::NiPoint3(g_config.flashlightMeshOffsetX, g_config.flashlightMeshOffsetY, g_config.flashlightMeshOffsetZ);

        logger::info("FlashlightMesh: attach for location {}", static_cast<int>(Utils::flashlightLocation));
        parentNode->AttachChild(clone, true);

        _meshNode = clone;
        _attachedTo = parentNode;
        _attachedForLocation = Utils::flashlightLocation;
    }

    /**
     * Removes the mesh node from its parent and refreshes the bone tree caches.
     * Clears all tracking state.
     */
    void FlashlightMesh::detach()
    {
        if (!_meshNode) {
            return;
        }

        if (_meshNode->parent) {
            logger::debug<>("FlashlightMesh: detached");
            _meshNode->parent->DetachChild(_meshNode);
        }

        _meshNode = nullptr;
        _attachedTo = nullptr;
        _attachedForLocation = FlashlightLocation::OnHead;
    }

    /**
     * Returns the VR controller wand node for the current flashlight location,
     * or nullptr if the location has no mesh (head, PA head, weapon).
     */
    RE::NiNode* FlashlightMesh::resolveParentNode()
    {
        if (Utils::flashlightLocation == FlashlightLocation::InOffhand) {
            return f4vr::getOffhandWandNode();
        }
        if (Utils::flashlightLocation == FlashlightLocation::InPrimaryHand) {
            return f4vr::getPrimaryHandWandNode();
        }
        return nullptr;
    }

    /**
     * Returns true when the current flashlight location warrants showing the hand mesh
     * (InOffhand or InPrimaryHand). Head and weapon modes are excluded.
     */
    bool FlashlightMesh::isMeshLocation()
    {
        return Utils::flashlightLocation == FlashlightLocation::InOffhand
            || Utils::flashlightLocation == FlashlightLocation::InPrimaryHand;
    }
}
