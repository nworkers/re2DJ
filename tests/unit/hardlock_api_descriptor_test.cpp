#include "re2dj/device/hardlock_api_descriptor.h"

#include <array>
#include <cstdint>
#include <span>

#include "test_support.h"

void RunHardlockApiDescriptorTests(re2dj::test::Context& context)
{
    std::array<std::uint8_t, re2dj::device::kHardlockApiDescriptorSize> bytes = {};
    bytes[0x00] = 0x03;
    bytes[0x01] = 0x47;
    bytes[0x06] = 0x34;
    bytes[0x07] = 0x12;
    bytes[0x08] = 0x78;
    bytes[0x09] = 0x56;
    bytes[0x12] = 0x44;
    bytes[0x13] = 0x33;
    bytes[0x14] = 0x22;
    bytes[0x15] = 0x11;
    bytes[0x16] = 0x01;
    bytes[0x18] = 0x0e;
    bytes[0x1a] = 0x07;
    bytes[0x1c] = 0x03;
    bytes[0x1e] = 0x78;
    bytes[0x1f] = 0x03;
    bytes[0x20] = 0x02;
    bytes[0x22] = 0x05;
    for (std::size_t index = 0; index < 8; ++index)
    {
        bytes[0x24 + index] = static_cast<std::uint8_t>(0x10 + index);
        bytes[0x2c + index] = static_cast<std::uint8_t>(0xa0 + index);
    }
    bytes[0xfe] = 0x34;
    bytes[0xff] = 0x12;

    re2dj::device::HardlockApiDescriptorHeader header;
    RE2DJ_CHECK(context,
                re2dj::device::ParseHardlockApiDescriptorHeader(bytes, &header));
    RE2DJ_CHECK_EQ(context, header.api_version,
                   (std::array<std::uint8_t, 2>{0x03, 0x47}));
    RE2DJ_CHECK_EQ(context, header.module_id, std::uint16_t{0x1234});
    RE2DJ_CHECK_EQ(context, header.module_address, std::uint16_t{0x5678});
    RE2DJ_CHECK_EQ(context, header.data_address, std::uint32_t{0x11223344});
    RE2DJ_CHECK_EQ(context, header.block_count, std::uint16_t{1});
    RE2DJ_CHECK_EQ(context, header.function, std::uint16_t{0x0e});
    RE2DJ_CHECK_EQ(context, header.status, std::uint16_t{7});
    RE2DJ_CHECK_EQ(context, header.remote, std::uint16_t{3});
    RE2DJ_CHECK_EQ(context, header.port, std::uint16_t{0x378});
    RE2DJ_CHECK_EQ(context, header.speed, std::uint16_t{2});
    RE2DJ_CHECK_EQ(context, header.network_users, std::uint16_t{5});
    RE2DJ_CHECK_EQ(context, header.id_reference,
                   (std::array<std::uint8_t, 8>{0x10, 0x11, 0x12, 0x13,
                                                0x14, 0x15, 0x16, 0x17}));
    RE2DJ_CHECK_EQ(context, header.id_verify,
                   (std::array<std::uint8_t, 8>{0xa0, 0xa1, 0xa2, 0xa3,
                                                0xa4, 0xa5, 0xa6, 0xa7}));
    std::uint16_t tail_word = 0;
    RE2DJ_CHECK(context,
                re2dj::device::ParseHardlockApiDescriptorTailWord(
                    bytes, &tail_word));
    RE2DJ_CHECK_EQ(context, tail_word, std::uint16_t{0x1234});

    const std::span<const std::uint8_t> short_bytes(
        bytes.data(), re2dj::device::kHardlockApiFixedHeaderSize - 1);
    RE2DJ_CHECK(context,
                !re2dj::device::ParseHardlockApiDescriptorHeader(short_bytes, &header));
    RE2DJ_CHECK(context,
                !re2dj::device::ParseHardlockApiDescriptorHeader(bytes, nullptr));
    RE2DJ_CHECK(context,
                !re2dj::device::ParseHardlockApiDescriptorTailWord(
                    short_bytes, &tail_word));
    RE2DJ_CHECK(context,
                !re2dj::device::ParseHardlockApiDescriptorTailWord(
                    bytes, nullptr));
    std::string error;
    RE2DJ_CHECK(context,
                re2dj::device::ParseHardlockApiTailWordHex(
                    "00fA", &tail_word, &error));
    RE2DJ_CHECK_EQ(context, tail_word, std::uint16_t{0x00fa});
    RE2DJ_CHECK(context,
                !re2dj::device::ParseHardlockApiTailWordHex(
                    "001", &tail_word, &error));
    RE2DJ_CHECK(context,
                !re2dj::device::ParseHardlockApiTailWordHex(
                    "00xz", &tail_word, &error));
}
