#include "re2dj/hle/api_call_record.h"

#include <algorithm>
#include <cstdio>

namespace re2dj::hle
{
namespace
{

// A guest string in quotes, with '"' and '\' escaped by a backslash and bytes
// outside printable ASCII as \xNN so CP949 text stays recoverable.
std::string QuoteText(std::string_view text)
{
    std::string quoted = "\"";
    char escape[5] = {};
    for (const char character : text)
    {
        const auto byte = static_cast<unsigned char>(character);
        if (byte == '"' || byte == '\\')
        {
            quoted.push_back('\\');
            quoted.push_back(character);
        }
        else if (byte < 0x20 || byte > 0x7E)
        {
            std::snprintf(escape, sizeof(escape), "\\x%02x", byte);
            quoted += escape;
        }
        else
        {
            quoted.push_back(character);
        }
    }
    quoted.push_back('"');
    return quoted;
}

// Bytes in memory order, grouped by four, with the omitted tail counted.
std::string HexGroups(const std::vector<std::uint8_t>& bytes, std::size_t length)
{
    std::string text;
    char pair[3] = {};
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        if (index != 0 && index % 4 == 0)
        {
            text.push_back(' ');
        }
        std::snprintf(pair, sizeof(pair), "%02x", static_cast<unsigned>(bytes[index]));
        text += pair;
    }
    if (length > bytes.size())
    {
        text += " ... (+" + std::to_string(length - bytes.size()) + ")";
    }
    return text;
}

// True for a fully kept, NUL-terminated run of printable ASCII (tab, CR, and
// LF included), which reads better as text than as hex.
bool IsAsciiString(const std::vector<std::uint8_t>& bytes, std::size_t length)
{
    if (length < 2 || bytes.size() != length || bytes.back() != 0)
    {
        return false;
    }
    for (std::size_t index = 0; index + 1 < bytes.size(); ++index)
    {
        const std::uint8_t byte = bytes[index];
        if ((byte < 0x20 || byte > 0x7E) && byte != '\t' && byte != '\r' && byte != '\n')
        {
            return false;
        }
    }
    return true;
}

}  // namespace

void RecordingImportCallServices::NoteBytes(ApiCallEvent::Kind kind,
                                            std::uint32_t address,
                                            std::span<const std::uint8_t> bytes,
                                            bool succeeded) const
{
    if (record_ == nullptr)
    {
        return;
    }
    ApiCallEvent event;
    event.kind = kind;
    event.address = address;
    event.length = bytes.size();
    event.succeeded = succeeded;
    if (succeeded)
    {
        const std::size_t kept = std::min(bytes.size(), kApiCallEventBytes);
        event.bytes.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(kept));
    }
    record_->events.push_back(std::move(event));
}

bool RecordingImportCallServices::ReadGuestString(runtime::GuestAddress address,
                                                  std::string* value,
                                                  std::string* error) const
{
    const bool read = inner_.ReadGuestString(address, value, error);
    if (record_ != nullptr)
    {
        ApiCallEvent event;
        event.kind = ApiCallEvent::Kind::kReadString;
        event.address = address.value();
        event.succeeded = read;
        if (read && value != nullptr)
        {
            event.text = *value;
            event.length = value->size();
        }
        record_->events.push_back(std::move(event));
    }
    return read;
}

runtime::GuestAddress RecordingImportCallServices::FindGuestModule(std::string_view name) const
{
    return inner_.FindGuestModule(name);
}

runtime::GuestAddress RecordingImportCallServices::FindGuestExport(runtime::GuestAddress module,
                                                                   std::string_view name) const
{
    return inner_.FindGuestExport(module, name);
}

runtime::GuestAddress RecordingImportCallServices::FindGuestExport(runtime::GuestAddress module,
                                                                   std::uint16_t ordinal) const
{
    return inner_.FindGuestExport(module, ordinal);
}

bool RecordingImportCallServices::IsGuestModule(runtime::GuestAddress handle) const
{
    return inner_.IsGuestModule(handle);
}

std::string RecordingImportCallServices::GuestModuleName(runtime::GuestAddress handle) const
{
    return inner_.GuestModuleName(handle);
}

bool RecordingImportCallServices::ReadGuestBytes(runtime::GuestAddress address,
                                                 std::span<std::uint8_t> bytes,
                                                 std::string* error) const
{
    const bool read = inner_.ReadGuestBytes(address, bytes, error);
    NoteBytes(ApiCallEvent::Kind::kReadBytes, address.value(), bytes, read);
    return read;
}

bool RecordingImportCallServices::WriteGuestBytes(runtime::GuestAddress address,
                                                  std::span<const std::uint8_t> bytes,
                                                  std::string* error) const
{
    const bool written = inner_.WriteGuestBytes(address, bytes, error);
    NoteBytes(ApiCallEvent::Kind::kWriteBytes, address.value(), bytes, written);
    return written;
}

GuestDeviceSet* RecordingImportCallServices::Devices() const
{
    return inner_.Devices();
}

GuestFiles* RecordingImportCallServices::Files() const
{
    return inner_.Files();
}

GuestProcess* RecordingImportCallServices::Process() const
{
    return inner_.Process();
}

bool RecordingImportCallServices::ReadClock(GuestClockReading* reading) const
{
    return inner_.ReadClock(reading);
}

void RecordingImportCallServices::SetLastError(std::uint32_t value) const
{
    inner_.SetLastError(value);
    if (record_ != nullptr)
    {
        ApiCallEvent event;
        event.kind = ApiCallEvent::Kind::kSetLastError;
        event.value = value;
        record_->events.push_back(std::move(event));
    }
}

std::uint32_t RecordingImportCallServices::LastError() const
{
    return inner_.LastError();
}

HostPresentation* RecordingImportCallServices::Presentation() const
{
    return inner_.Presentation();
}

HostAudio* RecordingImportCallServices::Audio() const
{
    return inner_.Audio();
}

HostProcessLauncher* RecordingImportCallServices::ProcessLauncher() const
{
    return inner_.ProcessLauncher();
}

runtime::GuestAddress RecordingImportCallServices::ThreadEnvironmentBlock() const
{
    return inner_.ThreadEnvironmentBlock();
}

bool RecordingImportCallServices::WaitMilliseconds(std::uint32_t milliseconds) const
{
    return inner_.WaitMilliseconds(milliseconds);
}

std::uint32_t RecordingImportCallServices::CurrentThreadId() const
{
    return inner_.CurrentThreadId();
}

bool RecordingImportCallServices::StartGuestThread(std::uint32_t start,
                                                   std::uint32_t parameter,
                                                   std::uint32_t thread_id,
                                                   std::string* error) const
{
    return inner_.StartGuestThread(start, parameter, thread_id, error);
}

bool RecordingImportCallServices::CallGuest(GuestCall* call,
                                            std::uint32_t* result,
                                            std::string* error) const
{
    // Noted before the call so the guest's nested imports, logged as they
    // finish, follow it in the record's order of intent.
    std::size_t index = 0;
    if (record_ != nullptr && call != nullptr)
    {
        ApiCallEvent event;
        event.kind = ApiCallEvent::Kind::kCallGuest;
        event.address = call->function;
        event.arguments = call->arguments;
        event.data_argument = call->data.empty() ? -1 : call->data_argument;
        event.length = call->data.size();
        index = record_->events.size();
        record_->events.push_back(std::move(event));
    }
    const bool called = inner_.CallGuest(call, result, error);
    if (record_ != nullptr && call != nullptr)
    {
        ApiCallEvent& event = record_->events[index];
        event.succeeded = called;
        event.value = called && result != nullptr ? *result : 0;
    }
    return called;
}

std::vector<std::string> FormatApiCallEvents(const ApiCallRecord& record, bool withhold_bytes)
{
    std::vector<std::string> lines;
    char head[48] = {};
    for (const ApiCallEvent& event : record.events)
    {
        switch (event.kind)
        {
        case ApiCallEvent::Kind::kReadString:
            std::snprintf(head, sizeof(head), "read  %08x ", event.address);
            lines.push_back(head + (event.succeeded ? QuoteText(event.text) : std::string("<unreadable>")));
            break;
        case ApiCallEvent::Kind::kReadBytes:
        case ApiCallEvent::Kind::kWriteBytes:
        {
            std::snprintf(head,
                          sizeof(head),
                          "%s %08x [%zu] ",
                          event.kind == ApiCallEvent::Kind::kReadBytes ? "read " : "write",
                          event.address,
                          event.length);
            std::string body;
            if (!event.succeeded)
            {
                body = "<failed>";
            }
            else if (withhold_bytes && event.length != 0)
            {
                body = "[withheld]";
            }
            else if (IsAsciiString(event.bytes, event.length))
            {
                body = QuoteText(std::string_view(reinterpret_cast<const char*>(event.bytes.data()),
                                                  event.bytes.size() - 1)) +
                       " + NUL";
            }
            else
            {
                body = HexGroups(event.bytes, event.length);
            }
            lines.push_back(head + body);
            break;
        }
        case ApiCallEvent::Kind::kSetLastError:
            lines.push_back("last_error <- " + std::to_string(event.value));
            break;
        case ApiCallEvent::Kind::kCallGuest:
        {
            std::snprintf(head, sizeof(head), "call  %08x(", event.address);
            std::string line = head;
            char word[32] = {};
            for (std::size_t argument = 0; argument < event.arguments.size(); ++argument)
            {
                if (static_cast<int>(argument) == event.data_argument)
                {
                    std::snprintf(word, sizeof(word), "%s&data[%zu]", argument == 0 ? "" : ", ", event.length);
                }
                else
                {
                    std::snprintf(word, sizeof(word), argument == 0 ? "%08x" : ", %08x", event.arguments[argument]);
                }
                line += word;
            }
            if (event.succeeded)
            {
                std::snprintf(word, sizeof(word), ") -> %08x", event.value);
                line += word;
            }
            else
            {
                line += ") <failed>";
            }
            lines.push_back(std::move(line));
            break;
        }
        }
    }
    return lines;
}

}  // namespace re2dj::hle
