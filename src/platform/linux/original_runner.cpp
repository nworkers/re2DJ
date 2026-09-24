#include "re2dj/platform/linux/original_runner.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include <signal.h>

#include "re2dj/platform/linux/native_helper_backend.h"
#include "native_in_process_runner.h"
#include "native_kernel32_diagnostic.h"
#include "re2dj/runtime/execution_backend.h"
#include "re2dj/runtime/pe_loader.h"
#include "../native_helper_protocol.h"

namespace re2dj::platform::linux
{
namespace
{

namespace protocol = re2dj::platform::native_protocol;

const runtime::ImportGate* FindImport(const runtime::LoadedPeImage& image,
                                      runtime::GuestAddress gate_address)
{
    for (const runtime::ImportGate& gate : image.imports)
    {
        if (gate.address == gate_address)
        {
            return &gate;
        }
    }
    return nullptr;
}

std::uint32_t ReadLe32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

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
        context.matched && run.fault.status_code == SIGTRAP &&
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
    if (!interrupted || !context.observed || run.fault.status_code != SIGTRAP ||
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

bool RunOriginalUntilBoundary(const std::filesystem::path& executable_path,
                              const exe::PeImageInfo& image_info,
                              const std::filesystem::path& helper_path,
                              OriginalRunResult* result,
                              std::string* error)
{
    if (executable_path.empty() || helper_path.empty() || result == nullptr || error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "invalid Linux original-run arguments";
        }
        return false;
    }

    *result = OriginalRunResult{};

    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error))
    {
        return false;
    }

    NativeHelperBackend backend(helper_path);
    runtime::LoadedPeImage loaded;
    if (!backend.PrepareImage(file_bytes, image_info, runtime::GuestAddress(), &loaded, error) ||
        !backend.Start(error))
    {
        return false;
    }

    runtime::ExecutionEvent event;
    if (!backend.WaitForEvent(&event, error))
    {
        return false;
    }

    result->load_base = loaded.load_base;
    result->entry_point = loaded.entry_point;
    result->instruction_pointer = event.instruction_pointer;
    result->stack_pointer = event.stack_pointer;
    result->gate_address = event.gate_address;
    result->status_code = event.status_code;

    switch (event.kind)
    {
    case runtime::ExecutionEventKind::kImportGate:
    {
        result->boundary = OriginalRunBoundary::kImportGate;
        const runtime::ImportGate* gate = FindImport(loaded, event.gate_address);
        if (gate == nullptr)
        {
            backend.RequestStop();
            *error = "helper reported an unknown import gate address";
            return false;
        }
        result->module = gate->module;
        result->name = gate->name;
        result->by_ordinal = gate->by_ordinal;
        result->ordinal = gate->ordinal;
        std::array<std::uint8_t, 8> stack_words{};
        if (!backend.ReadMemory(event.stack_pointer, stack_words, error))
        {
            backend.RequestStop();
            if (error->empty())
            {
                *error = "cannot read first Linux import stack words";
            }
            return false;
        }
        result->import_stack_observed = true;
        result->import_return_address = ReadLe32(stack_words.data());
        result->import_first_argument = ReadLe32(stack_words.data() + sizeof(std::uint32_t));
        if (result->import_first_argument != 0)
        {
            std::array<std::uint8_t, protocol::kMaximumImportStringSize> argument_bytes{};
            if (!backend.ReadMemory(runtime::GuestAddress(result->import_first_argument),
                                    argument_bytes,
                                    error))
            {
                backend.RequestStop();
                if (error->empty())
                {
                    *error = "cannot read first Linux import argument text";
                }
                return false;
            }
            const auto terminator = std::find(argument_bytes.begin(), argument_bytes.end(), 0);
            if (terminator == argument_bytes.end())
            {
                backend.RequestStop();
                *error = "first Linux import argument has no bounded terminator";
                return false;
            }
            result->import_first_argument_text_observed = true;
            result->import_first_argument_text.assign(argument_bytes.begin(), terminator);
        }
        backend.RequestStop();
        break;
    }
    case runtime::ExecutionEventKind::kProcessExit:
        result->boundary = OriginalRunBoundary::kProcessExit;
        break;
    case runtime::ExecutionEventKind::kFault:
        result->boundary = OriginalRunBoundary::kFault;
        backend.RequestStop();
        break;
    case runtime::ExecutionEventKind::kThreadExit:
    case runtime::ExecutionEventKind::kStopped:
        result->boundary = OriginalRunBoundary::kStopped;
        backend.RequestStop();
        break;
    }

    error->clear();
    return true;
}

}  // namespace re2dj::platform::linux
