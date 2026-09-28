#ifndef RE2DJ_HLE_MODULES_DINPUT_MODULE_H_
#define RE2DJ_HLE_MODULES_DINPUT_MODULE_H_

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// dinput.dll: DirectInputCreateA, with the IDirectInputA and
// IDirectInputDeviceA methods as exports named "<interface>::<method>" that
// their vtables point at. The system keyboard and mouse report nothing held
// until the host's input reaches them.
GuestModuleDescriptor MakeDinputModuleDescriptor();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_DINPUT_MODULE_H_
