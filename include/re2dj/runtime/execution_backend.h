#ifndef RE2DJ_RUNTIME_EXECUTION_BACKEND_H_
#define RE2DJ_RUNTIME_EXECUTION_BACKEND_H_

#include <cstdint>
#include <span>
#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/address_space.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::runtime
{

enum class ExecutionEventKind : std::uint8_t
{
    kImportGate,
    kThreadExit,
    kProcessExit,
    kFault,
    kStopped,
};

struct ExecutionEvent
{
    ExecutionEventKind kind = ExecutionEventKind::kStopped;
    std::uint64_t event_id = 0;
    std::uint32_t thread_id = 0;
    GuestAddress instruction_pointer;
    GuestAddress stack_pointer;
    GuestAddress gate_address;
    std::uint32_t status_code = 0;
};

enum class ImportCompletionAction : std::uint8_t
{
    kContinue,
    kStop,
};

struct ImportCompletion
{
    std::uint64_t event_id = 0;
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    std::uint32_t stack_bytes_to_pop = 0;
    ImportCompletionAction action = ImportCompletionAction::kContinue;
};

// Execution backends may run in-process or behind a helper process. This
// event/reply boundary therefore carries guest values only and never exposes
// host pointers or backend-specific handles.
class ExecutionBackend
{
public:
    virtual ~ExecutionBackend() = default;

    virtual bool PrepareImage(std::span<const std::uint8_t> file_bytes,
                              const exe::PeImageInfo& info,
                              GuestAddress requested_base,
                              LoadedPeImage* loaded,
                              std::string* error) = 0;
    virtual bool Start(std::string* error) = 0;
    virtual bool WaitForEvent(ExecutionEvent* event, std::string* error) = 0;
    virtual bool ReadMemory(GuestAddress address,
                            std::span<std::uint8_t> bytes,
                            std::string* error) = 0;
    virtual bool WriteMemory(GuestAddress address,
                             std::span<const std::uint8_t> bytes,
                             std::string* error) = 0;
    virtual bool CompleteImport(const ImportCompletion& completion,
                                std::string* error) = 0;
    virtual void RequestStop() = 0;
};

}  // namespace re2dj::runtime

#endif  // RE2DJ_RUNTIME_EXECUTION_BACKEND_H_
