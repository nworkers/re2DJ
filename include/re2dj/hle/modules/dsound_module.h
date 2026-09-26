#ifndef RE2DJ_HLE_MODULES_DSOUND_MODULE_H_
#define RE2DJ_HLE_MODULES_DSOUND_MODULE_H_

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// dsound.dll: DirectSoundCreate (ordinal 1), with the IDirectSound and
// IDirectSoundBuffer methods as exports named "<interface>::<method>" that
// their vtables point at. Buffers keep their samples in guest memory, where
// the guest's locks reach them; nothing plays them yet.
GuestModuleDescriptor MakeDsoundModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_DSOUND_MODULE_H_
