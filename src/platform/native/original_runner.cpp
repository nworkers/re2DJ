#include "re2dj/platform/native/original_runner.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>


#include "native_in_process_runner.h"
#include "native_kernel32_diagnostic.h"
#include "re2dj/runtime/execution_backend.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::platform::native
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

}  // namespace

bool RunOriginalInProcessFirstImport(const std::filesystem::path& executable_path,
                                     const exe::PeImageInfo& image_info,
                                     OriginalRunResult* result,
                                     std::string* error)
{
    if (result == nullptr || error == nullptr)
    {
        return false;
    }
    struct Context { std::uint32_t return_address = 0; bool matched = false; } context;
    auto handler = [](const NativeImportGateEvent& event, NativeImportGateResult* output, void* opaque) {
        auto* context = static_cast<Context*>(opaque);
        const auto* stack = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(event.stack_pointer));
        const char* argument = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(stack[1]));
        if (output == nullptr || std::strcmp(argument, "kernel32") != 0) return false;
        context->return_address = stack[0]; context->matched = true;
        *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(stack[0])) = 0xCC;
        output->eax = 1; output->stack_bytes_to_pop = 4; return true;
    };
    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error)) return false;
    NativeInProcessRunResult run;
    *result = {};
    const bool completed = !RunNativePeInProcess(file_bytes, image_info,
        static_cast<std::uint32_t>(image_info.image_base), handler, &context, &run, error) &&
        context.matched && run.fault.kind == NativeFaultKind::kBreakpoint &&
        run.fault.instruction_pointer == context.return_address + 1;
    if (!completed) return false;
    result->boundary = OriginalRunBoundary::kFirstImportCompleted;
    result->load_base = runtime::GuestAddress(static_cast<std::uint32_t>(image_info.image_base));
    result->entry_point = runtime::GuestAddress(static_cast<std::uint32_t>(image_info.image_base) + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->import_return_address = context.return_address;
    result->status_code = run.fault.status_code;
    result->import_stack_observed = true;
    result->import_first_argument_text_observed = true;
    result->import_first_argument_text = "kernel32";
    error->clear(); return true;
}

bool RunOriginalInProcessFirstResolver(const std::filesystem::path& executable_path,
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
        bool observed = false;
        std::uint32_t return_address = 0;
    } context(static_cast<std::uint32_t>(image_info.image_base), image_info.size_of_image);

    auto handler = [](const NativeImportGateEvent& event,
                      NativeImportGateResult* output,
                      void* opaque) {
        auto* state = static_cast<Context*>(opaque);
        if (output == nullptr || state == nullptr)
        {
            return false;
        }
        std::string name;
        if (!state->observed && state->kernel32.IsExportCall(event, "GetProcAddress") &&
            state->kernel32.ReadRequestedExportName(event, &name) && name == "GetVersion")
        {
            const auto* stack = reinterpret_cast<const std::uint32_t*>(
                static_cast<std::uintptr_t>(event.stack_pointer));
            if (!state->kernel32.ImageContains(stack[0], 1))
            {
                return false;
            }
            state->observed = true;
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
    if (!interrupted || !context.observed || run.fault.kind != NativeFaultKind::kBreakpoint ||
        run.fault.instruction_pointer != context.return_address + 1)
    {
        if (error->empty())
        {
            *error = "GetProcAddress(GetVersion) did not reach a confirmed diagnostic boundary";
        }
        return false;
    }
    const std::uint32_t image_base = context.kernel32.image_base();
    result->boundary = OriginalRunBoundary::kFirstResolverObserved;
    result->load_base = runtime::GuestAddress(image_base);
    result->entry_point = runtime::GuestAddress(image_base + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->import_return_address = context.return_address;
    result->status_code = run.fault.status_code;
    result->import_first_argument_text_observed = true;
    result->import_first_argument_text = "GetVersion";
    context.kernel32.CopyTo(result);
    error->clear();
    return true;
}

}  // namespace re2dj::platform::native
