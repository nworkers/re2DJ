#ifndef RE2DJ_DEVICE_HARDLOCK_450_RESPONSE_H_
#define RE2DJ_DEVICE_HARDLOCK_450_RESPONSE_H_

#include <array>
#include <cstdint>
#include <string>

namespace re2dj::device
{

using Hardlock450Response = std::array<std::uint8_t, 6>;

bool ParseHardlock450Response(const std::string& hex,
                              Hardlock450Response* response,
                              std::string* error);

}  // namespace re2dj::device

#endif  // RE2DJ_DEVICE_HARDLOCK_450_RESPONSE_H_
