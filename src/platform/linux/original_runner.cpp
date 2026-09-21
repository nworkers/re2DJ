#include "re2dj/platform/linux/original_runner.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include <signal.h>

#include "re2dj/platform/linux/native_helper_backend.h"
#if defined(__i386__)
#include "native_dynamic_thunk.h"
#include "native_in_process_runner.h"
#endif
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

#if defined(__i386__)
void CopyFaultObservation(const NativeInProcessRunResult& source,
                          OriginalRunResult* destination)
{
    if (destination == nullptr || source.fault.status_code == 0)
    {
        return;
    }
    OriginalFaultObservation& observation = destination->fault_observation;
    observation.observed = true;
    observation.fault_address = runtime::GuestAddress(source.fault.fault_address);
    observation.signal_code = source.fault.signal_code;
    observation.cpu_error_code = source.fault.cpu_error_code;
    observation.eax = source.fault.eax;
    observation.ebx = source.fault.ebx;
    observation.ecx = source.fault.ecx;
    observation.edx = source.fault.edx;
    observation.esi = source.fault.esi;
    observation.edi = source.fault.edi;
    observation.ebp = source.fault.ebp;
    observation.eflags = source.fault.eflags;
    observation.instruction_window_observed =
        source.fault_observation.instruction_window_observed;
    observation.instruction_window_address =
        runtime::GuestAddress(source.fault_observation.instruction_window_address);
    observation.instruction_window = source.fault_observation.instruction_window;
    observation.stack_words_observed = source.fault_observation.stack_words_observed;
    observation.stack_words = source.fault_observation.stack_words;
    observation.stack_word_count = source.fault_observation.stack_word_count;
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
#endif

}  // namespace

bool RunOriginalInProcessFirstImport(const std::filesystem::path& executable_path,
                                     const exe::PeImageInfo& image_info,
                                     OriginalRunResult* result,
                                     std::string* error)
{
#if !defined(__i386__)
    static_cast<void>(executable_path);
    static_cast<void>(image_info);
    static_cast<void>(result);
    if (error != nullptr)
    {
        *error = "Linux in-process first-import diagnostic requires an i386 host";
    }
    return false;
#else
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
#endif
}

bool RunOriginalInProcessFirstResolver(const std::filesystem::path& executable_path,
                                       const exe::PeImageInfo& image_info,
                                       OriginalRunResult* result,
                                       std::string* error)
{
#if !defined(__i386__)
    static_cast<void>(executable_path);
    static_cast<void>(image_info);
    static_cast<void>(result);
    if (error != nullptr)
    {
        *error = "Linux in-process resolver diagnostic requires an i386 host";
    }
    return false;
#else
    if (result == nullptr || error == nullptr) return false;
    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error)) return false;
    constexpr std::uint32_t kKernel32Module = 0x7F000001;
    struct Context { std::uint32_t base; std::uint32_t size; std::uint32_t return_address = 0; bool module = false; bool resolver = false; } context{
        static_cast<std::uint32_t>(image_info.image_base), image_info.size_of_image};
    auto handler = [](const NativeImportGateEvent& event, NativeImportGateResult* output, void* opaque) {
        auto* state = static_cast<Context*>(opaque);
        const auto* stack = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(event.stack_pointer));
        const auto text_at = [](const Context& context, std::uint32_t address, const char* expected) {
            const std::size_t length = std::strlen(expected) + 1;
            return address >= context.base && address - context.base <= context.size &&
                   length <= context.size - (address - context.base) &&
                   std::memcmp(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), expected, length) == 0;
        };
        if (output == nullptr) return false;
        if (!state->module && text_at(*state, stack[1], "kernel32")) {
            state->module = true; output->eax = kKernel32Module; output->stack_bytes_to_pop = 4; return true;
        }
        if (state->module && !state->resolver && stack[1] == kKernel32Module &&
            text_at(*state, stack[2], "GetVersion")) {
            state->resolver = true; state->return_address = stack[0];
            *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(stack[0])) = 0xCC;
            output->eax = 1; output->stack_bytes_to_pop = 8; return true;
        }
        return false;
    };
    NativeInProcessRunResult run;
    *result = {};
    const bool observed = !RunNativePeInProcess(file_bytes, image_info,
        static_cast<std::uint32_t>(image_info.image_base), handler, &context, &run, error) &&
        context.resolver && run.fault.status_code == SIGTRAP &&
        run.fault.instruction_pointer == context.return_address + 1;
    if (!observed) return false;
    result->boundary = OriginalRunBoundary::kFirstResolverObserved;
    result->load_base = runtime::GuestAddress(context.base);
    result->entry_point = runtime::GuestAddress(context.base + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->import_return_address = context.return_address;
    result->status_code = run.fault.status_code;
    result->import_first_argument_text_observed = true;
    result->import_first_argument_text = "GetVersion";
    error->clear(); return true;
#endif
}

bool RunOriginalInProcessGetVersionCall(const std::filesystem::path& executable_path,
                                        const exe::PeImageInfo& image_info,
                                        OriginalRunResult* result,
                                        std::string* error)
{
#if !defined(__i386__)
    static_cast<void>(executable_path);
    static_cast<void>(image_info);
    static_cast<void>(result);
    if (error != nullptr)
    {
        *error = "Linux in-process GetVersion diagnostic requires an i386 host";
    }
    return false;
#else
    if (result == nullptr || error == nullptr)
    {
        return false;
    }

    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error))
    {
        return false;
    }

    constexpr std::uint32_t kKernel32Module = 0x7F000001;
    constexpr std::uint32_t kGetVersionDynamicGate = 0xF1000001;
    NativeDynamicThunk get_version_thunk;
    if (!CreateNativeDynamicThunk(kGetVersionDynamicGate, &get_version_thunk, error))
    {
        return false;
    }
    struct DynamicThunkCleanup
    {
        NativeDynamicThunk* thunk;
        ~DynamicThunkCleanup() { ReleaseNativeDynamicThunk(thunk); }
    } thunk_cleanup{&get_version_thunk};

    struct Context
    {
        std::uint32_t base;
        std::uint32_t size;
        std::uint32_t thunk_address;
        std::uint32_t return_address = 0;
        bool module = false;
        bool resolved = false;
        bool called = false;
        bool unhandled_dynamic_request = false;
        bool trace_arm_failed = false;
        NativeInstructionTrace trace;
        std::string trace_error;
    } context{static_cast<std::uint32_t>(image_info.image_base),
              image_info.size_of_image,
              get_version_thunk.address,
              0,
              false,
              false,
              false,
              false,
              false,
              {},
              {}};
    auto handler = [](const NativeImportGateEvent& event,
                      NativeImportGateResult* output,
                      void* opaque) {
        auto* state = static_cast<Context*>(opaque);
        const auto* stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(event.stack_pointer));
        const auto text_at = [](const Context& context,
                                std::uint32_t address,
                                const char* expected) {
            const std::size_t length = std::strlen(expected) + 1;
            return address >= context.base && address - context.base <= context.size &&
                   length <= context.size - (address - context.base) &&
                   std::memcmp(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
                               expected,
                               length) == 0;
        };
        if (output == nullptr)
        {
            return false;
        }
        if (!state->module && text_at(*state, stack[1], "kernel32"))
        {
            state->module = true;
            output->eax = kKernel32Module;
            output->stack_bytes_to_pop = 4;
            return true;
        }
        if (state->module && !state->resolved && stack[1] == kKernel32Module &&
            text_at(*state, stack[2], "GetVersion"))
        {
            state->resolved = true;
            if (!ArmNativeInstructionTrace(&state->trace,
                                           stack[0],
                                           state->base,
                                           state->size,
                                           &state->trace_error))
            {
                state->trace_arm_failed = true;
                return false;
            }
            output->eax = state->thunk_address;
            output->stack_bytes_to_pop = 8;
            return true;
        }
        if (state->resolved && !state->called && event.gate_address == kGetVersionDynamicGate)
        {
            state->called = true;
            state->return_address = stack[0];
            *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(stack[0])) = 0xCC;
            output->eax = 0;
            output->stack_bytes_to_pop = 0;
            return true;
        }
        if (state->resolved && !state->unhandled_dynamic_request &&
            stack[1] == kKernel32Module && text_at(*state, stack[2], "CreateFileA"))
        {
            state->unhandled_dynamic_request = true;
        }
        return false;
    };

    NativeInProcessRunResult run;
    *result = {};
    const bool interrupted = !RunNativePeInProcess(file_bytes,
                                                    image_info,
                                                    static_cast<std::uint32_t>(image_info.image_base),
                                                    handler,
                                                    &context,
                                                    &run,
                                                    error);
    FinalizeNativeInstructionTrace(&context.trace);
    if (context.trace_arm_failed)
    {
        *error = context.trace_error;
        return false;
    }
    if (context.called && run.fault.status_code == SIGTRAP &&
        run.fault.instruction_pointer == context.return_address + 1)
    {
        result->boundary = OriginalRunBoundary::kGetVersionCalled;
        result->import_return_address = context.return_address;
    }
    else if (interrupted && context.resolved && !context.called && run.fault.status_code != 0)
    {
        result->boundary = OriginalRunBoundary::kGetVersionCallNotReached;
    }
    else
    {
        *error = "GetVersion continuation did not reach a confirmed diagnostic boundary";
        return false;
    }
    result->load_base = runtime::GuestAddress(context.base);
    result->entry_point = runtime::GuestAddress(context.base + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->status_code = run.fault.status_code;
    result->import_first_argument_text_observed = true;
    result->import_first_argument_text = "GetVersion";
    result->unhandled_dynamic_request_observed = context.unhandled_dynamic_request;
    if (context.unhandled_dynamic_request)
    {
        result->unhandled_dynamic_request = "CreateFileA";
    }
    CopyFaultObservation(run, result);
    CopyInstructionTrace(context.trace, result);
    error->clear();
    return true;
#endif
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
