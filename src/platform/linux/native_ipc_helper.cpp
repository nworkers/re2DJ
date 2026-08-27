#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../native_helper_protocol.h"
#include "native_import_thunks.h"
#include "native_process_bootstrap.h"
#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/pe_loader.h"

namespace
{

namespace protocol = re2dj::platform::native_protocol;

std::uint32_t pending_stack_start = 0;
std::uint32_t pending_stack_size = 0;
std::uint32_t completion_stack_bytes = 0;
std::uint64_t next_event_id = 1;
bool gate_failed = false;

struct MappedImage
{
    void* memory = nullptr;
    std::uint32_t size = 0;
    std::uint32_t entry_point = 0;
};

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void WriteU32(std::uint8_t* bytes, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

bool ApplyHighLowRelocations(const re2dj::exe::PeImageInfo& info,
                             MappedImage* image)
{
    const std::int64_t delta = static_cast<std::int64_t>(image->entry_point - info.entry_point_rva) -
                               static_cast<std::int64_t>(info.image_base);
    if (delta == 0)
    {
        return true;
    }
    const auto* directory = info.Directory(re2dj::exe::PeDirectoryIndex::kBaseRelocation);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size < 8 ||
        directory->virtual_address > image->size ||
        directory->size > image->size - directory->virtual_address)
    {
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(image->memory);
    std::uint32_t offset = 0;
    while (offset < directory->size)
    {
        if (directory->size - offset < 8)
        {
            return false;
        }
        const std::uint32_t page_rva = ReadU32(bytes + directory->virtual_address + offset);
        const std::uint32_t block_size = ReadU32(bytes + directory->virtual_address + offset + 4);
        if (block_size < 8 || block_size > directory->size - offset || (block_size & 1U) != 0)
        {
            return false;
        }
        for (std::uint32_t entry_offset = 8; entry_offset < block_size; entry_offset += 2)
        {
            const auto entry = static_cast<std::uint16_t>(
                bytes[directory->virtual_address + offset + entry_offset] |
                (static_cast<std::uint16_t>(bytes[directory->virtual_address + offset + entry_offset + 1]) << 8));
            const std::uint16_t type = entry >> 12;
            if (type == 0)
            {
                continue;
            }
            const std::uint32_t target_rva = page_rva + (entry & 0x0FFFU);
            if (type != 3 || target_rva > image->size || 4 > image->size - target_rva)
            {
                return false;
            }
            WriteU32(bytes + target_rva,
                     static_cast<std::uint32_t>(static_cast<std::int64_t>(ReadU32(bytes + target_rva)) + delta));
        }
        offset += block_size;
    }
    return offset == directory->size;
}

bool ReadImageString(const MappedImage& image, std::uint32_t rva, std::string* value)
{
    value->clear();
    if (rva >= image.size)
    {
        return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(image.memory);
    for (std::uint32_t index = rva; index < image.size && value->size() < 4096; ++index)
    {
        if (bytes[index] == 0)
        {
            return !value->empty();
        }
        value->push_back(static_cast<char>(bytes[index]));
    }
    return false;
}

[[maybe_unused]] bool CollectImports(const re2dj::exe::PeImageInfo& info, const MappedImage& image,
                    re2dj::runtime::ImportGateTable* gates, std::string* error)
{
    const auto* directory = info.Directory(re2dj::exe::PeDirectoryIndex::kImport);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return true;
    }
    if (directory->size < 20 || directory->virtual_address > image.size ||
        directory->size > image.size - directory->virtual_address)
    {
        return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(image.memory);
    for (std::uint32_t offset = 0; offset + 20 <= directory->size; offset += 20)
    {
        const auto* descriptor = bytes + directory->virtual_address + offset;
        if (ReadU32(descriptor) == 0 && ReadU32(descriptor + 12) == 0 && ReadU32(descriptor + 16) == 0)
        {
            return true;
        }
        const std::uint32_t lookup_rva = ReadU32(descriptor) != 0 ? ReadU32(descriptor) : ReadU32(descriptor + 16);
        std::string module;
        if (lookup_rva == 0 || !ReadImageString(image, ReadU32(descriptor + 12), &module))
        {
            return false;
        }
        for (std::uint32_t index = 0; lookup_rva <= image.size - 4 && index <= image.size / 4; ++index)
        {
            const std::uint32_t thunk_rva = lookup_rva + index * 4;
            if (thunk_rva > image.size || 4 > image.size - thunk_rva)
            {
                return false;
            }
            const std::uint32_t value = ReadU32(bytes + thunk_rva);
            if (value == 0)
            {
                break;
            }
            re2dj::runtime::GuestAddress address;
            if ((value & 0x80000000U) != 0)
            {
                if (!gates->BindByOrdinal(module, static_cast<std::uint16_t>(value), &address, error)) return false;
            }
            else
            {
                std::string name;
                if (value > image.size - 2 || !ReadImageString(image, value + 2, &name) ||
                    !gates->BindByName(module, name, &address, error)) return false;
            }
        }
    }
    return false;
}

void ReleaseMappedImage(MappedImage* image)
{
    if (image->memory != nullptr)
    {
        munmap(image->memory, image->size);
    }
    *image = {};
}

bool MapPe32Image(const std::vector<std::uint8_t>& file,
                  const re2dj::exe::PeImageInfo& info,
                  std::uint32_t requested_base,
                  MappedImage* image)
{
    if (!re2dj::exe::IsGuestExecutable(info) || requested_base == 0 ||
        info.size_of_image == 0 || info.size_of_headers > file.size() ||
        info.size_of_headers > info.size_of_image ||
        requested_base > UINT32_MAX - info.entry_point_rva)
    {
        return false;
    }
    void* memory = mmap(reinterpret_cast<void*>(static_cast<std::uintptr_t>(requested_base)),
                        info.size_of_image,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if (memory == MAP_FAILED ||
        reinterpret_cast<std::uintptr_t>(memory) != requested_base)
    {
        if (memory != MAP_FAILED)
        {
            munmap(memory, info.size_of_image);
        }
        return false;
    }
    std::memcpy(memory, file.data(), info.size_of_headers);
    for (const re2dj::exe::PeSection& section : info.sections)
    {
        const std::uint32_t size = section.virtual_size != 0 ?
            section.virtual_size : section.raw_size;
        if (size > info.size_of_image - section.virtual_address ||
            section.raw_offset > file.size() || section.raw_size > file.size() - section.raw_offset)
        {
            munmap(memory, info.size_of_image);
            return false;
        }
        const std::uint32_t copy_size = std::min(size, section.raw_size);
        if (copy_size != 0)
        {
            std::memcpy(static_cast<std::uint8_t*>(memory) + section.virtual_address,
                        file.data() + section.raw_offset,
                        copy_size);
        }
    }
    image->memory = memory;
    image->size = info.size_of_image;
    image->entry_point = requested_base + info.entry_point_rva;
    if (!ApplyHighLowRelocations(info, image))
    {
        ReleaseMappedImage(image);
        return false;
    }
    return true;
}

bool RunTlsCallbacks(const re2dj::exe::PeImageInfo& info,
                     const MappedImage& image,
                     re2dj::platform::linux::NativeProcessBootstrap* bootstrap,
                     re2dj::platform::linux::NativeGuestFault* fault,
                     std::string* error)
{
    const auto* directory = info.Directory(re2dj::exe::PeDirectoryIndex::kTls);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return true;
    }
    if (directory->size < 24 || directory->virtual_address > image.size ||
        24 > image.size - directory->virtual_address)
    {
        return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(image.memory);
    const std::uint32_t callbacks = ReadU32(bytes + directory->virtual_address + 12);
    const std::uint32_t base = image.entry_point - info.entry_point_rva;
    if (callbacks == 0)
    {
        return true;
    }
    if (callbacks < base || callbacks - base >= image.size)
    {
        return false;
    }
    for (std::uint32_t index = 0; index <= image.size / 4; ++index)
    {
        const std::uint32_t rva = callbacks - base + index * 4;
        if (rva > image.size || 4 > image.size - rva)
        {
            return false;
        }
        const std::uint32_t callback_address = ReadU32(bytes + rva);
        if (callback_address == 0)
        {
            return true;
        }
        if (callback_address < base || callback_address - base >= image.size)
        {
            return false;
        }
        if (!bootstrap->RunTlsCallback(callback_address, base, fault, error))
        {
            return false;
        }
    }
    return false;
}

bool ReadExact(int descriptor, void* destination, std::uint32_t size)
{
    auto* bytes = static_cast<std::uint8_t*>(destination);
    std::uint32_t consumed = 0;
    while (consumed < size)
    {
        const ssize_t result = read(descriptor, bytes + consumed, size - consumed);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result <= 0)
        {
            return false;
        }
        consumed += static_cast<std::uint32_t>(result);
    }
    return true;
}

bool WriteExact(int descriptor, const void* source, std::uint32_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    std::uint32_t consumed = 0;
    while (consumed < size)
    {
        const ssize_t result = write(descriptor, bytes + consumed, size - consumed);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result <= 0)
        {
            return false;
        }
        consumed += static_cast<std::uint32_t>(result);
    }
    return true;
}

bool SendPacket(protocol::MessageType type,
                const void* payload,
                std::uint32_t payload_size)
{
    protocol::MessageHeader header;
    header.type = static_cast<std::uint32_t>(type);
    header.payload_size = payload_size;
    return WriteExact(STDOUT_FILENO, &header, sizeof(header)) &&
           (payload_size == 0 || WriteExact(STDOUT_FILENO, payload, payload_size));
}

bool SendError(const std::string& message)
{
    return SendPacket(protocol::MessageType::kError,
                      message.data(),
                      static_cast<std::uint32_t>(message.size()));
}

bool ReceiveHeader(protocol::MessageHeader* header)
{
    return ReadExact(STDIN_FILENO, header, sizeof(*header)) &&
           header->magic == protocol::kMagic && header->version == protocol::kVersion &&
           header->payload_size <= protocol::kMaximumPayloadSize;
}

bool SendImportMetadata(const re2dj::runtime::ImportGate& gate)
{
    protocol::ImportMetadata metadata;
    metadata.gate_address = gate.address.value();
    metadata.by_ordinal = gate.by_ordinal ? 1U : 0U;
    metadata.ordinal = gate.ordinal;
    metadata.module_size = static_cast<std::uint32_t>(gate.module.size());
    metadata.name_size = static_cast<std::uint32_t>(gate.name.size());
    std::vector<std::uint8_t> payload(sizeof(metadata) + gate.module.size() + gate.name.size());
    std::memcpy(payload.data(), &metadata, sizeof(metadata));
    std::memcpy(payload.data() + sizeof(metadata), gate.module.data(), gate.module.size());
    if (!gate.name.empty())
    {
        std::memcpy(payload.data() + sizeof(metadata) + gate.module.size(), gate.name.data(), gate.name.size());
    }
    return SendPacket(protocol::MessageType::kImportMetadata,
                      payload.data(), static_cast<std::uint32_t>(payload.size()));
}

bool SendFaultEvent(const re2dj::platform::linux::NativeGuestFault& fault)
{
    protocol::ExecutionEvent event;
    event.kind = static_cast<std::uint32_t>(protocol::EventKind::kFault);
    const std::uint64_t event_id = next_event_id++;
    event.event_id_low = static_cast<std::uint32_t>(event_id);
    event.event_id_high = static_cast<std::uint32_t>(event_id >> 32);
    event.thread_id = static_cast<std::uint32_t>(syscall(SYS_gettid));
    event.instruction_pointer = fault.instruction_pointer;
    event.stack_pointer = fault.stack_pointer;
    event.status_code = fault.status_code;
    return SendPacket(protocol::MessageType::kExecutionEvent, &event, sizeof(event));
}

bool MemoryRangeAllowed(std::uint32_t address, std::uint32_t size)
{
    const std::uint64_t start = pending_stack_start;
    const std::uint64_t end = start + pending_stack_size;
    const std::uint64_t request_start = address;
    const std::uint64_t request_end = request_start + size;
    return request_start >= start && request_end >= request_start && request_end <= end;
}

bool SendMemory(const protocol::ReadMemoryRequest& request)
{
    if (!MemoryRangeAllowed(request.address, request.size))
    {
        return false;
    }
    std::vector<std::uint8_t> bytes(request.size);
    if (request.size != 0)
    {
        std::memcpy(bytes.data(),
                    reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(request.address)),
                    request.size);
    }
    return SendPacket(protocol::MessageType::kMemoryData,
                      bytes.data(),
                      request.size);
}

bool ReceiveAndWriteMemory(std::uint32_t payload_size)
{
    protocol::ReadMemoryRequest request;
    if (payload_size < sizeof(request) ||
        !ReadExact(STDIN_FILENO, &request, sizeof(request)) ||
        payload_size != sizeof(request) + request.size ||
        !MemoryRangeAllowed(request.address, request.size))
    {
        return false;
    }
    std::vector<std::uint8_t> bytes(request.size);
    if (request.size != 0 &&
        !ReadExact(STDIN_FILENO, bytes.data(), request.size))
    {
        return false;
    }
    if (request.size != 0)
    {
        std::memcpy(reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>(request.address)),
                    bytes.data(),
                    request.size);
    }
    protocol::WriteMemoryResult result;
    result.success = 1;
    result.size = request.size;
    return SendPacket(protocol::MessageType::kWriteResult, &result, sizeof(result));
}

extern "C" __attribute__((noinline, stdcall)) std::uint64_t NativeImportGate(
    std::uint32_t gate_address)
{
    auto* frame = static_cast<std::uint8_t*>(__builtin_frame_address(0));
    std::uint8_t* bridge_return_slot = frame + sizeof(void*);
    std::uint8_t* return_slot = bridge_return_slot + 2 * sizeof(void*);
    std::uint32_t return_address = 0;
    std::memcpy(&return_address, return_slot, sizeof(return_address));
    pending_stack_start = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(return_slot));
    pending_stack_size = 4096;

    const std::uint64_t event_id = next_event_id++;

    protocol::ExecutionEvent event;
    event.kind = static_cast<std::uint32_t>(protocol::EventKind::kImportGate);
    event.event_id_low = static_cast<std::uint32_t>(event_id);
    event.event_id_high = static_cast<std::uint32_t>(event_id >> 32);
    event.thread_id = static_cast<std::uint32_t>(syscall(SYS_gettid));
    event.instruction_pointer = return_address;
    event.stack_pointer = pending_stack_start;
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
            return 0;
        }
        const auto type = static_cast<protocol::MessageType>(header.type);
        if (type == protocol::MessageType::kReadMemory &&
            header.payload_size == sizeof(protocol::ReadMemoryRequest))
        {
            protocol::ReadMemoryRequest request;
            if (!ReadExact(STDIN_FILENO, &request, sizeof(request)) ||
                !SendMemory(request))
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
            if (!ReadExact(STDIN_FILENO, &completion, sizeof(completion)) ||
                completion.event_id_low != static_cast<std::uint32_t>(event_id) ||
                completion.event_id_high != static_cast<std::uint32_t>(event_id >> 32) ||
                completion.action != 0)
            {
                gate_failed = true;
                return 0;
            }
            pending_stack_start = 0;
            pending_stack_size = 0;
            completion_stack_bytes = completion.stack_bytes_to_pop;
            return (static_cast<std::uint64_t>(completion.edx) << 32) | completion.eax;
        }
        gate_failed = true;
        return 0;
    }
}

}  // namespace

int main()
{
    static_assert(sizeof(void*) == 4, "Linux native helper must be i386");
    protocol::MessageHeader request;
    if (!ReceiveHeader(&request))
    {
        return 1;
    }

    if (static_cast<protocol::MessageType>(request.type) == protocol::MessageType::kLoadImage)
    {
        protocol::LoadImageRequest load;
        if (request.payload_size < sizeof(load) || !ReadExact(STDIN_FILENO, &load, sizeof(load)) ||
            load.file_size == 0 || request.payload_size != sizeof(load) + load.file_size)
        {
            return 2;
        }
        std::vector<std::uint8_t> file(load.file_size);
        if (!ReadExact(STDIN_FILENO, file.data(), load.file_size))
        {
            return 3;
        }
        re2dj::exe::PeImageInfo info;
        std::string error;
        MappedImage image;
        re2dj::runtime::ImportGateTable gates;
        re2dj::platform::linux::NativeImportThunkRegion thunks;
        re2dj::platform::linux::NativeProcessBootstrap bootstrap;
        if (!re2dj::exe::ReadPeImageInfo(file.data(), file.size(), &info, &error) ||
            !MapPe32Image(file, info, load.requested_base, &image) ||
            !re2dj::platform::linux::BindNativeImportThunks(
                info,
                image.memory,
                image.size,
                reinterpret_cast<std::uintptr_t>(&NativeImportGate),
                reinterpret_cast<std::uintptr_t>(&completion_stack_bytes),
                &gates,
                &thunks,
                &error) ||
            !bootstrap.Initialize(load.requested_base, &error))
        {
            SendError(error.empty() ? "cannot map native PE image" : error);
            return 4;
        }
        protocol::LoadResult result;
        result.success = 1;
        result.load_base = load.requested_base;
        result.entry_point = image.entry_point;
        result.import_count = static_cast<std::uint32_t>(gates.gates().size());
        if (!SendPacket(protocol::MessageType::kLoadResult, &result, sizeof(result)))
        {
            ReleaseMappedImage(&image);
            re2dj::platform::linux::ReleaseNativeImportThunks(&thunks);
            return 5;
        }
        for (const re2dj::runtime::ImportGate& gate : gates.gates())
        {
            if (!SendImportMetadata(gate))
            {
                ReleaseMappedImage(&image);
                re2dj::platform::linux::ReleaseNativeImportThunks(&thunks);
                return 5;
            }
        }
        if (!ReceiveHeader(&request) ||
            static_cast<protocol::MessageType>(request.type) != protocol::MessageType::kStart ||
            request.payload_size != 0)
        {
            ReleaseMappedImage(&image);
            re2dj::platform::linux::ReleaseNativeImportThunks(&thunks);
            return 6;
        }
        re2dj::platform::linux::NativeGuestFault fault;
        if (!RunTlsCallbacks(info, image, &bootstrap, &fault, &error))
        {
            const bool sent = fault.status_code != 0 ? SendFaultEvent(fault) : SendError(error);
            ReleaseMappedImage(&image);
            re2dj::platform::linux::ReleaseNativeImportThunks(&thunks);
            return sent ? 0 : 6;
        }
        std::uint32_t result_code = 0;
        if (!bootstrap.RunEntry(image.entry_point, &result_code, &fault, &error))
        {
            const bool sent = fault.status_code != 0 ? SendFaultEvent(fault) : SendError(error);
            ReleaseMappedImage(&image);
            re2dj::platform::linux::ReleaseNativeImportThunks(&thunks);
            return sent ? 0 : 7;
        }
        protocol::ExecutionEvent event;
        event.kind = static_cast<std::uint32_t>(protocol::EventKind::kProcessExit);
        const std::uint64_t exit_event_id = next_event_id++;
        event.event_id_low = static_cast<std::uint32_t>(exit_event_id);
        event.event_id_high = static_cast<std::uint32_t>(exit_event_id >> 32);
        event.thread_id = static_cast<std::uint32_t>(syscall(SYS_gettid));
        event.instruction_pointer = image.entry_point;
        event.status_code = result_code;
        const bool sent = SendPacket(protocol::MessageType::kExecutionEvent, &event, sizeof(event));
        ReleaseMappedImage(&image);
        re2dj::platform::linux::ReleaseNativeImportThunks(&thunks);
        return sent && !gate_failed ? 0 : 7;
    }

    SendError("expected LoadImage request");
    return 1;
}
