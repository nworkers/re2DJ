#include "re2dj/platform/linux/native_helper_backend.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "../native_helper_protocol.h"

namespace re2dj::platform::linux
{
namespace
{

namespace protocol = native_protocol;

constexpr std::uint32_t kTransferLimit = protocol::kMaximumMemoryTransferSize;

void SetError(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
    }
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

std::uint64_t EventId(std::uint32_t low, std::uint32_t high)
{
    return static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32);
}

std::uint32_t AccessValue(runtime::GuestMemoryAccess access)
{
    return static_cast<std::uint32_t>(access);
}

bool IsValidAccess(runtime::GuestMemoryAccess access)
{
    return (AccessValue(access) & ~protocol::kGuestMemoryAccessMask) == 0;
}

}  // namespace

class NativeHelperBackend::Impl
{
public:
    explicit Impl(std::filesystem::path path) : path_(std::move(path)) {}

    ~Impl()
    {
        Stop();
    }

    bool Prepare(std::span<const std::uint8_t> file,
                 const exe::PeImageInfo& info,
                 runtime::GuestAddress requested,
                 runtime::LoadedPeImage* loaded,
                 std::string* error)
    {
        if (state_ != State::kIdle || loaded == nullptr || file.empty() ||
            file.size() > protocol::kMaximumPayloadSize - sizeof(protocol::LoadImageRequest) ||
            !exe::IsGuestExecutable(info))
        {
            SetError(error, "invalid Linux native helper image arguments");
            return false;
        }
        const std::uint32_t base =
            requested.value() == 0 ? static_cast<std::uint32_t>(info.image_base) : requested.value();
        if (!Launch(error) || !Negotiate(error))
        {
            StopAfterPrepareFailure();
            return false;
        }

        protocol::LoadImageRequest request{base, static_cast<std::uint32_t>(file.size())};
        std::vector<std::uint8_t> payload(sizeof(request) + file.size());
        std::memcpy(payload.data(), &request, sizeof(request));
        std::memcpy(payload.data() + sizeof(request), file.data(), file.size());
        if (!Send(protocol::MessageType::kLoadImage,
                  payload.data(),
                  static_cast<std::uint32_t>(payload.size()),
                  error))
        {
            Stop();
            return false;
        }

        protocol::LoadResult result;
        if (!Receive(protocol::MessageType::kLoadResult, &result, error) || result.success != 1 ||
            result.load_base != base || result.entry_point != base + info.entry_point_rva ||
            result.import_count > protocol::kMaximumImportCount)
        {
            if (error == nullptr || error->empty())
            {
                SetError(error, "Linux helper returned inconsistent image metadata");
            }
            Stop();
            return false;
        }

        loaded->load_base = runtime::GuestAddress(result.load_base);
        loaded->entry_point = runtime::GuestAddress(result.entry_point);
        loaded->imports.clear();
        for (std::uint32_t index = 0; index < result.import_count; ++index)
        {
            runtime::ImportGate gate;
            if (!ReceiveMetadata(&gate, error))
            {
                Stop();
                return false;
            }
            loaded->imports.push_back(std::move(gate));
        }
        const auto* tls = info.Directory(exe::PeDirectoryIndex::kTls);
        loaded->tls_directory_rva = tls == nullptr ? 0 : tls->virtual_address;
        loaded->tls_directory_size = tls == nullptr ? 0 : tls->size;
        state_ = State::kPrepared;
        return true;
    }

    bool Start(std::string* error)
    {
        if (state_ != State::kPrepared || !Send(protocol::MessageType::kStart, nullptr, 0, error))
        {
            SetError(error, "Linux helper cannot start image");
            Stop();
            return false;
        }
        state_ = State::kRunning;
        return true;
    }

    bool Wait(runtime::ExecutionEvent* event, std::string* error)
    {
        if (state_ != State::kRunning || event == nullptr)
        {
            SetError(error, "Linux helper is not running");
            return false;
        }
        protocol::ExecutionEvent packet;
        if (!Receive(protocol::MessageType::kExecutionEvent, &packet, error))
        {
            return ReportChildFailure(event, error);
        }
        event->event_id = EventId(packet.event_id_low, packet.event_id_high);
        event->thread_id = packet.thread_id;
        event->instruction_pointer = runtime::GuestAddress(packet.instruction_pointer);
        event->stack_pointer = runtime::GuestAddress(packet.stack_pointer);
        event->gate_address = runtime::GuestAddress(packet.gate_address);
        event->status_code = packet.status_code;
        if (packet.kind == static_cast<std::uint32_t>(protocol::EventKind::kImportGate))
        {
            event->kind = runtime::ExecutionEventKind::kImportGate;
            pending_ = event->event_id;
            state_ = State::kPending;
            return true;
        }
        if (packet.kind == static_cast<std::uint32_t>(protocol::EventKind::kProcessExit))
        {
            event->kind = runtime::ExecutionEventKind::kProcessExit;
            state_ = State::kExited;
            Close();
            return true;
        }
        event->kind = runtime::ExecutionEventKind::kFault;
        state_ = State::kFailed;
        return true;
    }

    bool Read(runtime::GuestAddress address,
              std::span<std::uint8_t> bytes,
              std::string* error)
    {
        if (!ValidateTransfer(bytes.size(), error))
        {
            return false;
        }
        protocol::ReadMemoryRequest request{address.value(), static_cast<std::uint32_t>(bytes.size())};
        return Send(protocol::MessageType::kReadMemory, &request, sizeof(request), error) &&
               ReceiveBytes(protocol::MessageType::kMemoryData, bytes, error);
    }

    bool Write(runtime::GuestAddress address,
               std::span<const std::uint8_t> bytes,
               std::string* error)
    {
        if (!ValidateTransfer(bytes.size(), error))
        {
            return false;
        }
        protocol::ReadMemoryRequest request{address.value(), static_cast<std::uint32_t>(bytes.size())};
        std::vector<std::uint8_t> payload(sizeof(request) + bytes.size());
        std::memcpy(payload.data(), &request, sizeof(request));
        if (!bytes.empty())
        {
            std::memcpy(payload.data() + sizeof(request), bytes.data(), bytes.size());
        }
        protocol::WriteMemoryResult result;
        if (!Send(protocol::MessageType::kWriteMemory,
                  payload.data(),
                  static_cast<std::uint32_t>(payload.size()),
                  error) ||
            !Receive(protocol::MessageType::kWriteResult, &result, error) || result.success != 1 ||
            result.size != bytes.size())
        {
            if (error == nullptr || error->empty())
            {
                SetError(error, "cannot write Linux helper memory");
            }
            return false;
        }
        return true;
    }

    bool Allocate(std::uint32_t size,
                  runtime::GuestMemoryAccess access,
                  runtime::GuestAddress* address,
                  std::uint32_t* allocated_size,
                  std::string* error)
    {
        if (!ValidatePending(error) || size == 0 || address == nullptr || allocated_size == nullptr ||
            !IsValidAccess(access))
        {
            SetError(error, "invalid guest memory allocation arguments");
            return false;
        }
        protocol::AllocateMemoryRequest request{size, AccessValue(access)};
        protocol::AllocateMemoryResult result;
        if (!Send(protocol::MessageType::kAllocateMemory, &request, sizeof(request), error) ||
            !Receive(protocol::MessageType::kAllocateMemoryResult, &result, error) || result.address == 0 ||
            result.size < size || (result.size & 4095U) != 0)
        {
            if (error == nullptr || error->empty())
            {
                SetError(error, "Linux helper could not allocate guest memory");
            }
            return false;
        }
        *address = runtime::GuestAddress(result.address);
        *allocated_size = result.size;
        return true;
    }

    bool Protect(runtime::GuestAddress address,
                 std::uint32_t size,
                 runtime::GuestMemoryAccess access,
                 runtime::GuestMemoryAccess* previous_access,
                 std::string* error)
    {
        if (!ValidatePending(error) || size == 0 || previous_access == nullptr || !IsValidAccess(access))
        {
            SetError(error, "invalid guest memory protection arguments");
            return false;
        }
        protocol::ProtectMemoryRequest request{address.value(), size, AccessValue(access)};
        protocol::ProtectMemoryResult result;
        if (!Send(protocol::MessageType::kProtectMemory, &request, sizeof(request), error) ||
            !Receive(protocol::MessageType::kProtectMemoryResult, &result, error) ||
            result.previous_access > protocol::kGuestMemoryAccessMask)
        {
            if (error == nullptr || error->empty())
            {
                SetError(error, "Linux helper could not protect guest memory");
            }
            return false;
        }
        *previous_access = static_cast<runtime::GuestMemoryAccess>(result.previous_access);
        return true;
    }

    bool Free(runtime::GuestAddress address, std::string* error)
    {
        if (!ValidatePending(error))
        {
            return false;
        }
        protocol::FreeMemoryRequest request{address.value()};
        protocol::FreeMemoryResult result;
        if (!Send(protocol::MessageType::kFreeMemory, &request, sizeof(request), error) ||
            !Receive(protocol::MessageType::kFreeMemoryResult, &result, error) || result.released_size == 0)
        {
            if (error == nullptr || error->empty())
            {
                SetError(error, "Linux helper could not free guest memory");
            }
            return false;
        }
        return true;
    }

    bool Complete(const runtime::ImportCompletion& completion, std::string* error)
    {
        if (state_ != State::kPending || completion.event_id != pending_)
        {
            SetError(error, "invalid Linux helper import completion");
            return false;
        }
        if (completion.action == runtime::ImportCompletionAction::kStop)
        {
            pending_ = 0;
            Stop();
            state_ = State::kStopped;
            return true;
        }
        protocol::CompleteImport packet;
        packet.event_id_low = static_cast<std::uint32_t>(completion.event_id);
        packet.event_id_high = static_cast<std::uint32_t>(completion.event_id >> 32);
        packet.eax = completion.eax;
        packet.edx = completion.edx;
        packet.stack_bytes_to_pop = completion.stack_bytes_to_pop;
        packet.action = 0;
        if (!Send(protocol::MessageType::kCompleteImport, &packet, sizeof(packet), error))
        {
            Stop();
            return false;
        }
        state_ = State::kRunning;
        return true;
    }

    void Stop()
    {
        Close();
        if (pid_ > 0)
        {
            kill(pid_, SIGKILL);
            while (waitpid(pid_, nullptr, 0) < 0 && errno == EINTR)
            {
            }
            pid_ = -1;
        }
        if (state_ != State::kExited)
        {
            state_ = State::kFailed;
        }
    }

private:
    enum class State
    {
        kIdle,
        kPrepared,
        kRunning,
        kPending,
        kExited,
        kStopped,
        kFailed,
    };

    void StopAfterPrepareFailure()
    {
        Close();
        if (pid_ <= 0)
        {
            state_ = State::kFailed;
            return;
        }

        constexpr int kReapAttempts = 25;
        constexpr timespec kReapDelay = {0, 10 * 1000 * 1000};
        for (int attempt = 0; attempt < kReapAttempts; ++attempt)
        {
            const pid_t result = waitpid(pid_, nullptr, WNOHANG);
            if (result == pid_ || (result < 0 && errno != EINTR))
            {
                pid_ = -1;
                state_ = State::kFailed;
                return;
            }
            nanosleep(&kReapDelay, nullptr);
        }
        Stop();
    }

    bool ReportChildFailure(runtime::ExecutionEvent* event, std::string* error)
    {
        int status = 0;
        const pid_t result = waitpid(pid_, &status, WNOHANG);
        if (result == pid_)
        {
            pid_ = -1;
            Close();
            state_ = State::kFailed;
            if (WIFSIGNALED(status))
            {
                *event = {};
                event->kind = runtime::ExecutionEventKind::kFault;
                event->status_code = static_cast<std::uint32_t>(WTERMSIG(status));
                if (error != nullptr)
                {
                    error->clear();
                }
                return true;
            }
            if (error != nullptr && error->empty())
            {
                *error = "Linux helper exited before reporting an execution event";
            }
            return false;
        }
        Stop();
        return false;
    }

    bool Launch(std::string* error)
    {
        int input_pipe[2] = {-1, -1};
        int output_pipe[2] = {-1, -1};
        if (pipe(input_pipe) != 0 || pipe(output_pipe) != 0)
        {
            SetError(error, "cannot create Linux helper pipes");
            return false;
        }
        pid_ = fork();
        if (pid_ == 0)
        {
            dup2(input_pipe[0], STDIN_FILENO);
            dup2(output_pipe[1], STDOUT_FILENO);
            close(input_pipe[0]);
            close(input_pipe[1]);
            close(output_pipe[0]);
            close(output_pipe[1]);
            execl(path_.c_str(), path_.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        if (pid_ < 0)
        {
            close(input_pipe[0]);
            close(input_pipe[1]);
            close(output_pipe[0]);
            close(output_pipe[1]);
            SetError(error, "cannot fork Linux helper");
            return false;
        }
        close(input_pipe[0]);
        close(output_pipe[1]);
        input_ = input_pipe[1];
        output_ = output_pipe[0];
        return true;
    }

    bool Negotiate(std::string* error)
    {
        protocol::HelloRequest request;
        request.required_features = protocol::kSupportedFeatures;
        protocol::HelloResult result;
        if (!Send(protocol::MessageType::kHello, &request, sizeof(request), error) ||
            !Receive(protocol::MessageType::kHelloResult, &result, error))
        {
            return false;
        }
        if ((result.supported_features & request.required_features) !=
            request.required_features)
        {
            SetError(error, "Linux helper does not support the required protocol features");
            return false;
        }
        return true;
    }

    void Close()
    {
        if (input_ >= 0)
        {
            close(input_);
            input_ = -1;
        }
        if (output_ >= 0)
        {
            close(output_);
            output_ = -1;
        }
    }

    bool Send(protocol::MessageType type,
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
            SetError(error, "cannot write Linux helper packet");
            return false;
        }
        return true;
    }

    bool ReceiveHeader(protocol::MessageHeader* header, std::string* error)
    {
        if (!ReadExact(output_, header, sizeof(*header)) || header->magic != protocol::kMagic ||
            header->version != protocol::kVersion ||
            header->payload_size > protocol::kMaximumPayloadSize)
        {
            SetError(error, "invalid Linux helper packet");
            return false;
        }
        return true;
    }

    bool ReadError(std::uint32_t payload_size, std::string* error)
    {
        std::vector<char> message(payload_size + 1, 0);
        if (payload_size != 0 && !ReadExact(output_, message.data(), payload_size))
        {
            SetError(error, "cannot read Linux helper error");
            return false;
        }
        SetError(error, message.data());
        return false;
    }

    template <typename Payload>
    bool Receive(protocol::MessageType expected, Payload* payload, std::string* error)
    {
        protocol::MessageHeader header;
        if (!ReceiveHeader(&header, error))
        {
            return false;
        }
        if (header.type == static_cast<std::uint32_t>(protocol::MessageType::kError))
        {
            return ReadError(header.payload_size, error);
        }
        if (header.type != static_cast<std::uint32_t>(expected) ||
            header.payload_size != sizeof(Payload) || !ReadExact(output_, payload, sizeof(Payload)))
        {
            SetError(error, "unexpected Linux helper packet");
            return false;
        }
        return true;
    }

    bool ReceiveBytes(protocol::MessageType expected,
                      std::span<std::uint8_t> bytes,
                      std::string* error)
    {
        protocol::MessageHeader header;
        if (!ReceiveHeader(&header, error))
        {
            return false;
        }
        if (header.type == static_cast<std::uint32_t>(protocol::MessageType::kError))
        {
            return ReadError(header.payload_size, error);
        }
        if (header.type != static_cast<std::uint32_t>(expected) ||
            header.payload_size != bytes.size() ||
            (!bytes.empty() &&
             !ReadExact(output_, bytes.data(), static_cast<std::uint32_t>(bytes.size()))))
        {
            SetError(error, "cannot read Linux helper memory");
            return false;
        }
        return true;
    }

    bool ReceiveMetadata(runtime::ImportGate* gate, std::string* error)
    {
        protocol::MessageHeader header;
        if (!ReceiveHeader(&header, error) ||
            header.type != static_cast<std::uint32_t>(protocol::MessageType::kImportMetadata) ||
            header.payload_size < sizeof(protocol::ImportMetadata))
        {
            SetError(error, "invalid import metadata");
            return false;
        }
        protocol::ImportMetadata metadata;
        if (!ReadExact(output_, &metadata, sizeof(metadata)) || metadata.module_size == 0 ||
            metadata.module_size > protocol::kMaximumImportStringSize ||
            metadata.name_size > protocol::kMaximumImportStringSize ||
            header.payload_size != sizeof(metadata) + metadata.module_size + metadata.name_size)
        {
            SetError(error, "invalid import metadata payload");
            return false;
        }
        gate->module.resize(metadata.module_size);
        gate->name.resize(metadata.name_size);
        if (!ReadExact(output_, gate->module.data(), metadata.module_size) ||
            (metadata.name_size != 0 &&
             !ReadExact(output_, gate->name.data(), metadata.name_size)))
        {
            SetError(error, "cannot read import metadata");
            return false;
        }
        gate->address = runtime::GuestAddress(metadata.gate_address);
        gate->by_ordinal = metadata.by_ordinal != 0;
        gate->ordinal = static_cast<std::uint16_t>(metadata.ordinal);
        return true;
    }

    bool ValidatePending(std::string* error) const
    {
        if (state_ != State::kPending)
        {
            SetError(error, "guest memory is available only while an import is pending");
            return false;
        }
        return true;
    }

    bool ValidateTransfer(std::size_t size, std::string* error) const
    {
        if (!ValidatePending(error))
        {
            return false;
        }
        if (size > kTransferLimit)
        {
            SetError(error, "guest memory transfer exceeds the protocol limit");
            return false;
        }
        return true;
    }

    std::filesystem::path path_;
    pid_t pid_ = -1;
    int input_ = -1;
    int output_ = -1;
    State state_ = State::kIdle;
    std::uint64_t pending_ = 0;
};

NativeHelperBackend::NativeHelperBackend(std::filesystem::path path)
    : impl_(std::make_unique<Impl>(std::move(path)))
{
}

NativeHelperBackend::~NativeHelperBackend() = default;

bool NativeHelperBackend::PrepareImage(std::span<const std::uint8_t> file_bytes,
                                       const exe::PeImageInfo& info,
                                       runtime::GuestAddress requested_base,
                                       runtime::LoadedPeImage* loaded,
                                       std::string* error)
{
    return impl_->Prepare(file_bytes, info, requested_base, loaded, error);
}

bool NativeHelperBackend::Start(std::string* error)
{
    return impl_->Start(error);
}

bool NativeHelperBackend::WaitForEvent(runtime::ExecutionEvent* event, std::string* error)
{
    return impl_->Wait(event, error);
}

bool NativeHelperBackend::ReadMemory(runtime::GuestAddress address,
                                     std::span<std::uint8_t> bytes,
                                     std::string* error)
{
    return impl_->Read(address, bytes, error);
}

bool NativeHelperBackend::WriteMemory(runtime::GuestAddress address,
                                      std::span<const std::uint8_t> bytes,
                                      std::string* error)
{
    return impl_->Write(address, bytes, error);
}

bool NativeHelperBackend::AllocateGuestMemory(std::uint32_t size,
                                              runtime::GuestMemoryAccess access,
                                              runtime::GuestAddress* address,
                                              std::uint32_t* allocated_size,
                                              std::string* error)
{
    return impl_->Allocate(size, access, address, allocated_size, error);
}

bool NativeHelperBackend::ProtectGuestMemory(runtime::GuestAddress address,
                                             std::uint32_t size,
                                             runtime::GuestMemoryAccess access,
                                             runtime::GuestMemoryAccess* previous_access,
                                             std::string* error)
{
    return impl_->Protect(address, size, access, previous_access, error);
}

bool NativeHelperBackend::FreeGuestMemory(runtime::GuestAddress address, std::string* error)
{
    return impl_->Free(address, error);
}

bool NativeHelperBackend::CompleteImport(const runtime::ImportCompletion& completion,
                                         std::string* error)
{
    return impl_->Complete(completion, error);
}

void NativeHelperBackend::RequestStop()
{
    impl_->Stop();
}

}  // namespace re2dj::platform::linux
