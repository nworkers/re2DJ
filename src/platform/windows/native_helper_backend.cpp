#define NOMINMAX
#include <windows.h>

#include "re2dj/platform/windows/native_helper_backend.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "../native_helper_protocol.h"

namespace re2dj::platform::windows
{
namespace
{

namespace protocol = re2dj::platform::native_protocol;

constexpr DWORD kExitWaitMilliseconds = 5000;
constexpr DWORD kStopWaitMilliseconds = 250;
constexpr std::uint32_t kMemoryTransferLimit = 4096;

void SetError(std::string* error, std::string message)
{
    if (error != nullptr)
    {
        *error = std::move(message);
    }
}

std::string WindowsError(std::string message)
{
    message += " (Windows error ";
    message += std::to_string(GetLastError());
    message += ")";
    return message;
}

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

std::uint64_t JoinEventId(std::uint32_t low, std::uint32_t high)
{
    return static_cast<std::uint64_t>(low) |
           (static_cast<std::uint64_t>(high) << 32);
}

}  // namespace

class NativeHelperBackend::Impl
{
public:
    explicit Impl(std::filesystem::path helper_path)
        : helper_path_(std::move(helper_path))
    {
    }

    ~Impl()
    {
        StopChild();
    }

    bool PrepareImage(std::span<const std::uint8_t> file_bytes,
                      const exe::PeImageInfo& info,
                      runtime::GuestAddress requested_base,
                      runtime::LoadedPeImage* loaded,
                      std::string* error)
    {
        if (state_ != State::kIdle)
        {
            SetError(error, "native helper backend is not idle");
            return false;
        }
        if (loaded == nullptr || file_bytes.empty() ||
            file_bytes.size() >
                protocol::kMaximumPayloadSize - sizeof(protocol::LoadImageRequest) ||
            file_bytes.size() > (std::numeric_limits<std::uint32_t>::max)())
        {
            SetError(error, "native helper image arguments are invalid");
            return false;
        }
        if (!exe::IsGuestExecutable(info) ||
            info.image_base > (std::numeric_limits<std::uint32_t>::max)())
        {
            SetError(error, "native helper requires an x86 PE32 executable");
            return false;
        }
        const std::uint32_t preferred_base = static_cast<std::uint32_t>(info.image_base);
        const std::uint32_t load_base =
            requested_base.value() == 0 ? preferred_base : requested_base.value();
        if (!Launch(error))
        {
            state_ = State::kFailed;
            return false;
        }
        protocol::LoadImageRequest request;
        request.requested_base = load_base;
        request.file_size = static_cast<std::uint32_t>(file_bytes.size());
        std::vector<std::uint8_t> payload(sizeof(request) + file_bytes.size());
        std::memcpy(payload.data(), &request, sizeof(request));
        std::memcpy(payload.data() + sizeof(request),
                    file_bytes.data(),
                    file_bytes.size());
        if (!SendPacket(protocol::MessageType::kLoadImage,
                        payload.data(),
                        static_cast<std::uint32_t>(payload.size()),
                        error))
        {
            FailAndStop();
            return false;
        }

        protocol::LoadResult result;
        if (!ReceivePacket(protocol::MessageType::kLoadResult, &result, error))
        {
            FailAndStop();
            return false;
        }
        if (result.success != 1 || result.load_base != load_base ||
            result.entry_point != load_base + info.entry_point_rva ||
            result.import_count > protocol::kMaximumImportCount)
        {
            SetError(error, "native helper returned inconsistent image metadata");
            FailAndStop();
            return false;
        }

        loaded->load_base = runtime::GuestAddress(result.load_base);
        loaded->entry_point = runtime::GuestAddress(result.entry_point);
        loaded->imports.clear();
        loaded->imports.reserve(result.import_count);
        for (std::uint32_t index = 0; index < result.import_count; ++index)
        {
            runtime::ImportGate import;
            if (!ReceiveImportMetadata(&import, error))
            {
                FailAndStop();
                return false;
            }
            for (const runtime::ImportGate& existing : loaded->imports)
            {
                if (existing.address == import.address)
                {
                    SetError(error, "native helper returned a duplicate import gate");
                    FailAndStop();
                    return false;
                }
            }
            loaded->imports.push_back(std::move(import));
        }
        const exe::PeDataDirectory* tls = info.Directory(exe::PeDirectoryIndex::kTls);
        loaded->tls_directory_rva = tls == nullptr ? 0 : tls->virtual_address;
        loaded->tls_directory_size = tls == nullptr ? 0 : tls->size;
        state_ = State::kPrepared;
        return true;
    }

    bool Start(std::string* error)
    {
        if (state_ != State::kPrepared)
        {
            SetError(error, "native helper image is not prepared");
            return false;
        }
        if (!SendPacket(protocol::MessageType::kStart, nullptr, 0, error))
        {
            FailAndStop();
            return false;
        }
        state_ = State::kRunning;
        return true;
    }

    bool WaitForEvent(runtime::ExecutionEvent* event, std::string* error)
    {
        if (state_ != State::kRunning || event == nullptr)
        {
            SetError(error, "native helper is not ready to report an event");
            return false;
        }
        protocol::ExecutionEvent packet;
        if (!ReceivePacket(protocol::MessageType::kExecutionEvent, &packet, error))
        {
            FailAndStop();
            return false;
        }

        event->event_id = JoinEventId(packet.event_id_low, packet.event_id_high);
        event->thread_id = packet.thread_id;
        event->instruction_pointer = runtime::GuestAddress(packet.instruction_pointer);
        event->stack_pointer = runtime::GuestAddress(packet.stack_pointer);
        event->gate_address = runtime::GuestAddress(packet.gate_address);
        event->status_code = packet.status_code;

        switch (static_cast<protocol::EventKind>(packet.kind))
        {
        case protocol::EventKind::kImportGate:
            event->kind = runtime::ExecutionEventKind::kImportGate;
            pending_event_id_ = event->event_id;
            state_ = State::kImportPending;
            return true;
        case protocol::EventKind::kProcessExit:
            event->kind = runtime::ExecutionEventKind::kProcessExit;
            if (!WaitForCleanExit(error))
            {
                state_ = State::kFailed;
                StopChild();
                return false;
            }
            state_ = State::kExited;
            CloseHandles();
            return true;
        case protocol::EventKind::kFault:
            event->kind = runtime::ExecutionEventKind::kFault;
            state_ = State::kFailed;
            return true;
        default:
            SetError(error, "native helper returned an unknown event kind");
            FailAndStop();
            return false;
        }
    }

    bool ReadMemory(runtime::GuestAddress address,
                    std::span<std::uint8_t> bytes,
                    std::string* error)
    {
        if (!ValidateMemoryTransfer(bytes.size(), error))
        {
            return false;
        }
        protocol::ReadMemoryRequest request;
        request.address = address.value();
        request.size = static_cast<std::uint32_t>(bytes.size());
        if (!SendPacket(protocol::MessageType::kReadMemory,
                        &request,
                        sizeof(request),
                        error))
        {
            FailAndStop();
            return false;
        }

        protocol::MessageHeader header;
        if (!ReceiveHeader(&header, error) ||
            !ExpectPayload(header,
                           protocol::MessageType::kMemoryData,
                           static_cast<std::uint32_t>(bytes.size()),
                           error) ||
            (!bytes.empty() &&
             !ReadExact(output_, bytes.data(), static_cast<std::uint32_t>(bytes.size()))))
        {
            if (error != nullptr && error->empty())
            {
                *error = "cannot read native helper memory response";
            }
            FailAndStop();
            return false;
        }
        return true;
    }

    bool WriteMemory(runtime::GuestAddress address,
                     std::span<const std::uint8_t> bytes,
                     std::string* error)
    {
        if (!ValidateMemoryTransfer(bytes.size(), error))
        {
            return false;
        }
        protocol::ReadMemoryRequest request;
        request.address = address.value();
        request.size = static_cast<std::uint32_t>(bytes.size());
        std::vector<std::uint8_t> payload(sizeof(request) + bytes.size());
        std::memcpy(payload.data(), &request, sizeof(request));
        if (!bytes.empty())
        {
            std::memcpy(payload.data() + sizeof(request), bytes.data(), bytes.size());
        }
        if (!SendPacket(protocol::MessageType::kWriteMemory,
                        payload.data(),
                        static_cast<std::uint32_t>(payload.size()),
                        error))
        {
            FailAndStop();
            return false;
        }

        protocol::WriteMemoryResult result;
        if (!ReceivePacket(protocol::MessageType::kWriteResult, &result, error) ||
            result.success != 1 || result.size != bytes.size())
        {
            if (error != nullptr && error->empty())
            {
                *error = "native helper could not write guest memory";
            }
            FailAndStop();
            return false;
        }
        return true;
    }

    bool CompleteImport(const runtime::ImportCompletion& completion,
                        std::string* error)
    {
        if (state_ != State::kImportPending)
        {
            SetError(error, "native helper has no pending import event");
            return false;
        }
        if (completion.event_id != pending_event_id_)
        {
            SetError(error, "native helper import completion has the wrong event ID");
            return false;
        }

        protocol::CompleteImport packet;
        packet.event_id_low = static_cast<std::uint32_t>(completion.event_id);
        packet.event_id_high = static_cast<std::uint32_t>(completion.event_id >> 32);
        packet.eax = completion.eax;
        packet.edx = completion.edx;
        packet.stack_bytes_to_pop = completion.stack_bytes_to_pop;
        packet.action = completion.action == runtime::ImportCompletionAction::kContinue ? 0U : 1U;
        if (!SendPacket(protocol::MessageType::kCompleteImport,
                        &packet,
                        sizeof(packet),
                        error))
        {
            FailAndStop();
            return false;
        }
        pending_event_id_ = 0;
        if (completion.action == runtime::ImportCompletionAction::kContinue)
        {
            state_ = State::kRunning;
        }
        else
        {
            state_ = State::kStopped;
            StopChild();
        }
        return true;
    }

    void RequestStop()
    {
        StopChild();
        if (state_ != State::kExited && state_ != State::kFailed)
        {
            state_ = State::kStopped;
        }
    }

private:
    enum class State
    {
        kIdle,
        kPrepared,
        kRunning,
        kImportPending,
        kExited,
        kStopped,
        kFailed,
    };

    bool Launch(std::string* error)
    {
        SECURITY_ATTRIBUTES security = {};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        HANDLE child_input = INVALID_HANDLE_VALUE;
        HANDLE child_output = INVALID_HANDLE_VALUE;
        if (!CreatePipe(&child_input, &input_, &security, 0))
        {
            SetError(error, WindowsError("cannot create native helper input pipe"));
            return false;
        }
        if (!CreatePipe(&output_, &child_output, &security, 0))
        {
            CloseHandle(child_input);
            CloseHandles();
            SetError(error, WindowsError("cannot create native helper output pipe"));
            return false;
        }
        if (!SetHandleInformation(input_, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(output_, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(child_input);
            CloseHandle(child_output);
            CloseHandles();
            SetError(error, WindowsError("cannot configure native helper pipes"));
            return false;
        }

        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = child_input;
        startup.hStdOutput = child_output;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        std::wstring command = L"\"" + helper_path_.wstring() + L"\"";
        std::vector<wchar_t> mutable_command(command.begin(), command.end());
        mutable_command.push_back(L'\0');
        const BOOL created = CreateProcessW(helper_path_.c_str(),
                                            mutable_command.data(),
                                            nullptr,
                                            nullptr,
                                            TRUE,
                                            CREATE_NO_WINDOW,
                                            nullptr,
                                            nullptr,
                                            &startup,
                                            &process_);
        const DWORD create_error = GetLastError();
        CloseHandle(child_input);
        CloseHandle(child_output);
        if (created == FALSE)
        {
            CloseHandles();
            SetLastError(create_error);
            SetError(error, WindowsError("cannot launch native helper"));
            return false;
        }
        return true;
    }

    bool SendPacket(protocol::MessageType type,
                    const void* payload,
                    std::uint32_t payload_size,
                    std::string* error)
    {
        protocol::MessageHeader header;
        header.type = static_cast<std::uint32_t>(type);
        header.payload_size = payload_size;
        if (!WriteExact(input_, &header, sizeof(header)) ||
            (payload_size != 0 && !WriteExact(input_, payload, payload_size)))
        {
            SetError(error, WindowsError("cannot write native helper packet"));
            return false;
        }
        return true;
    }

    bool ReceiveHeader(protocol::MessageHeader* header, std::string* error)
    {
        if (!ReadExact(output_, header, sizeof(*header)))
        {
            SetError(error, WindowsError("cannot read native helper packet header"));
            return false;
        }
        if (header->magic != protocol::kMagic || header->version != protocol::kVersion ||
            header->payload_size > protocol::kMaximumPayloadSize)
        {
            SetError(error, "native helper returned an invalid packet header");
            return false;
        }
        return true;
    }

    bool ExpectPayload(const protocol::MessageHeader& header,
                       protocol::MessageType expected,
                       std::uint32_t expected_size,
                       std::string* error)
    {
        const auto type = static_cast<protocol::MessageType>(header.type);
        if (type == protocol::MessageType::kError)
        {
            std::vector<char> message(header.payload_size + 1, 0);
            if (header.payload_size != 0 &&
                !ReadExact(output_, message.data(), header.payload_size))
            {
                SetError(error, "cannot read native helper error packet");
                return false;
            }
            SetError(error, std::string(message.data(), header.payload_size));
            return false;
        }
        if (type != expected || header.payload_size != expected_size)
        {
            SetError(error, "native helper returned an unexpected packet");
            return false;
        }
        return true;
    }

    template <typename Payload>
    bool ReceivePacket(protocol::MessageType expected,
                       Payload* payload,
                       std::string* error)
    {
        protocol::MessageHeader header;
        if (!ReceiveHeader(&header, error) ||
            !ExpectPayload(header, expected, sizeof(Payload), error))
        {
            return false;
        }
        if (!ReadExact(output_, payload, sizeof(Payload)))
        {
            SetError(error, WindowsError("cannot read native helper packet payload"));
            return false;
        }
        return true;
    }

    bool ReceiveImportMetadata(runtime::ImportGate* import, std::string* error)
    {
        protocol::MessageHeader packet_header;
        if (!ReceiveHeader(&packet_header, error))
        {
            return false;
        }
        if (static_cast<protocol::MessageType>(packet_header.type) ==
            protocol::MessageType::kError)
        {
            return ExpectPayload(packet_header,
                                 protocol::MessageType::kImportMetadata,
                                 0,
                                 error);
        }
        if (static_cast<protocol::MessageType>(packet_header.type) !=
                protocol::MessageType::kImportMetadata ||
            packet_header.payload_size < sizeof(protocol::ImportMetadata) ||
            packet_header.payload_size > sizeof(protocol::ImportMetadata) +
                                             2 * protocol::kMaximumImportStringSize)
        {
            SetError(error, "native helper returned invalid import metadata size");
            return false;
        }

        protocol::ImportMetadata metadata;
        if (!ReadExact(output_, &metadata, sizeof(metadata)) ||
            metadata.module_size == 0 ||
            metadata.module_size > protocol::kMaximumImportStringSize ||
            metadata.name_size > protocol::kMaximumImportStringSize ||
            packet_header.payload_size !=
                sizeof(metadata) + metadata.module_size + metadata.name_size ||
            metadata.by_ordinal > 1 || metadata.ordinal > 0xFFFF ||
            (metadata.by_ordinal == 0 && metadata.name_size == 0) ||
            (metadata.by_ordinal == 0 && metadata.ordinal != 0) ||
            (metadata.by_ordinal != 0 && metadata.name_size != 0) ||
            metadata.gate_address < runtime::kDefaultImportGateBase ||
            (metadata.gate_address - runtime::kDefaultImportGateBase) %
                    runtime::kDefaultImportGateStride !=
                0)
        {
            SetError(error, "native helper returned malformed import metadata");
            return false;
        }

        import->module.resize(metadata.module_size);
        import->name.resize(metadata.name_size);
        if (!ReadExact(output_, import->module.data(), metadata.module_size) ||
            (metadata.name_size != 0 &&
             !ReadExact(output_, import->name.data(), metadata.name_size)))
        {
            SetError(error, "cannot read native helper import metadata");
            return false;
        }
        import->address = runtime::GuestAddress(metadata.gate_address);
        import->by_ordinal = metadata.by_ordinal != 0;
        import->ordinal = static_cast<std::uint16_t>(metadata.ordinal);
        return true;
    }

    bool ValidateMemoryTransfer(std::size_t size, std::string* error) const
    {
        if (state_ != State::kImportPending)
        {
            SetError(error, "guest memory is available only while an import is pending");
            return false;
        }
        if (size > kMemoryTransferLimit)
        {
            SetError(error, "guest memory transfer exceeds the prototype limit");
            return false;
        }
        return true;
    }

    bool WaitForCleanExit(std::string* error)
    {
        if (process_.hProcess == nullptr ||
            WaitForSingleObject(process_.hProcess, kExitWaitMilliseconds) != WAIT_OBJECT_0)
        {
            SetError(error, "native helper did not exit after the process event");
            return false;
        }
        DWORD exit_code = (std::numeric_limits<DWORD>::max)();
        if (!GetExitCodeProcess(process_.hProcess, &exit_code) || exit_code != 0)
        {
            SetError(error, "native helper exited with code " + std::to_string(exit_code));
            return false;
        }
        return true;
    }

    void FailAndStop()
    {
        state_ = State::kFailed;
        StopChild();
    }

    void StopChild()
    {
        if (input_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(input_);
            input_ = INVALID_HANDLE_VALUE;
        }
        if (process_.hProcess != nullptr &&
            WaitForSingleObject(process_.hProcess, kStopWaitMilliseconds) == WAIT_TIMEOUT)
        {
            TerminateProcess(process_.hProcess, 99);
            WaitForSingleObject(process_.hProcess, kExitWaitMilliseconds);
        }
        CloseHandles();
    }

    void CloseHandles()
    {
        if (input_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(input_);
            input_ = INVALID_HANDLE_VALUE;
        }
        if (output_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(output_);
            output_ = INVALID_HANDLE_VALUE;
        }
        if (process_.hThread != nullptr)
        {
            CloseHandle(process_.hThread);
            process_.hThread = nullptr;
        }
        if (process_.hProcess != nullptr)
        {
            CloseHandle(process_.hProcess);
            process_.hProcess = nullptr;
        }
    }

    std::filesystem::path helper_path_;
    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION process_ = {};
    State state_ = State::kIdle;
    std::uint64_t pending_event_id_ = 0;
};

NativeHelperBackend::NativeHelperBackend(std::filesystem::path helper_path)
    : impl_(std::make_unique<Impl>(std::move(helper_path)))
{
}

NativeHelperBackend::~NativeHelperBackend() = default;

bool NativeHelperBackend::PrepareImage(std::span<const std::uint8_t> file_bytes,
                                       const exe::PeImageInfo& info,
                                       runtime::GuestAddress requested_base,
                                       runtime::LoadedPeImage* loaded,
                                       std::string* error)
{
    return impl_->PrepareImage(file_bytes, info, requested_base, loaded, error);
}

bool NativeHelperBackend::Start(std::string* error)
{
    return impl_->Start(error);
}

bool NativeHelperBackend::WaitForEvent(runtime::ExecutionEvent* event,
                                       std::string* error)
{
    return impl_->WaitForEvent(event, error);
}

bool NativeHelperBackend::ReadMemory(runtime::GuestAddress address,
                                     std::span<std::uint8_t> bytes,
                                     std::string* error)
{
    return impl_->ReadMemory(address, bytes, error);
}

bool NativeHelperBackend::WriteMemory(runtime::GuestAddress address,
                                      std::span<const std::uint8_t> bytes,
                                      std::string* error)
{
    return impl_->WriteMemory(address, bytes, error);
}

bool NativeHelperBackend::CompleteImport(const runtime::ImportCompletion& completion,
                                         std::string* error)
{
    return impl_->CompleteImport(completion, error);
}

void NativeHelperBackend::RequestStop()
{
    impl_->RequestStop();
}

}  // namespace re2dj::platform::windows
