#ifndef RE2DJ_HLE_HARDLOCK_API_DESCRIPTOR_H_
#define RE2DJ_HLE_HARDLOCK_API_DESCRIPTOR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace re2dj::hle::hardlock
{

constexpr std::size_t kHardlockApiDescriptorSize = 256;
constexpr std::size_t kHardlockApiFixedHeaderSize = 52;
constexpr std::size_t kHardlockApiTailWordOffset = 0xfe;

struct HardlockApiDescriptorHeader
{
    std::array<std::uint8_t, 2> api_version = {};
    std::uint16_t module_id = 0;
    std::uint16_t module_address = 0;
    std::uint32_t data_address = 0;
    std::uint16_t block_count = 0;
    std::uint16_t function = 0;
    std::uint16_t status = 0;
    std::uint16_t remote = 0;
    std::uint16_t port = 0;
    std::uint16_t speed = 0;
    std::uint16_t network_users = 0;
    std::array<std::uint8_t, 8> id_reference = {};
    std::array<std::uint8_t, 8> id_verify = {};

    bool operator==(const HardlockApiDescriptorHeader&) const = default;
};

bool ParseHardlockApiDescriptorHeader(
    std::span<const std::uint8_t> bytes,
    HardlockApiDescriptorHeader* header);

bool ParseHardlockApiDescriptorTailWord(
    std::span<const std::uint8_t> bytes,
    std::uint16_t* tail_word);

bool ParseHardlockApiTailWordHex(const std::string& hex,
                                 std::uint16_t* tail_word,
                                 std::string* error);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_API_DESCRIPTOR_H_
