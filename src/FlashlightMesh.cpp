#include "FlashlightMesh.h"

#include "Config.h"
#include "Utils.h"
#include "common/MatrixUtils.h"
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
        if (_attachedTo && (parent != _attachedTo || Utils::flashlightLocation != _attachedForLocation)) {
            detach();
        }

        if (!_attachedTo) {
            attach(parent);
        }
    }

    /**
     * Detaches the mesh but keeps the cached clone alive so the next onFrameUpdate call
     * re-attaches without re-cloning. Call after power armor transitions and game session
     * loads since the skeleton node pointers may have changed.
     */
    void FlashlightMesh::invalidate()
    {
        if (!g_config.showFlashlightMesh) {
            return;
        }
        detach();
    }

    /**
     * Attaches the flashlight mesh to parentNode. Clones the NIF on first use; subsequent
     * calls re-use the cached node. Always re-applies the configured position offset.
     */
    void FlashlightMesh::attach(RE::NiNode* parentNode)
    {
        if (!_meshNode) {
            _meshNode.reset(f4vr::getClonedNiNodeForNifFileSetName(NIF_PATH, MESH_NODE_NAME));
            logger::info("FlashlightMesh: cloned NIF for location {}", static_cast<int>(Utils::flashlightLocation));
        } else {
            logger::info("FlashlightMesh: re-attaching cached node for location {}", static_cast<int>(Utils::flashlightLocation));
        }

        parentNode->AttachChild(_meshNode.get(), true);

        _attachedTo = parentNode;
        _attachedForLocation = Utils::flashlightLocation;

        const float sign = Utils::flashlightLocation == FlashlightLocation::InOffhand ? -1.0f : 1.0f;
        _meshNode->local.translate = RE::NiPoint3(-g_config.flashlightMeshOffsetX, g_config.flashlightMeshOffsetY, sign * g_config.flashlightMeshOffsetZ);
        _meshNode->local.rotate = common::MatrixUtils::getMatrixFromEulerAnglesDegrees(sign * (25 - g_config.flashlightInHandControllerAngleOffset), 0, 90);
    }

    /**
     * Removes the mesh node from its parent, keeping the clone alive in _meshNode
     * so the next attach() can re-use it without re-cloning the NIF.
     */
    void FlashlightMesh::detach()
    {
        if (!_attachedTo) {
            return;
        }

        if (_meshNode && _meshNode->parent) {
            logger::debug<>("FlashlightMesh: detached");
            RE::NiPointer<RE::NiAVObject> held;
            _meshNode->parent->DetachChild(_meshNode.get(), held);
            // held goes out of scope; _meshNode keeps the clone alive
        }

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
            return f4vr::isLeftHandedMode()
                ? f4vr::findNode(f4vr::getCommonNode(), "RArm_Hand")
                : f4vr::findNode(f4vr::getCommonNode(), "LArm_Hand");
        }
        if (Utils::flashlightLocation == FlashlightLocation::InPrimaryHand) {
            return !f4vr::isLeftHandedMode()
                ? f4vr::findNode(f4vr::getCommonNode(), "RArm_Hand")
                : f4vr::findNode(f4vr::getCommonNode(), "LArm_Hand");
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
