#ifndef RE2DJ_HLE_HARDLOCK_HANDSHAKE_RESPONSE_H_
#define RE2DJ_HLE_HARDLOCK_HANDSHAKE_RESPONSE_H_

#include <array>
#include <cstdint>
#include <string>

namespace re2dj::hle::hardlock
{

using HardlockHandshakeResponse = std::array<std::uint8_t, 6>;

bool ParseHardlockHandshakeResponse(const std::string& hex,
                              HardlockHandshakeResponse* response,
                              std::string* error);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_HANDSHAKE_RESPONSE_H_
