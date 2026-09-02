#ifndef RE2DJ_HLE_HARDLOCK_PROTOCOL_H_
#define RE2DJ_HLE_HARDLOCK_PROTOCOL_H_

#include <cstdint>

namespace re2dj::hle::hardlock
{

// The four control codes the vendor driver exposes, confirmed by static
// analysis of the driver and by the original's own request sequence.
constexpr std::uint32_t kHardlockIoctlInitialize = 0x9c402468;
constexpr std::uint32_t kHardlockIoctlHandshake = 0x9c402450;
constexpr std::uint32_t kHardlockIoctlDescriptor = 0x9c40244c;
constexpr std::uint32_t kHardlockIoctlTransform = 0x9c402458;

enum class HardlockRequestKind
{
    kUnknown,
    kInitialize,
    kHandshake,
    kDescriptor,
    kTransform,
};

HardlockRequestKind ClassifyHardlockRequest(std::uint32_t control_code);

const char* HardlockRequestKindName(HardlockRequestKind kind);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_PROTOCOL_H_
