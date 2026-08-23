#ifndef RE2DJ_DEVICE_LPTDI_CHALLENGE_RESPONSE_H_
#define RE2DJ_DEVICE_LPTDI_CHALLENGE_RESPONSE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace re2dj::device
{

constexpr std::size_t kLptdiTargetStateSize = 8;
using LptdiTargetState = std::array<std::uint8_t, kLptdiTargetStateSize>;

std::uint32_t AdvanceLptdiChallenge(std::uint32_t value);

LptdiTargetState ComputeLptdiChallengeMask(std::uint32_t seed);

LptdiTargetState EncodeLptdiTargetState(std::uint32_t seed,
                                        const LptdiTargetState& target_state);

bool ParseLptdiTargetState(std::string_view text,
                           LptdiTargetState* target_state,
                           std::string* error);

}  // namespace re2dj::device

#endif  // RE2DJ_DEVICE_LPTDI_CHALLENGE_RESPONSE_H_
