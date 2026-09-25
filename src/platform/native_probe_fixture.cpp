#include "native_probe_fixture.h"

#include <array>
#include <cstddef>

namespace re2dj::platform::native_probe
{
namespace
{

void PutU16(std::vector<std::uint8_t>* bytes, std::size_t offset, std::uint16_t value)
{
    (*bytes)[offset] = static_cast<std::uint8_t>(value);
    (*bytes)[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void PutU32(std::vector<std::uint8_t>* bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void PutString(std::vector<std::uint8_t>* bytes, std::size_t offset, const char* value)
{
    for (std::size_t index = 0;; ++index)
    {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(value[index]);
        if (value[index] == '\0')
        {
            return;
        }
    }
}

void PutSectionName(std::vector<std::uint8_t>* bytes,
                    std::size_t offset,
                    const char* value)
{
    for (std::size_t index = 0; index < 8 && value[index] != '\0'; ++index)
    {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(value[index]);
    }
}

}  // namespace

bool ProbeImportHandler(const re2dj::hle::ImportCall& call,
                        re2dj::hle::ImportReturn* result,
                        std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        if (error != nullptr)
        {
            *error = "synthetic import needs exactly one argument";
        }
        return false;
    }
    if (!call.gate.by_ordinal && call.arguments[0] == 41)
    {
        result->eax = 42;
        return true;
    }
    if (call.gate.by_ordinal && call.gate.ordinal == 7 && call.arguments[0] == 42)
    {
        result->eax = 43;
        result->edx = 1;
        return true;
    }
    if (error != nullptr)
    {
        *error = "unexpected synthetic import argument";
    }
    return false;
}

std::vector<std::uint8_t> MakeSyntheticPe32()
{
    constexpr std::size_t kPeOffset = 0x80;
    constexpr std::size_t kFileHeader = kPeOffset + 4;
    constexpr std::size_t kOptionalHeader = kFileHeader + 20;
    constexpr std::size_t kSectionTable = kOptionalHeader + 224;
    std::vector<std::uint8_t> bytes(0xC00, 0);

    bytes[0] = 'M';
    bytes[1] = 'Z';
    PutU32(&bytes, 0x3C, static_cast<std::uint32_t>(kPeOffset));
    bytes[kPeOffset] = 'P';
    bytes[kPeOffset + 1] = 'E';
    PutU16(&bytes, kFileHeader, 0x014C);
    PutU16(&bytes, kFileHeader + 2, 4);
    PutU16(&bytes, kFileHeader + 16, 224);
    PutU16(&bytes, kFileHeader + 18, 0x010E);

    PutU16(&bytes, kOptionalHeader, 0x010B);
    PutU32(&bytes, kOptionalHeader + 16, kEntryRva);
    PutU32(&bytes, kOptionalHeader + 28, kImageBase);
    PutU32(&bytes, kOptionalHeader + 32, 0x1000);
    PutU32(&bytes, kOptionalHeader + 36, 0x200);
    PutU32(&bytes, kOptionalHeader + 56, 0x11000);
    PutU32(&bytes, kOptionalHeader + 60, 0x400);
    PutU16(&bytes, kOptionalHeader + 68, 3);
    PutU32(&bytes, kOptionalHeader + 92, 16);
    PutU32(&bytes, kOptionalHeader + 96 + 8, 0x2000);
    PutU32(&bytes, kOptionalHeader + 96 + 12, 40);
    PutU32(&bytes, kOptionalHeader + 96 + 5 * 8, 0x4000);
    PutU32(&bytes, kOptionalHeader + 96 + 5 * 8 + 4, 32);
    PutU32(&bytes, kOptionalHeader + 96 + 9 * 8, 0x3000);
    PutU32(&bytes, kOptionalHeader + 96 + 9 * 8 + 4, 24);
    PutU32(&bytes, kOptionalHeader + 96 + 12 * 8, kIatRva);
    PutU32(&bytes, kOptionalHeader + 96 + 12 * 8 + 4, 12);

    PutSectionName(&bytes, kSectionTable, ".text");
    PutU32(&bytes, kSectionTable + 8, 0x100);
    PutU32(&bytes, kSectionTable + 12, 0x1000);
    PutU32(&bytes, kSectionTable + 16, 0x200);
    PutU32(&bytes, kSectionTable + 20, 0x400);
    PutU32(&bytes, kSectionTable + 36, 0x60000020);

    PutSectionName(&bytes, kSectionTable + 40, ".idata");
    PutU32(&bytes, kSectionTable + 48, 0x200);
    PutU32(&bytes, kSectionTable + 52, 0x2000);
    PutU32(&bytes, kSectionTable + 56, 0x200);
    PutU32(&bytes, kSectionTable + 60, 0x600);
    PutU32(&bytes, kSectionTable + 76, 0xC0000040);

    PutSectionName(&bytes, kSectionTable + 80, ".data");
    PutU32(&bytes, kSectionTable + 88, 0x100);
    PutU32(&bytes, kSectionTable + 92, 0x3000);
    PutU32(&bytes, kSectionTable + 96, 0x200);
    PutU32(&bytes, kSectionTable + 100, 0x800);
    PutU32(&bytes, kSectionTable + 116, 0xC0000040);

    PutSectionName(&bytes, kSectionTable + 120, ".reloc");
    PutU32(&bytes, kSectionTable + 128, 0x100);
    PutU32(&bytes, kSectionTable + 132, 0x4000);
    PutU32(&bytes, kSectionTable + 136, 0x200);
    PutU32(&bytes, kSectionTable + 140, 0xA00);
    PutU32(&bytes, kSectionTable + 156, 0x42000040);

    // Call the named import with 41, then the ordinal import with its result.
    bytes[0x400] = 0x6A;
    bytes[0x401] = 0x29;
    bytes[0x402] = 0xFF;
    bytes[0x403] = 0x15;
    PutU32(&bytes, 0x404, kImageBase + kIatRva);
    bytes[0x408] = 0x50;
    bytes[0x409] = 0xFF;
    bytes[0x40A] = 0x15;
    PutU32(&bytes, 0x40B, kImageBase + kIatRva + 4);
    bytes[0x40F] = 0x03;
    bytes[0x410] = 0xC2;
    bytes[0x411] = 0x03;
    bytes[0x412] = 0x05;
    PutU32(&bytes, 0x413, kImageBase + kTlsStateRva);
    bytes[0x417] = 0xC3;

    // TLS callback: validate FS self and TEB stack bounds, then set state = 7.
    const std::array<std::uint8_t, 49> tls_code = {
        0x64, 0xA1, 0x18, 0x00, 0x00, 0x00,
        0x3B, 0x40, 0x18,
        0x75, 0x19,
        0x89, 0xE1,
        0x3B, 0x48, 0x04,
        0x73, 0x12,
        0x3B, 0x48, 0x08,
        0x76, 0x0D,
        0xC7, 0x05, 0, 0, 0, 0, 7, 0, 0, 0,
        0xC2, 0x0C, 0x00,
        0xC7, 0x05, 0, 0, 0, 0, 0, 0, 0, 0,
        0xC2, 0x0C, 0x00,
    };
    for (std::size_t index = 0; index < tls_code.size(); ++index)
    {
        bytes[0x420 + index] = tls_code[index];
    }
    PutU32(&bytes, 0x439, kImageBase + kTlsStateRva);
    PutU32(&bytes, 0x446, kImageBase + kTlsStateRva);

    PutU32(&bytes, 0x600, 0x2060);
    PutU32(&bytes, 0x60C, 0x2080);
    PutU32(&bytes, 0x610, kIatRva);
    PutU32(&bytes, 0x640, 0x20A0);
    PutU32(&bytes, 0x644, 0x80000007);
    PutU32(&bytes, 0x660, 0x20A0);
    PutU32(&bytes, 0x664, 0x80000007);
    PutString(&bytes, 0x680, "probe.dll");
    PutU16(&bytes, 0x6A0, 0);
    PutString(&bytes, 0x6A2, "ProbeGate");

    PutU32(&bytes, 0x80C, kImageBase + kTlsCallbacksRva);
    PutU32(&bytes, 0x820, kImageBase + kTlsCallbackRva);

    PutU32(&bytes, 0xA00, 0x1000);
    PutU32(&bytes, 0xA04, 20);
    PutU16(&bytes, 0xA08, 0x3004);
    PutU16(&bytes, 0xA0A, 0x300B);
    PutU16(&bytes, 0xA0C, 0x3013);
    PutU16(&bytes, 0xA0E, 0x3039);
    PutU16(&bytes, 0xA10, 0x3046);
    PutU16(&bytes, 0xA12, 0x0000);
    PutU32(&bytes, 0xA14, 0x3000);
    PutU32(&bytes, 0xA18, 12);
    PutU16(&bytes, 0xA1C, 0x300C);
    PutU16(&bytes, 0xA1E, 0x3020);
    return bytes;
}

}  // namespace re2dj::platform::native_probe
