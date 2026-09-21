#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#include "../native_helper_protocol.h"
#include "native_import_bridge.h"
#include "native_import_thunks.h"
#include "native_ipc_helper_main.h"
#include "native_pe_image.h"
#include "native_pe_session.h"
#include "native_process_bootstrap.h"
#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/pe_loader.h"

namespace
{

namespace protocol = re2dj::platform::native_protocol;

std::uint32_t pending_stack_start = 0;
std::uint32_t pending_stack_size = 0;
std::uint32_t active_image_start = 0;
std::uint32_t active_image_size = 0;
std::uint64_t next_event_id = 1;
bool gate_failed = false;

struct DynamicGuestMapping
{
    void* memory = nullptr;
    std::uint32_t address = 0;
    std::uint32_t size = 0;
    std::vector<std::uint32_t> page_access;
};

std::vector<DynamicGuestMapping> dynamic_mappings;

constexpr std::uint32_t kGuestPageSize = 4096;

bool HasMemoryAccess(std::uint32_t available, std::uint32_t required)
{
    return (available & required) == required;
}

bool IsValidMemoryAccess(std::uint32_t access)
{
    return (access & ~protocol::kGuestMemoryAccessMask) == 0;
}

int NativeProtection(std::uint32_t access)
{
    int protection = PROT_NONE;
    if ((access & protocol::kGuestMemoryAccessRead) != 0)
    {
        protection |= PROT_READ;
    }
    if ((access & protocol::kGuestMemoryAccessWrite) != 0)
    {
        protection |= PROT_WRITE;
    }
    if ((access & protocol::kGuestMemoryAccessExecute) != 0)
    {
        protection |= PROT_EXEC;
    }
    return protection;
}

bool RangeWithin(std::uint32_t address,
                 std::uint32_t size,
                 std::uint32_t range_start,
                 std::uint32_t range_size)
{
    const std::uint64_t request_start = address;
    const std::uint64_t request_end = request_start + size;
    const std::uint64_t range_end = static_cast<std::uint64_t>(range_start) + range_size;
    return size != 0 && request_end >= request_start && request_start >= range_start &&
           request_end <= range_end;
}

DynamicGuestMapping* FindDynamicMapping(std::uint32_t address, std::uint32_t size)
{
    for (DynamicGuestMapping& mapping : dynamic_mappings)
    {
        if (RangeWithin(address, size, mapping.address, mapping.size))
        {
            return &mapping;
        }
    }
    return nullptr;
}

bool MappingRangeHasAccess(const DynamicGuestMapping& mapping,
                           std::uint32_t address,
                           std::uint32_t size,
                           std::uint32_t required_access)
{
    if (!RangeWithin(address, size, mapping.address, mapping.size))
    {
        return false;
    }
    const std::uint32_t offset = address - mapping.address;
    const std::uint32_t first_page = offset / kGuestPageSize;
    const std::uint32_t last_page = (offset + size - 1) / kGuestPageSize;
    for (std::uint32_t page = first_page; page <= last_page; ++page)
    {
        if (!HasMemoryAccess(mapping.page_access[page], required_access))
        {
            return false;
        }
    }
    return true;
}

void ReleaseDynamicMappings()
{
    for (DynamicGuestMapping& mapping : dynamic_mappings)
    {
        munmap(mapping.memory, mapping.size);
    }
    dynamic_mappings.clear();
}

class DynamicMappingCleanup
{
public:
    ~DynamicMappingCleanup()
    {
        ReleaseDynamicMappings();
    }
};

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool ReadImageString(const re2dj::platform::linux::NativePeImage& image, std::uint32_t rva, std::string* value)
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

[[maybe_unused]] bool CollectImports(const re2dj::exe::PeImageInfo& info, const re2dj::platform::linux::NativePeImage& image,
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

[[maybe_unused]] bool RunTlsCallbacks(const re2dj::exe::PeImageInfo& info,
                     const re2dj::platform::linux::NativePeImage& image,
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

bool MemoryRangeAllowed(std::uint32_t address, std::uint32_t size, std::uint32_t access)
{
    if (size > protocol::kMaximumMemoryTransferSize)
    {
        return false;
    }
    if (RangeWithin(address, size, active_image_start, active_image_size) ||
        RangeWithin(address, size, pending_stack_start, pending_stack_size))
    {
        return true;
    }
    const DynamicGuestMapping* mapping = FindDynamicMapping(address, size);
    return mapping != nullptr && MappingRangeHasAccess(*mapping, address, size, access);
}

bool SendMemory(const protocol::ReadMemoryRequest& request)
{
    if (!MemoryRangeAllowed(request.address, request.size, protocol::kGuestMemoryAccessRead))
    {
        return SendError("guest memory request is outside the allowed regions");
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
        !ReadExact(STDIN_FILENO, &request, sizeof(request)))
    {
        return false;
    }
    const std::uint64_t expected_payload = sizeof(request) +
                                            static_cast<std::uint64_t>(request.size);
    if (expected_payload != payload_size ||
        request.size > protocol::kMaximumMemoryTransferSize)
    {
        std::vector<std::uint8_t> discarded(payload_size - sizeof(request));
        if (!discarded.empty() &&
            !ReadExact(STDIN_FILENO, discarded.data(),
                       static_cast<std::uint32_t>(discarded.size())))
        {
            return false;
        }
        return SendError("write memory packet has an invalid size");
    }
    std::vector<std::uint8_t> bytes(request.size);
    if (request.size != 0 &&
        !ReadExact(STDIN_FILENO, bytes.data(), request.size))
    {
        return false;
    }
    if (!MemoryRangeAllowed(request.address, request.size, protocol::kGuestMemoryAccessWrite))
    {
        return SendError("guest memory request is outside the allowed regions");
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

bool ReceiveAndAllocateMemory(std::uint32_t payload_size)
{
    protocol::AllocateMemoryRequest request;
    if (payload_size != sizeof(request) || !ReadExact(STDIN_FILENO, &request, sizeof(request)) ||
        request.size == 0 || !IsValidMemoryAccess(request.access))
    {
        return SendError("invalid guest memory allocation request");
    }
    const std::uint64_t rounded_size =
        (static_cast<std::uint64_t>(request.size) + kGuestPageSize - 1) &
        ~static_cast<std::uint64_t>(kGuestPageSize - 1);
    if (rounded_size == 0 || rounded_size > std::numeric_limits<std::uint32_t>::max())
    {
        return SendError("guest memory allocation is too large");
    }
    const std::uint32_t size = static_cast<std::uint32_t>(rounded_size);
    void* memory = mmap(nullptr,
                        size,
                        NativeProtection(request.access),
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    const std::uintptr_t raw_address = reinterpret_cast<std::uintptr_t>(memory);
    if (memory == MAP_FAILED || raw_address == 0 ||
        raw_address > std::numeric_limits<std::uint32_t>::max() ||
        size > std::numeric_limits<std::uint32_t>::max() - static_cast<std::uint32_t>(raw_address) + 1)
    {
        if (memory != MAP_FAILED)
        {
            munmap(memory, size);
        }
        return SendError("cannot map guest memory below 4 GiB");
    }
    try
    {
        DynamicGuestMapping mapping;
        mapping.memory = memory;
        mapping.address = static_cast<std::uint32_t>(raw_address);
        mapping.size = size;
        mapping.page_access.assign(size / kGuestPageSize, request.access);
        dynamic_mappings.push_back(std::move(mapping));
    }
    catch (const std::bad_alloc&)
    {
        munmap(memory, size);
        return SendError("cannot record guest memory allocation");
    }
    protocol::AllocateMemoryResult result;
    result.address = static_cast<std::uint32_t>(raw_address);
    result.size = size;
    return SendPacket(protocol::MessageType::kAllocateMemoryResult, &result, sizeof(result));
}

bool ReceiveAndProtectMemory(std::uint32_t payload_size)
{
    protocol::ProtectMemoryRequest request;
    if (payload_size != sizeof(request) || !ReadExact(STDIN_FILENO, &request, sizeof(request)) ||
        request.size == 0 || !IsValidMemoryAccess(request.access))
    {
        return SendError("invalid guest memory protection request");
    }
    DynamicGuestMapping* mapping = FindDynamicMapping(request.address, request.size);
    if (mapping == nullptr || request.address % kGuestPageSize != 0 ||
        request.size % kGuestPageSize != 0)
    {
        return SendError("cannot change guest memory protection");
    }
    const std::uint32_t offset = request.address - mapping->address;
    const std::uint32_t first_page = offset / kGuestPageSize;
    const std::uint32_t page_count = request.size / kGuestPageSize;
    const std::uint32_t previous_access = mapping->page_access[first_page];
    for (std::uint32_t page = first_page; page < first_page + page_count; ++page)
    {
        if (mapping->page_access[page] != previous_access)
        {
            return SendError("guest memory protection range has mixed prior access");
        }
    }
    auto* protection_start = static_cast<std::uint8_t*>(mapping->memory) + offset;
    if (mprotect(protection_start, request.size, NativeProtection(request.access)) != 0)
    {
        return SendError("cannot change guest memory protection");
    }
    protocol::ProtectMemoryResult result;
    result.previous_access = previous_access;
    for (std::uint32_t page = first_page; page < first_page + page_count; ++page)
    {
        mapping->page_access[page] = request.access;
    }
    return SendPacket(protocol::MessageType::kProtectMemoryResult, &result, sizeof(result));
}

bool ReceiveAndFreeMemory(std::uint32_t payload_size)
{
    protocol::FreeMemoryRequest request;
    if (payload_size != sizeof(request) || !ReadExact(STDIN_FILENO, &request, sizeof(request)))
    {
        return SendError("invalid guest memory free request");
    }
    for (auto iterator = dynamic_mappings.begin(); iterator != dynamic_mappings.end(); ++iterator)
    {
        if (iterator->address == request.address)
        {
            if (munmap(iterator->memory, iterator->size) != 0)
            {
                return SendError("cannot free guest memory");
            }
            protocol::FreeMemoryResult result;
            result.released_size = iterator->size;
            dynamic_mappings.erase(iterator);
            return SendPacket(protocol::MessageType::kFreeMemoryResult, &result, sizeof(result));
        }
    }
    return SendError("guest memory allocation was not found");
}

bool HandleIpcImportGate(const re2dj::platform::linux::NativeImportGateEvent& gate_event,
                         re2dj::platform::linux::NativeImportGateResult* result,
                         void*)
{
    if (result == nullptr)
    {
        gate_failed = true;
        return false;
    }
    pending_stack_start = gate_event.stack_pointer;
    pending_stack_size = 4096;

    const std::uint64_t event_id = next_event_id++;

    protocol::ExecutionEvent event;
    event.kind = static_cast<std::uint32_t>(protocol::EventKind::kImportGate);
    event.event_id_low = static_cast<std::uint32_t>(event_id);
    event.event_id_high = static_cast<std::uint32_t>(event_id >> 32);
    event.thread_id = static_cast<std::uint32_t>(syscall(SYS_gettid));
    event.instruction_pointer = gate_event.instruction_pointer;
    event.stack_pointer = pending_stack_start;
    event.gate_address = gate_event.gate_address;
    if (!SendPacket(protocol::MessageType::kExecutionEvent, &event, sizeof(event)))
    {
        gate_failed = true;
        return false;
    }

    for (;;)
    {
        protocol::MessageHeader header;
        if (!ReceiveHeader(&header))
        {
            gate_failed = true;
            return false;
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
                return false;
            }
            continue;
        }
        if (type == protocol::MessageType::kWriteMemory)
        {
            if (!ReceiveAndWriteMemory(header.payload_size))
            {
                gate_failed = true;
                return false;
            }
            continue;
        }
        if (type == protocol::MessageType::kAllocateMemory)
        {
            if (!ReceiveAndAllocateMemory(header.payload_size))
            {
                gate_failed = true;
                return false;
            }
            continue;
        }
        if (type == protocol::MessageType::kProtectMemory)
        {
            if (!ReceiveAndProtectMemory(header.payload_size))
            {
                gate_failed = true;
                return false;
            }
            continue;
        }
        if (type == protocol::MessageType::kFreeMemory)
        {
            if (!ReceiveAndFreeMemory(header.payload_size))
            {
                gate_failed = true;
                return false;
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
                return false;
            }
            pending_stack_start = 0;
            pending_stack_size = 0;
            result->eax = completion.eax;
            result->edx = completion.edx;
            result->stack_bytes_to_pop = completion.stack_bytes_to_pop;
            return true;
        }
        gate_failed = true;
        return false;
    }
}

class ImportGateHandlerCleanup
{
public:
    ~ImportGateHandlerCleanup()
    {
        re2dj::platform::linux::ClearNativeImportGateHandler();
    }
};

int RunNativeIpcHelperImpl()
{
    protocol::MessageHeader request;
    if (!ReceiveHeader(&request))
    {
        return 1;
    }

    if (static_cast<protocol::MessageType>(request.type) != protocol::MessageType::kHello ||
        request.payload_size != sizeof(protocol::HelloRequest))
    {
        SendError("expected protocol hello request");
        return 1;
    }
    protocol::HelloRequest hello;
    if (!ReadExact(STDIN_FILENO, &hello, sizeof(hello)))
    {
        return 1;
    }
    if ((hello.required_features & ~protocol::kSupportedFeatures) != 0)
    {
        SendError("helper does not support required protocol features");
        return 1;
    }
    protocol::HelloResult hello_result;
    hello_result.supported_features = protocol::kSupportedFeatures;
    if (!SendPacket(protocol::MessageType::kHelloResult, &hello_result, sizeof(hello_result)) ||
        !ReceiveHeader(&request))
    {
        return 1;
    }

    if (static_cast<protocol::MessageType>(request.type) == protocol::MessageType::kLoadImage)
    {
        DynamicMappingCleanup dynamic_mapping_cleanup;
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
        re2dj::platform::linux::NativePeSession session;
        if (!re2dj::platform::linux::ConfigureNativeImportGateHandler(&HandleIpcImportGate,
                                                                       nullptr))
        {
            SendError("cannot configure native import gate handler");
            return 4;
        }
        ImportGateHandlerCleanup import_gate_handler_cleanup;
        if (!re2dj::exe::ReadPeImageInfo(file.data(), file.size(), &info, &error) ||
            !session.Prepare(file, info, load.requested_base,
                             re2dj::platform::linux::NativeImportGateBridgeAddress(),
                             re2dj::platform::linux::NativeImportGateCleanupAddress(), &error))
        {
            SendError(error.empty() ? "cannot map native PE image" : error);
            return 4;
        }
        active_image_start = load.requested_base;
        active_image_size = session.image().size;
        protocol::LoadResult result;
        result.success = 1;
        result.load_base = load.requested_base;
        result.entry_point = session.image().entry_point;
        result.import_count = static_cast<std::uint32_t>(session.gates().gates().size());
        if (!SendPacket(protocol::MessageType::kLoadResult, &result, sizeof(result)))
        {
            session.Release();
            active_image_start = 0;
            active_image_size = 0;
            return 5;
        }
        for (const re2dj::runtime::ImportGate& gate : session.gates().gates())
        {
            if (!SendImportMetadata(gate))
            {
                session.Release();
                active_image_start = 0;
                active_image_size = 0;
                return 5;
            }
        }
        if (!ReceiveHeader(&request) ||
            static_cast<protocol::MessageType>(request.type) != protocol::MessageType::kStart ||
            request.payload_size != 0)
        {
            session.Release();
            active_image_start = 0;
            active_image_size = 0;
            return 6;
        }
        re2dj::platform::linux::NativeGuestFault fault;
        if (!session.RunTlsCallbacks(&fault, &error))
        {
            const bool sent = fault.status_code != 0 ? SendFaultEvent(fault) : SendError(error);
            session.Release();
            active_image_start = 0;
            active_image_size = 0;
            return sent ? 0 : 6;
        }
        std::uint32_t result_code = 0;
        if (!session.RunEntry(&result_code, &fault, &error))
        {
            const bool sent = fault.status_code != 0 ? SendFaultEvent(fault) : SendError(error);
            session.Release();
            active_image_start = 0;
            active_image_size = 0;
            return sent ? 0 : 7;
        }
        protocol::ExecutionEvent event;
        event.kind = static_cast<std::uint32_t>(protocol::EventKind::kProcessExit);
        const std::uint64_t exit_event_id = next_event_id++;
        event.event_id_low = static_cast<std::uint32_t>(exit_event_id);
        event.event_id_high = static_cast<std::uint32_t>(exit_event_id >> 32);
        event.thread_id = static_cast<std::uint32_t>(syscall(SYS_gettid));
        event.instruction_pointer = session.image().entry_point;
        event.status_code = result_code;
        const bool sent = SendPacket(protocol::MessageType::kExecutionEvent, &event, sizeof(event));
        session.Release();
        active_image_start = 0;
        active_image_size = 0;
        return sent && !gate_failed ? 0 : 7;
    }

    SendError("expected LoadImage request");
    return 1;
}

}  // namespace

int re2dj::platform::linux::RunNativeIpcHelper()
{
    return RunNativeIpcHelperImpl();
}
