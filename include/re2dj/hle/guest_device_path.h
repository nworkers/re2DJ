#ifndef RE2DJ_HLE_GUEST_DEVICE_PATH_H_
#define RE2DJ_HLE_GUEST_DEVICE_PATH_H_

#include <string_view>

namespace re2dj::hle
{

// Whether a guest CreateFile name opens the device a profile names, such as
// "\\.\FEnteDev". The test is an ASCII case-insensitive prefix match, because
// some device names vary in their tail (the LPTDI port digit). An empty
// prefix matches nothing; a host that wants a fallback supplies it.
bool MatchesGuestDevicePrefix(std::string_view name, std::string_view prefix);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_DEVICE_PATH_H_
