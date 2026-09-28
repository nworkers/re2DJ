#ifndef RE2DJ_HLE_API_CALL_RECORD_H_
#define RE2DJ_HLE_API_CALL_RECORD_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/import_dispatcher.h"

namespace re2dj::hle
{

// One guest-memory access or last-error change a handler made, in order.
struct ApiCallEvent
{
    enum class Kind : std::uint8_t
    {
        kReadString,
        kReadBytes,
        kWriteBytes,
        kSetLastError,
        kCallGuest,
    };

    Kind kind = Kind::kReadBytes;
    std::uint32_t address = 0;
    // Bytes read or written, at most kApiCallEventBytes of them.
    std::vector<std::uint8_t> bytes;
    // The full length of the access.
    std::size_t length = 0;
    // The string read, the last-error value, or for a guest call the
    // function's address, its arguments, and its result.
    std::string text;
    std::vector<std::uint32_t> arguments;
    int data_argument = -1;
    std::uint32_t value = 0;
    bool succeeded = true;
};

inline constexpr std::size_t kApiCallEventBytes = 64;

// What one facade call did through its services.
struct ApiCallRecord
{
    std::vector<ApiCallEvent> events;
};

// Forwards every service to inner and notes the handler's guest reads,
// writes, and last-error changes in record.
class RecordingImportCallServices final : public ImportCallServices
{
public:
    RecordingImportCallServices(const ImportCallServices& inner, ApiCallRecord* record)
        : inner_(inner), record_(record)
    {
    }

    bool ReadGuestString(runtime::GuestAddress address,
                         std::string* value,
                         std::string* error) const override;
    runtime::GuestAddress FindGuestModule(std::string_view name) const override;
    runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                          std::string_view name) const override;
    runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                          std::uint16_t ordinal) const override;
    bool IsGuestModule(runtime::GuestAddress handle) const override;
    std::string GuestModuleName(runtime::GuestAddress handle) const override;
    bool ReadGuestBytes(runtime::GuestAddress address,
                        std::span<std::uint8_t> bytes,
                        std::string* error) const override;
    bool WriteGuestBytes(runtime::GuestAddress address,
                         std::span<const std::uint8_t> bytes,
                         std::string* error) const override;
    GuestDeviceSet* Devices() const override;
    GuestFiles* Files() const override;
    GuestProcess* Process() const override;
    bool ReadClock(GuestClockReading* reading) const override;
    void SetLastError(std::uint32_t value) const override;
    std::uint32_t LastError() const override;
    bool CallGuest(GuestCall* call, std::uint32_t* result, std::string* error) const override;
    HostPresentation* Presentation() const override;
    HostAudio* Audio() const override;
    runtime::GuestAddress ThreadEnvironmentBlock() const override;
    bool WaitMilliseconds(std::uint32_t milliseconds) const override;
    std::uint32_t CurrentThreadId() const override;
    bool StartGuestThread(std::uint32_t start,
                          std::uint32_t parameter,
                          std::uint32_t thread_id,
                          std::string* error) const override;

private:
    void NoteBytes(ApiCallEvent::Kind kind,
                   std::uint32_t address,
                   std::span<const std::uint8_t> bytes,
                   bool succeeded) const;

    const ImportCallServices& inner_;
    ApiCallRecord* record_ = nullptr;
};

// The record's detail lines: reads, writes, last-error changes, and guest
// calls. Bytes
// appear in memory order in groups of four, or as quoted text when they are a
// whole NUL-terminated ASCII string. With withhold_bytes, byte contents are
// replaced by "[withheld]" and only lengths remain, for buffers derived from
// secret material.
std::vector<std::string> FormatApiCallEvents(const ApiCallRecord& record, bool withhold_bytes);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_API_CALL_RECORD_H_
