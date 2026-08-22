#define NOMINMAX
#include <windows.h>

#include <intrin.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../native_helper_protocol.h"
#include "native_pe_image.h"
#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/pe_loader.h"

namespace
{

namespace protocol = re2dj::platform::native_protocol;
namespace windows_platform = re2dj::platform::windows;

HANDLE input_handle = INVALID_HANDLE_VALUE;
HANDLE output_handle = INVALID_HANDLE_VALUE;
std::uint32_t completion_stack_bytes = 0;
std::uint64_t next_event_id = 1;
bool gate_failed = false;

bool ReadExact(HANDLE handle, void* destination, std::uint32_t size)
{
    auto* bytes = static_cast<std::uint8_t*>(destination);
    std::uint32_t consumed = 0;
    while (consumed < size)
    {
        DWORD read = 0;
        if (!ReadFile(handle, bytes + consumed, size - consumed, &read, nullptr) || read == 0)
        {
            return false;
        }
        consumed += read;
    }
    return true;
}

bool WriteExact(HANDLE handle, const void* source, std::uint32_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    std::uint32_t consumed = 0;
    while (consumed < size)
    {
        DWORD written = 0;
        if (!WriteFile(handle, bytes + consumed, size - consumed, &written, nullptr) ||
            written == 0)
        {
            return false;
        }
        consumed += written;
    }
    return true;
}

bool SendPacket(protocol::MessageType type, const void* payload, std::uint32_t payload_size)
{
    protocol::MessageHeader header;
    header.type = static_cast<std::uint32_t>(type);
    header.payload_size = payload_size;
    return WriteExact(output_handle, &header, sizeof(header)) &&
           (payload_size == 0 || WriteExact(output_handle, payload, payload_size));
}

bool SendError(const std::string& message)
{
    return SendPacket(protocol::MessageType::kError,
                      message.data(),
                      static_cast<std::uint32_t>(message.size()));
}

bool ReceiveHeader(protocol::MessageHeader* header)
{
    return ReadExact(input_handle, header, sizeof(*header)) && header->magic == protocol::kMagic &&
           header->version == protocol::kVersion &&
           header->payload_size <= protocol::kMaximumPayloadSize;
}

extern "C" std::uint64_t __stdcall NativeImportGate(std::uint32_t gate_address);


bool SendMemory(std::uint32_t address, std::uint32_t size)
{
    if (size > 4096)
    {
        return SendError("memory request exceeds prototype limit");
    }
    std::vector<std::uint8_t> bytes(size);
    SIZE_T copied = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),
                           reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
                           bytes.data(),
                           size,
                           &copied) ||
        copied != size)
    {
        return SendError("cannot read requested guest memory");
    }
    return SendPacket(protocol::MessageType::kMemoryData, bytes.data(), size);
}

bool ReceiveAndWriteMemory(std::uint32_t payload_size)
{
    if (payload_size < sizeof(protocol::ReadMemoryRequest))
    {
        return SendError("write memory packet is truncated");
    }
    protocol::ReadMemoryRequest request;
    if (!ReadExact(input_handle, &request, sizeof(request)) || request.size > 4096 ||
        payload_size != sizeof(request) + request.size)
    {
        return SendError("write memory packet has an invalid size");
    }
    std::vector<std::uint8_t> bytes(request.size);
    if (request.size != 0 && !ReadExact(input_handle, bytes.data(), request.size))
    {
        return false;
    }
    SIZE_T copied = 0;
    protocol::WriteMemoryResult result;
    result.size = request.size;
    result.success = WriteProcessMemory(
                         GetCurrentProcess(),
                         reinterpret_cast<void*>(static_cast<std::uintptr_t>(request.address)),
                         bytes.data(),
                         request.size,
                         &copied) &&
                             copied == request.size
                         ? 1U
                         : 0U;
    return SendPacket(protocol::MessageType::kWriteResult, &result, sizeof(result));
}

bool SendImportMetadata(const re2dj::runtime::ImportGate& gate)
{
    if (gate.module.empty() ||
        gate.module.size() > protocol::kMaximumImportStringSize ||
        gate.name.size() > protocol::kMaximumImportStringSize ||
        (!gate.by_ordinal && gate.name.empty()) ||
        (gate.by_ordinal && !gate.name.empty()))
    {
        return false;
    }
    protocol::ImportMetadata metadata;
    metadata.gate_address = gate.address.value();
    metadata.by_ordinal = gate.by_ordinal ? 1U : 0U;
    metadata.ordinal = gate.ordinal;
    metadata.module_size = static_cast<std::uint32_t>(gate.module.size());
    metadata.name_size = static_cast<std::uint32_t>(gate.name.size());
    std::vector<std::uint8_t> payload(sizeof(metadata) + gate.module.size() +
                                      gate.name.size());
    std::memcpy(payload.data(), &metadata, sizeof(metadata));
    std::memcpy(payload.data() + sizeof(metadata),
                gate.module.data(),
                gate.module.size());
    if (!gate.name.empty())
    {
        std::memcpy(payload.data() + sizeof(metadata) + gate.module.size(),
                    gate.name.data(),
                    gate.name.size());
    }
    return SendPacket(protocol::MessageType::kImportMetadata,
                      payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

extern "C" __declspec(noinline) std::uint64_t __stdcall NativeImportGate(
    std::uint32_t gate_address)
{
    auto* bridge_return_slot = static_cast<std::uint8_t*>(_AddressOfReturnAddress());
    std::uint8_t* guest_return_slot = bridge_return_slot + 8;
    std::uint32_t return_address = 0;
    std::memcpy(&return_address, guest_return_slot, sizeof(return_address));

    const std::uint64_t event_id = next_event_id++;

    protocol::ExecutionEvent event;
    event.kind = static_cast<std::uint32_t>(protocol::EventKind::kImportGate);
    event.event_id_low = static_cast<std::uint32_t>(event_id);
    event.event_id_high = static_cast<std::uint32_t>(event_id >> 32);
    event.thread_id = GetCurrentThreadId();
    event.instruction_pointer = return_address;
    event.stack_pointer =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(guest_return_slot));
    event.gate_address = gate_address;
    if (!SendPacket(protocol::MessageType::kExecutionEvent, &event, sizeof(event)))
    {
        gate_failed = true;
        return 0;
    }

    for (;;)
    {
        protocol::MessageHeader header;
        if (!ReceiveHeader(&header))
        {
            gate_failed = true;
            return 0;
        }
        const auto type = static_cast<protocol::MessageType>(header.type);
        if (type == protocol::MessageType::kReadMemory &&
            header.payload_size == sizeof(protocol::ReadMemoryRequest))
        {
            protocol::ReadMemoryRequest request;
            if (!ReadExact(input_handle, &request, sizeof(request)) ||
                !SendMemory(request.address, request.size))
            {
                gate_failed = true;
                return 0;
            }
            continue;
        }
        if (type == protocol::MessageType::kWriteMemory)
        {
            if (!ReceiveAndWriteMemory(header.payload_size))
            {
                gate_failed = true;
                return 0;
            }
            continue;
        }
        if (type == protocol::MessageType::kCompleteImport &&
            header.payload_size == sizeof(protocol::CompleteImport))
        {
            protocol::CompleteImport completion;
            if (!ReadExact(input_handle, &completion, sizeof(completion)) ||
                completion.event_id_low != static_cast<std::uint32_t>(event_id) ||
                completion.event_id_high != static_cast<std::uint32_t>(event_id >> 32) ||
                completion.action != 0)
            {
                gate_failed = true;
                return 0;
            }
            completion_stack_bytes = completion.stack_bytes_to_pop;
            return (static_cast<std::uint64_t>(completion.edx) << 32) |
                   completion.eax;
        }
        gate_failed = true;
        return 0;
    }
}

}  // namespace

int main()
{
    static_assert(sizeof(void*) == 4, "native IPC helper must be built for Win32 x86");
    input_handle = GetStdHandle(STD_INPUT_HANDLE);
    output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input_handle == INVALID_HANDLE_VALUE || output_handle == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    protocol::MessageHeader load_header;
    if (!ReceiveHeader(&load_header) ||
        static_cast<protocol::MessageType>(load_header.type) !=
            protocol::MessageType::kLoadImage ||
        load_header.payload_size < sizeof(protocol::LoadImageRequest))
    {
        SendError("expected LoadImage packet");
        return 2;
    }
    protocol::LoadImageRequest load_request;
    if (!ReadExact(input_handle, &load_request, sizeof(load_request)) ||
        load_request.file_size == 0 ||
        load_header.payload_size != sizeof(load_request) + load_request.file_size)
    {
        SendError("invalid LoadImage payload");
        return 3;
    }
    std::vector<std::uint8_t> file(load_request.file_size);
    if (!ReadExact(input_handle, file.data(), load_request.file_size))
    {
        return 3;
    }

    re2dj::exe::PeImageInfo info;
    re2dj::runtime::ImportGateTable gates;
    windows_platform::NativePeImage image;
    std::string error;
    protocol::LoadResult load_result;
    if (!re2dj::exe::ReadPeImageInfo(file.data(), file.size(), &info, &error) ||
        !windows_platform::MapNativePeImage(
            file,
            info,
            load_request.requested_base,
            reinterpret_cast<std::uintptr_t>(&NativeImportGate),
            reinterpret_cast<std::uintptr_t>(&completion_stack_bytes),
            &gates,
            &image,
            &error))
    {
        SendError(error);
        return 4;
    }
    load_result.success = 1;
    load_result.load_base = image.load_base;
    load_result.entry_point = image.entry_point;
    load_result.import_count = static_cast<std::uint32_t>(gates.gates().size());
    if (!SendPacket(protocol::MessageType::kLoadResult, &load_result, sizeof(load_result)))
    {
        return 5;
    }
    for (const re2dj::runtime::ImportGate& gate : gates.gates())
    {
        if (!SendImportMetadata(gate))
        {
            return 6;
        }
    }

    protocol::MessageHeader start_header;
    if (!ReceiveHeader(&start_header) ||
        static_cast<protocol::MessageType>(start_header.type) != protocol::MessageType::kStart ||
        start_header.payload_size != 0)
    {
        SendError("expected Start packet");
        return 7;
    }
    if (!windows_platform::RunNativeTlsCallbacks(info, image, &error))
    {
        SendError(error);
        windows_platform::ReleaseNativePeImage(&image);
        return 8;
    }

    using GuestEntry = std::uint32_t(__cdecl*)();
#pragma warning(suppress : 4191)
    const GuestEntry entry = reinterpret_cast<GuestEntry>(
        static_cast<std::uintptr_t>(load_result.entry_point));
    const std::uint32_t result = entry();

    protocol::ExecutionEvent exit_event;
    exit_event.kind = static_cast<std::uint32_t>(protocol::EventKind::kProcessExit);
    const std::uint64_t exit_event_id = next_event_id++;
    exit_event.event_id_low = static_cast<std::uint32_t>(exit_event_id);
    exit_event.event_id_high = static_cast<std::uint32_t>(exit_event_id >> 32);
    exit_event.thread_id = GetCurrentThreadId();
    exit_event.instruction_pointer = load_result.entry_point;
    exit_event.status_code = result;
    if (!SendPacket(protocol::MessageType::kExecutionEvent,
                    &exit_event,
                    sizeof(exit_event)))
    {
        return 9;
    }
    windows_platform::ReleaseNativePeImage(&image);
    return gate_failed ? 10 : 0;
}
