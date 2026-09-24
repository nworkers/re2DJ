#include "re2dj/platform/linux/original_runner.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <signal.h>

#include "native_in_process_runner.h"
#include "native_kernel32_diagnostic.h"

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

// Indices of up to two ANSI string arguments for facade exports that take
// them, -1 for none. A by-ordinal GetProcAddress request fails the string
// read and records none.
struct StringArguments
{
    int first = -1;
    int second = -1;
};

StringArguments StringArgumentIndices(std::string_view export_name)
{
    if (export_name == "GetModuleHandleA" || export_name == "CreateFileA")
    {
        return {0, -1};
    }
    if (export_name == "GetProcAddress")
    {
        return {1, -1};
    }
    if (export_name == "MessageBoxA")
    {
        // lpText, then lpCaption.
        return {1, 2};
    }
    return {};
}

struct ContinuationContext
{
    ContinuationContext(std::uint32_t image_base, std::uint32_t image_size)
        : kernel32(image_base, image_size)
    {
    }

    NativeKernel32Diagnostic kernel32;
    std::vector<OriginalApiCall> calls;
    std::uint32_t call_count = 0;
    bool stop_requested = false;
    bool stop_redirect_failed = false;
    OriginalRunBoundary stop = OriginalRunBoundary::kStopped;
    std::string stop_detail;
};

void RequestStop(ContinuationContext* state,
                 const NativeImportGateEvent& event,
                 OriginalRunBoundary boundary,
                 std::string detail)
{
    state->stop_requested = true;
    state->stop = boundary;
    state->stop_detail = std::move(detail);
    state->stop_redirect_failed = !state->kernel32.RedirectReturnToStop(event);
}

bool HandleContinuationGate(const NativeImportGateEvent& event,
                            NativeImportGateResult* output,
                            void* opaque)
{
    auto* state = static_cast<ContinuationContext*>(opaque);
    if (output == nullptr || state == nullptr || state->stop_requested)
    {
        return false;
    }

    OriginalApiCall call;
    call.sequence = ++state->call_count;
    call.name = state->kernel32.GateName(event);
    call.return_address = event.instruction_pointer;
    const hle::modules::RegisteredGuestExport* facade_export =
        state->kernel32.FindFacadeExport(event);
    if (facade_export != nullptr)
    {
        call.argument_count = (std::min)(facade_export->descriptor.argument_count,
                                         static_cast<std::uint32_t>(call.arguments.size()));
        const std::size_t argument_bytes = call.argument_count * sizeof(std::uint32_t);
        if (event.stack_pointer >= event.guest_stack_limit &&
            event.stack_pointer <= event.guest_stack_base &&
            sizeof(std::uint32_t) + argument_bytes <=
                event.guest_stack_base - event.stack_pointer)
        {
            std::memcpy(call.arguments.data(),
                        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                            event.stack_pointer + sizeof(std::uint32_t))),
                        argument_bytes);
        }
        else
        {
            call.argument_count = 0;
        }
        const StringArguments strings = StringArgumentIndices(facade_export->descriptor.name);
        if (strings.first >= 0)
        {
            call.text_observed = state->kernel32.ReadArgumentString(
                event, static_cast<std::uint32_t>(strings.first), &call.text);
        }
        if (strings.second >= 0)
        {
            call.second_text_observed = state->kernel32.ReadArgumentString(
                event, static_cast<std::uint32_t>(strings.second), &call.second_text);
        }
    }

    call.handled = state->kernel32.Dispatch(event, output);
    call.eax = call.handled ? output->eax : 0;
    if (state->calls.size() < kOriginalApiCallLogMaximum)
    {
        state->calls.push_back(call);
    }

    if (!call.handled)
    {
        RequestStop(state, event, OriginalRunBoundary::kContinuationUnhandledImport, call.name);
        return false;
    }
    // A null GetModuleHandleA result is a legitimate Win32 outcome the guest
    // may tolerate, so it is only logged. A null GetProcAddress result, including
    // one for a null module handle, leaves the guest holding no entry point.
    const bool resolver = facade_export != nullptr &&
                          facade_export->descriptor.name == "GetProcAddress";
    if (resolver && call.eax == 0)
    {
        const std::string requested = call.text_observed
            ? call.text
            : "#" + std::to_string(call.arguments[1] & 0xFFFFU);
        char module[16];
        std::snprintf(module, sizeof(module), "%08x", call.arguments[0]);
        RequestStop(state,
                    event,
                    OriginalRunBoundary::kContinuationUnresolvedLookup,
                    "GetProcAddress(" + std::string(module) + ", " + requested + ")");
    }
    else if (state->call_count >= kOriginalContinuationCallLimit)
    {
        RequestStop(state, event, OriginalRunBoundary::kContinuationCallLimit, call.name);
    }
    return true;
}

}  // namespace

bool RunOriginalInProcessContinuation(const std::filesystem::path& executable_path,
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

    ContinuationContext context(static_cast<std::uint32_t>(image_info.image_base),
                                image_info.size_of_image);
    if (!context.kernel32.PrepareStopStub(error))
    {
        return false;
    }

    NativeInProcessRunResult run;
    *result = {};
    const bool completed = RunConfiguredNativePeInProcess(file_bytes,
                                                          image_info,
                                                          context.kernel32.image_base(),
                                                          &HandleContinuationGate,
                                                          &context,
                                                          &NativeKernel32Diagnostic::Setup,
                                                          &context.kernel32,
                                                          &run,
                                                          error);
    const std::uint32_t stop_address = context.kernel32.stop_stub();
    if (context.stop_requested && !context.stop_redirect_failed &&
        run.fault.status_code == SIGTRAP && run.fault.instruction_pointer == stop_address + 1)
    {
        result->boundary = context.stop;
        result->import_return_address = context.kernel32.stopped_return_address();
        result->instruction_pointer = runtime::GuestAddress(context.kernel32.stopped_return_address());
    }
    else if (completed)
    {
        result->boundary = OriginalRunBoundary::kProcessExit;
        result->status_code = run.exit_code;
        char detail[40];
        std::snprintf(detail,
                      sizeof(detail),
                      run.process_exited ? "ExitProcess(0x%08x)" : "entry returned 0x%08x",
                      run.exit_code);
        context.stop_detail = detail;
    }
    else if (context.kernel32.prepared() && run.fault.status_code != 0)
    {
        result->boundary = OriginalRunBoundary::kContinuationFault;
        result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
        result->status_code = run.fault.status_code;
        CopyNativeFaultObservation(run, result);
    }
    else
    {
        const std::string cause = error->empty() ? "" : ": " + *error;
        *error = "continuation did not reach a confirmed diagnostic boundary (calls=" +
                 std::to_string(context.call_count) +
                 ", stop_requested=" + std::to_string(context.stop_requested ? 1 : 0) +
                 ", redirect_failed=" + std::to_string(context.stop_redirect_failed ? 1 : 0) +
                 ", signal=" + std::to_string(run.fault.status_code) +
                 ", eip=" + std::to_string(run.fault.instruction_pointer) + ")" + cause;
        return false;
    }

    const std::uint32_t image_base = context.kernel32.image_base();
    result->load_base = runtime::GuestAddress(image_base);
    result->entry_point = runtime::GuestAddress(image_base + image_info.entry_point_rva);
    result->seh_dispatch_count = run.seh_dispatch_count;
    result->last_seh_handler = runtime::GuestAddress(run.last_seh_handler);
    result->last_seh_resumed_eip = runtime::GuestAddress(run.last_seh_resumed_eip);
    result->api_calls = std::move(context.calls);
    result->api_call_count = context.call_count;
    result->continuation_stop_detail = context.stop_detail;
    context.kernel32.CopyTo(result);
    error->clear();
    return true;
}

}  // namespace re2dj::platform::linux
