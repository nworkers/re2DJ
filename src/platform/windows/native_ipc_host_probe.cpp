#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/platform/windows/native_helper_backend.h"

namespace
{

constexpr std::uint32_t kImageBase = 0x10000000;
constexpr std::uint32_t kRequestedBase = 0x11000000;
constexpr std::uint32_t kEntryRva = 0x1000;
constexpr std::uint32_t kIatRva = 0x2040;
constexpr std::uint32_t kTlsCallbackRva = 0x1020;
constexpr std::uint32_t kTlsCallbacksRva = 0x3020;
constexpr std::uint32_t kTlsStateRva = 0x3030;

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
    PutU32(&bytes, kOptionalHeader + 56, 0x5000);
    PutU32(&bytes, kOptionalHeader + 60, 0x400);
    PutU16(&bytes, kOptionalHeader + 68, 3);
    PutU32(&bytes, kOptionalHeader + 92, 16);
    PutU32(&bytes, kOptionalHeader + 96 + 8, 0x2000);
    PutU32(&bytes, kOptionalHeader + 96 + 12, 40);
    PutU32(&bytes, kOptionalHeader + 96 + 5 * 8, 0x4000);
    PutU32(&bytes, kOptionalHeader + 96 + 5 * 8 + 4, 28);
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

    // TLS callback: state = 7; return and pop three callback arguments.
    bytes[0x420] = 0xC7;
    bytes[0x421] = 0x05;
    PutU32(&bytes, 0x422, kImageBase + kTlsStateRva);
    PutU32(&bytes, 0x426, 7);
    bytes[0x42A] = 0xC2;
    bytes[0x42B] = 0x0C;
    bytes[0x42C] = 0x00;

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
    PutU32(&bytes, 0xA04, 16);
    PutU16(&bytes, 0xA08, 0x3004);
    PutU16(&bytes, 0xA0A, 0x300B);
    PutU16(&bytes, 0xA0C, 0x3013);
    PutU16(&bytes, 0xA0E, 0x3022);
    PutU32(&bytes, 0xA10, 0x3000);
    PutU32(&bytes, 0xA14, 12);
    PutU16(&bytes, 0xA18, 0x300C);
    PutU16(&bytes, 0xA1A, 0x3020);
    return bytes;
}

}  // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: re2dj_native_ipc_host_probe <x86-helper>\n");
        return 1;
    }

    const std::vector<std::uint8_t> image = MakeSyntheticPe32();
    re2dj::exe::PeImageInfo info;
    std::string error;
    if (!re2dj::exe::ReadPeImageInfo(image.data(), image.size(), &info, &error))
    {
        std::fprintf(stderr, "native-ipc-host-probe: %s\n", error.c_str());
        return 2;
    }

    re2dj::platform::windows::NativeHelperBackend backend{
        std::filesystem::path(argv[1])};
    re2dj::runtime::LoadedPeImage loaded;
    bool success = backend.PrepareImage(image,
                                        info,
                                        re2dj::runtime::GuestAddress(kRequestedBase),
                                        &loaded,
                                        &error) &&
                   loaded.load_base.value() == kRequestedBase &&
                   loaded.entry_point.value() == kRequestedBase + kEntryRva &&
                   loaded.imports.size() == 2 &&
                   loaded.imports[0].module == "probe.dll" &&
                   loaded.imports[0].name == "ProbeGate" &&
                   !loaded.imports[0].by_ordinal &&
                   loaded.imports[0].address.value() ==
                       re2dj::runtime::kDefaultImportGateBase &&
                   loaded.imports[1].module == "probe.dll" &&
                   loaded.imports[1].by_ordinal &&
                   loaded.imports[1].ordinal == 7 &&
                   loaded.imports[1].address.value() ==
                       re2dj::runtime::kDefaultImportGateBase +
                           re2dj::runtime::kDefaultImportGateStride &&
                   backend.Start(&error);

    auto complete_gate = [&](std::size_t import_index,
                             std::uint64_t expected_event_id,
                             std::uint32_t expected_instruction_pointer,
                             std::uint32_t expected_argument,
                             std::uint32_t eax,
                             std::uint32_t edx) -> bool
    {
        re2dj::runtime::ExecutionEvent event;
        if (!backend.WaitForEvent(&event, &error) ||
            event.kind != re2dj::runtime::ExecutionEventKind::kImportGate ||
            event.event_id != expected_event_id || event.thread_id == 0 ||
            event.instruction_pointer.value() != expected_instruction_pointer ||
            event.stack_pointer.value() == 0 ||
            event.gate_address != loaded.imports[import_index].address)
        {
            return false;
        }

        std::array<std::uint8_t, 4> argument_bytes = {};
        if (!backend.ReadMemory(event.stack_pointer + 4, argument_bytes, &error))
        {
            return false;
        }
        const std::uint32_t argument =
            static_cast<std::uint32_t>(argument_bytes[0]) |
            (static_cast<std::uint32_t>(argument_bytes[1]) << 8) |
            (static_cast<std::uint32_t>(argument_bytes[2]) << 16) |
            (static_cast<std::uint32_t>(argument_bytes[3]) << 24);
        if (argument != expected_argument ||
            !backend.WriteMemory(event.stack_pointer + 4, argument_bytes, &error))
        {
            return false;
        }

        re2dj::runtime::ImportCompletion completion;
        completion.event_id = event.event_id;
        completion.eax = eax;
        completion.edx = edx;
        completion.stack_bytes_to_pop = 4;
        return backend.CompleteImport(completion, &error);
    };

    success = success && complete_gate(0, 1, kRequestedBase + kEntryRva + 8, 41, 42, 0);
    success = success && complete_gate(1, 2, kRequestedBase + kEntryRva + 15, 42, 43, 1);

    re2dj::runtime::ExecutionEvent exit_event;
    success = success && backend.WaitForEvent(&exit_event, &error) &&
              exit_event.kind == re2dj::runtime::ExecutionEventKind::kProcessExit &&
              exit_event.event_id == 3 && exit_event.status_code == 51;
    if (!success)
    {
        std::fprintf(stderr, "native-ipc-host-probe: %s\n", error.c_str());
        backend.RequestStop();
        return 3;
    }

    std::printf(
        "native-ipc-host-probe: load=0x%08x entry=0x%08x imports=2 arguments=41,42 "
        "result=%u child=0\n",
        loaded.load_base.value(),
        loaded.entry_point.value(),
        exit_event.status_code);
    return 0;
}
