#pragma once

#include <vector>

namespace ImFl
{
    /**
     * Applies the light form's (TESObjectLIGH) beam values to the live game light — the NiLight + BSLight the form
     * spawned when the flashlight was turned on — so a change shows without turning the light off and on. The game
     * copies the form onto a light only while building it (TESObjectLIGH::GenDynamic), so refresh() repeats that
     * copy on the light that already exists; it swaps objects the renderer uses, so it's kept to beam tuning.
     * recreate() builds a new light through the engine's own hide and show, for everything else and for changes
     * refresh() can't apply. Offsets and engine functions are VR 1.2.72; see docs/tech/pipboy-light-internals.md.
     */
    class LiveLight
    {
    public:
        static bool refresh(const RE::TESObjectLIGH& form);
        static void recreate();
        static void onFrameUpdate();

    private:
        static void rebuildLightVolume(RE::BSLight* bsLight, const RE::TESObjectLIGH& form, float fov, float radius);
        static void setGobo(RE::BSLight* bsLight, const RE::TESObjectLIGH& form);
        static void retire(RE::NiRefObject* object);

        // How many frames a replaced light-volume cone or gobo texture is kept alive: the renderer may still hold
        // the pointer the light had when it was swapped. How long it can is unknown, so this is generous.
        static constexpr int RETIRE_FRAMES = 60;

        // How many frames in a row a cone rebuild the busy renderer refused is retried before the old cone is kept.
        static constexpr int MAX_LIGHT_VOLUME_REBUILD_ATTEMPTS = 30;

        struct RetiredObject
        {
            RE::NiRefObject* object;
            int framesLeft;
        };

        // Objects replaced on the light, each holding one reference that onFrameUpdate() drops once the frames run out.
        // Raw pointers on purpose: nothing here may call into the engine from a static destructor at game exit.
        inline static std::vector<RetiredObject> _retiredObjects;

        // The form of a refresh whose cone rebuild the renderer refused, redone on the next frame (null = none).
        inline static const RE::TESObjectLIGH* _pendingRefreshForm = nullptr;
        inline static int _lightVolumeRebuildAttempts = 0;
    };
}
