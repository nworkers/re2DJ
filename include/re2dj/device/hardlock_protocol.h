#ifndef RE2DJ_DEVICE_HARDLOCK_PROTOCOL_H_
#define RE2DJ_DEVICE_HARDLOCK_PROTOCOL_H_

#include <cstddef>
#include <cstdint>
#include <span>

namespace re2dj::device
{

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

struct HardlockRequestObservation
{
    HardlockRequestKind kind = HardlockRequestKind::kUnknown;
    bool shape_valid = false;
    bool sequence_valid = false;
    bool descriptor_present = false;
    bool descriptor_valid = false;
    bool module_address_matches = false;
    std::uint16_t function = 0;
    std::uint16_t block_count = 0;
};

class HardlockProtocolTracker
{
public:
    HardlockRequestObservation Observe(std::uint32_t control_code,
                                       std::span<const std::uint8_t> input,
                                       std::size_t output_size,
                                       std::uint16_t configured_module_address);

private:
    enum class Stage
    {
        kStart,
        kInitialized,
        kHandshake,
        kDescriptor,
        kTransform,
    };

    Stage stage_ = Stage::kStart;
};

const char* HardlockRequestKindName(HardlockRequestKind kind);

}  // namespace re2dj::device

#endif  // RE2DJ_DEVICE_HARDLOCK_PROTOCOL_H_
