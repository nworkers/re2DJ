#ifndef RE2DJ_GRAPHICS_PRESENT_SYNC_H_
#define RE2DJ_GRAPHICS_PRESENT_SYNC_H_

#include <string_view>

namespace re2dj::graphics
{

// When a present is allowed to return. The host picks this; the value stays
// platform-neutral so every host expresses the same policy.
//
// This sits in its own header because the policy is chosen well outside the
// graphics backend — a target profile and the product command line both carry
// it — and those layers should not pull in the backend interface to name it.
enum class PresentSync
{
    // Every present blocks until the next vertical retrace. This caps the
    // guest at the display's refresh rate and is what the driver default
    // happened to give before the policy became explicit, so it stays the
    // default everywhere.
    kVerticalSync,
    // Presents never block. Tearing is allowed and the guest's own pacing is
    // the only limiter left.
    kImmediate,
    // A frame that met the refresh deadline waits for it; a frame that missed
    // it does not wait for the following one. Drivers may refuse this, in
    // which case the backend falls back to kVerticalSync.
    kAdaptive,
};

// The canonical spelling of a policy, and its inverse. These are the words the
// launcher option carries between processes, so the two directions live
// together and stay each other's inverse. The product command line spells the
// same choices as on/off/adaptive for its users and maps to these.
const char* PresentSyncName(PresentSync value);
bool ParsePresentSyncName(std::string_view text, PresentSync* value);

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_PRESENT_SYNC_H_
