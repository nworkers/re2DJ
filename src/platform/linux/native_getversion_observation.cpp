#include "re2dj/platform/linux/original_runner.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <signal.h>

#include "native_in_process_runner.h"
#include "native_kernel32_diagnostic.h"
#include "native_instruction_trace.h"

namespace re2dj::platform::linux
{
namespace
{

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

void CopyInstructionTrace(const NativeInstructionTrace& source,
                          OriginalRunResult* destination)
{
    if (destination == nullptr || !source.armed)
    {
        return;
    }
    OriginalInstructionTrace& trace = destination->instruction_trace;
    trace.armed = source.armed;
    trace.started = source.started;
    trace.limit_reached = source.limit_reached;
    trace.breakpoint = runtime::GuestAddress(source.breakpoint);
    trace.frame_count = source.frame_count;
    if (trace.frame_count > trace.frames.size())
    {
        trace.frame_count = static_cast<std::uint32_t>(trace.frames.size());
    }
    for (std::uint32_t index = 0; index < trace.frame_count; ++index)
    {
        const NativeInstructionTraceFrame& from = source.frames[index];
        OriginalInstructionTraceFrame& to = trace.frames[index];
        to.instruction_pointer = runtime::GuestAddress(from.instruction_pointer);
        to.stack_pointer = runtime::GuestAddress(from.stack_pointer);
        to.eax = from.eax;
        to.ebx = from.ebx;
        to.ecx = from.ecx;
        to.edx = from.edx;
        to.esi = from.esi;
        to.edi = from.edi;
        to.ebp = from.ebp;
        to.eflags = from.eflags;
    }
}

}  // namespace

// Bounds execution at the first GetVersion call and single-steps from the
// GetProcAddress(GetVersion) return with the instruction trace.
bool RunOriginalInProcessGetVersionCall(const std::filesystem::path& executable_path,
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

    struct Context
    {
        Context(std::uint32_t image_base, std::uint32_t image_size)
            : kernel32(image_base, image_size)
        {
        }

        NativeKernel32Diagnostic kernel32;
        bool resolved = false;
        bool called = false;
        bool trace_arm_failed = false;
        std::uint32_t return_address = 0;
        NativeInstructionTrace trace;
        std::string trace_error;
    } context(static_cast<std::uint32_t>(image_info.image_base), image_info.size_of_image);

    auto handler = [](const NativeImportGateEvent& event,
                      NativeImportGateResult* output,
                      void* opaque) {
        auto* state = static_cast<Context*>(opaque);
        if (output == nullptr || state == nullptr)
        {
            return false;
        }
        const auto* stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(event.stack_pointer));
        std::string name;
        if (!state->resolved && state->kernel32.IsExportCall(event, "GetProcAddress") &&
            state->kernel32.ReadRequestedExportName(event, &name) && name == "GetVersion")
        {
            state->resolved = true;
            if (!ArmNativeInstructionTrace(&state->trace,
                                           stack[0],
                                           state->kernel32.image_base(),
                                           state->kernel32.image_size(),
                                           &state->trace_error))
            {
                state->trace_arm_failed = true;
                return false;
            }
        }
        else if (state->resolved && !state->called &&
                 state->kernel32.IsExportCall(event, "GetVersion"))
        {
            if (event.stack_pointer < event.guest_stack_limit ||
                event.stack_pointer >= event.guest_stack_base)
            {
                return false;
            }
            // The trace would re-arm on this return site and swallow our INT3.
            StopNativeInstructionTrace();
            state->called = true;
            state->return_address = stack[0];
            *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(stack[0])) = 0xCC;
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
    FinalizeNativeInstructionTrace(&context.trace);
    if (context.trace_arm_failed)
    {
        *error = context.trace_error;
        return false;
    }
    const bool resolved =
        context.resolved && context.kernel32.guest_get_version().value() != 0;
    if (context.called && run.fault.status_code == SIGTRAP &&
        run.fault.instruction_pointer == context.return_address + 1)
    {
        result->boundary = OriginalRunBoundary::kGetVersionCalled;
        result->import_return_address = context.return_address;
    }
    else if (interrupted && resolved && !context.called && run.fault.status_code != 0)
    {
        result->boundary = OriginalRunBoundary::kGetVersionCallNotReached;
    }
    else
    {
        const std::string cause = error->empty() ? "" : "; " + *error;
        *error = "GetVersion continuation did not reach a confirmed diagnostic boundary"
                 " (interrupted=" + std::to_string(interrupted ? 1 : 0) +
                 ", resolved=" + std::to_string(resolved ? 1 : 0) +
                 ", called=" + std::to_string(context.called ? 1 : 0) +
                 ", return=" + std::to_string(context.return_address) +
                 ", signal=" + std::to_string(run.fault.status_code) +
                 ", eip=" + std::to_string(run.fault.instruction_pointer) +
                 ", exit=" + std::to_string(run.exit_code) + cause + ")";
        return false;
    }
    const std::uint32_t image_base = context.kernel32.image_base();
    result->load_base = runtime::GuestAddress(image_base);
    result->entry_point = runtime::GuestAddress(image_base + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->status_code = run.fault.status_code;
    result->import_first_argument_text_observed = true;
    result->import_first_argument_text = "GetVersion";
    context.kernel32.CopyTo(result);
    CopyNativeFaultObservation(run, result);
    CopyInstructionTrace(context.trace, result);
    error->clear();
    return true;
}

}  // namespace re2dj::platform::linux
