#ifndef RE2DJ_PLATFORM_NATIVE_HELPER_PROTOCOL_H_
#define RE2DJ_PLATFORM_NATIVE_HELPER_PROTOCOL_H_

#include <cstdint>

namespace re2dj::platform::native_protocol
{

inline constexpr std::uint32_t kMagic = 0x504A4432;
inline constexpr std::uint32_t kVersion = 3;
inline constexpr std::uint32_t kMaximumPayloadSize = 16 * 1024 * 1024;
inline constexpr std::uint32_t kMaximumImportStringSize = 4096;
inline constexpr std::uint32_t kMaximumImportCount = 65536;

enum class MessageType : std::uint32_t
{
    kLoadImage = 1,
    kLoadResult = 2,
    kStart = 3,
    kExecutionEvent = 4,
    kReadMemory = 5,
    kMemoryData = 6,
    kCompleteImport = 7,
    kWriteMemory = 8,
    kWriteResult = 9,
    kError = 10,
    kImportMetadata = 11,
};

enum class EventKind : std::uint32_t
{
    kImportGate = 1,
    kProcessExit = 2,
    kFault = 3,
};

struct MessageHeader
{
    std::uint32_t magic = kMagic;
    std::uint32_t version = kVersion;
    std::uint32_t type = 0;
    std::uint32_t payload_size = 0;
};

struct LoadImageRequest
{
    std::uint32_t requested_base = 0;
    std::uint32_t file_size = 0;
};

struct LoadResult
{
    std::uint32_t success = 0;
    std::uint32_t load_base = 0;
    std::uint32_t entry_point = 0;
    std::uint32_t import_count = 0;
};

struct ImportMetadata
{
    std::uint32_t gate_address = 0;
    std::uint32_t by_ordinal = 0;
    std::uint32_t ordinal = 0;
    std::uint32_t module_size = 0;
    std::uint32_t name_size = 0;
};

struct ExecutionEvent
{
    std::uint32_t kind = 0;
    std::uint32_t event_id_low = 0;
    std::uint32_t event_id_high = 0;
    std::uint32_t thread_id = 0;
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t gate_address = 0;
    std::uint32_t status_code = 0;
};

struct ReadMemoryRequest
{
    std::uint32_t address = 0;
    std::uint32_t size = 0;
};

struct CompleteImport
{
    std::uint32_t event_id_low = 0;
    std::uint32_t event_id_high = 0;
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    std::uint32_t stack_bytes_to_pop = 0;
    std::uint32_t action = 0;
};

struct WriteMemoryResult
{
    std::uint32_t success = 0;
    std::uint32_t size = 0;
};

static_assert(sizeof(MessageHeader) == 16);
static_assert(sizeof(LoadImageRequest) == 8);
static_assert(sizeof(LoadResult) == 16);
static_assert(sizeof(ImportMetadata) == 20);
static_assert(sizeof(ExecutionEvent) == 32);
static_assert(sizeof(ReadMemoryRequest) == 8);
static_assert(sizeof(CompleteImport) == 24);
static_assert(sizeof(WriteMemoryResult) == 8);

}  // namespace re2dj::platform::native_protocol

#endif  // RE2DJ_PLATFORM_NATIVE_HELPER_PROTOCOL_H_

