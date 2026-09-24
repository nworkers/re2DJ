#include "re2dj/platform/linux/original_runner.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <signal.h>

#include "native_in_process_runner.h"
#include "native_kernel32_diagnostic.h"

namespace re2dj::platform::linux
{
namespace
{

constexpr std::size_t kMaximumObservedPathLength = 260;

bool ReadExecutable(const std::filesystem::path& path,
                    std::vector<std::uint8_t>* bytes,
                    std::string* error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error = "cannot open the selected guest executable";
        return false;
    }
    bytes->assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (stream.bad() || bytes->empty())
    {
        *error = "cannot read the selected guest executable";
        return false;
    }
    return true;
}

struct CreateFileContext
{
    CreateFileContext(std::uint32_t image_base, std::uint32_t image_size)
        : kernel32(image_base, image_size)
    {
    }

    NativeKernel32Diagnostic kernel32;
    bool create_file_called = false;
    std::uint32_t return_address = 0;
    OriginalCreateFileObservation observation;
};

bool StackContains(const NativeImportGateEvent& event,
                   std::uint32_t address,
                   std::size_t size)
{
    return address >= event.guest_stack_limit && address <= event.guest_stack_base &&
           size <= event.guest_stack_base - address;
}

bool CopyGuestString(const NativeKernel32Diagnostic& kernel32,
                     const NativeImportGateEvent& event,
                     std::uint32_t address,
                     std::string* value)
{
    if (value == nullptr)
    {
        return false;
    }
    std::size_t available = 0;
    if (kernel32.ImageContains(address, 1))
    {
        available = static_cast<std::size_t>(
            kernel32.image_size() - (address - kernel32.image_base()));
    }
    else if (StackContains(event, address, 1))
    {
        available = static_cast<std::size_t>(event.guest_stack_base - address);
    }
    else
    {
        return false;
    }
    const std::size_t limit = (std::min)(available, kMaximumObservedPathLength + 1);
    const char* text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
    const void* terminator = std::memchr(text, '\0', limit);
    if (terminator == nullptr)
    {
        return false;
    }
    value->assign(text, static_cast<const char*>(terminator));
    return true;
}


}  // namespace

bool RunOriginalInProcessCreateFileCall(const std::filesystem::path& executable_path,
                                        const exe::PeImageInfo& image_info,
                                        OriginalRunResult* result,
                                        std::string* error)
{
    if (result == nullptr || error == nullptr)
    {
        return false;
    }

    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error))
    {
        return false;
    }

    CreateFileContext context(static_cast<std::uint32_t>(image_info.image_base),
                              image_info.size_of_image);

    auto handler = [](const NativeImportGateEvent& event,
                      NativeImportGateResult* output,
                      void* opaque) {
        auto* state = static_cast<CreateFileContext*>(opaque);
        const auto* stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(event.stack_pointer));
        if (output == nullptr || state == nullptr)
        {
            return false;
        }
        if (state->kernel32.IsExportCall(event, "CreateFileA") && !state->create_file_called)
        {
            if (!StackContains(event,
                               event.stack_pointer,
                               8 * sizeof(std::uint32_t)) ||
                !state->kernel32.ImageContains(stack[0], 1))
            {
                return false;
            }
            state->create_file_called = true;
            state->return_address = stack[0];
            state->observation.observed = true;
            state->observation.file_name_address = runtime::GuestAddress(stack[1]);
            state->observation.file_name_observed = CopyGuestString(
                state->kernel32, event, stack[1], &state->observation.file_name);
            state->observation.desired_access = stack[2];
            state->observation.share_mode = stack[3];
            state->observation.security_attributes = runtime::GuestAddress(stack[4]);
            state->observation.creation_disposition = stack[5];
            state->observation.flags_and_attributes = stack[6];
            state->observation.template_file = runtime::GuestAddress(stack[7]);
            *reinterpret_cast<std::uint8_t*>(
                static_cast<std::uintptr_t>(state->return_address)) = 0xCC;
        }
        return state->kernel32.Dispatch(event, output);
    };

    NativeInProcessRunResult run;
    *result = {};
    const bool interrupted = !RunConfiguredNativePeInProcess(file_bytes,
                                                               image_info,
                                                               context.kernel32.image_base(),
                                                               handler,
                                                               &context,
                                                               &NativeKernel32Diagnostic::Setup,
                                                               &context.kernel32,
                                                               &run,
                                                               error);
    if (context.create_file_called && run.fault.status_code == SIGTRAP &&
        run.fault.instruction_pointer == context.return_address + 1)
    {
        result->boundary = OriginalRunBoundary::kCreateFileCalled;
        result->import_return_address = context.return_address;
    }
    else if (interrupted && context.kernel32.prepared() &&
             run.fault.status_code != 0)
    {
        result->boundary = OriginalRunBoundary::kCreateFileCallNotReached;
    }
    else
    {
        *error = "CreateFileA continuation did not reach a confirmed diagnostic boundary";
        return false;
    }

    const std::uint32_t image_base = context.kernel32.image_base();
    result->load_base = runtime::GuestAddress(image_base);
    result->entry_point = runtime::GuestAddress(image_base + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->status_code = run.fault.status_code;
    result->create_file_observation = context.observation;
    context.kernel32.CopyTo(result);
    CopyNativeFaultObservation(run, result);
    error->clear();
    return true;
}

}  // namespace re2dj::platform::linux
