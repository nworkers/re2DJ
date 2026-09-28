#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <signal.h>

#include "../native_dynamic_thunk.h"
#include "../native_in_process_runner.h"
#include "../native_instruction_trace.h"
#include "../native_thread_probe.h"
#include "../../native_probe_fixture.h"

using namespace re2dj::platform::native_probe;

namespace
{

struct HandlerContext
{
    std::uint32_t calls = 0;
};

bool CompleteSyntheticImport(const re2dj::platform::linux::NativeImportGateEvent& event,
                             re2dj::platform::linux::NativeImportGateResult* result,
                             void* context)
{
    if (result == nullptr || context == nullptr)
    {
        return false;
    }
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(event.stack_pointer));
    HandlerContext* handler = static_cast<HandlerContext*>(context);
    const std::uint32_t argument = stack[1];
    result->stack_bytes_to_pop = sizeof(argument);
    if (handler->calls == 0 && argument == 41)
    {
        result->eax = 42;
        ++handler->calls;
        return true;
    }
    if (handler->calls == 1 && argument == 42)
    {
        result->eax = 43;
        result->edx = 1;
        ++handler->calls;
        return true;
    }
    return false;
}

struct DynamicThunkContext
{
    std::uint32_t gate_address = 0;
    bool called = false;
};

struct DynamicStdcallContext
{
    std::uint32_t gate_address = 0;
    std::array<std::uint32_t, 7> arguments = {};
    bool called = false;
};

struct TraceHandlerContext
{
    re2dj::platform::linux::NativeInstructionTrace trace;
    std::string error;
    std::uint32_t image_base = 0;
    std::uint32_t image_size = 0;
    bool handled = false;
};

bool CompleteTraceImport(const re2dj::platform::linux::NativeImportGateEvent& event,
                         re2dj::platform::linux::NativeImportGateResult* result,
                         void* context)
{
    auto* trace = static_cast<TraceHandlerContext*>(context);
    if (result == nullptr || trace == nullptr || trace->handled ||
        !re2dj::platform::linux::ArmNativeInstructionTrace(&trace->trace,
                                                             event.instruction_pointer,
                                                             trace->image_base,
                                                             trace->image_size,
                                                             &trace->error))
    {
        return false;
    }
    trace->handled = true;
    result->eax = 42;
    result->stack_bytes_to_pop = sizeof(std::uint32_t);
    return true;
}

bool CompleteDynamicThunk(const re2dj::platform::linux::NativeImportGateEvent& event,
                          re2dj::platform::linux::NativeImportGateResult* result,
                          void* context)
{
    auto* dynamic = static_cast<DynamicThunkContext*>(context);
    if (result == nullptr || dynamic == nullptr || event.gate_address != dynamic->gate_address)
    {
        return false;
    }
    dynamic->called = true;
    result->eax = 0x12345678;
    return true;
}

bool CompleteDynamicStdcallThunk(
    const re2dj::platform::linux::NativeImportGateEvent& event,
    re2dj::platform::linux::NativeImportGateResult* result,
    void* context)
{
    auto* dynamic = static_cast<DynamicStdcallContext*>(context);
    if (result == nullptr || dynamic == nullptr || event.gate_address != dynamic->gate_address)
    {
        return false;
    }
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(event.stack_pointer));
    for (std::size_t index = 0; index < dynamic->arguments.size(); ++index)
    {
        dynamic->arguments[index] = stack[index + 1];
    }
    dynamic->called = true;
    result->eax = 0xFFFFFFFF;
    result->stack_bytes_to_pop =
        static_cast<std::uint32_t>(dynamic->arguments.size() * sizeof(std::uint32_t));
    return true;
}

bool ReadInfo(const std::vector<std::uint8_t>& image,
              re2dj::exe::PeImageInfo* info,
              std::string* error)
{
    return re2dj::exe::ReadPeImageInfo(image.data(), image.size(), info, error);
}

struct FirstImportContext
{
    std::uint32_t return_address = 0;
    bool matched = false;
};

bool CompleteFirstKernel32Import(const re2dj::platform::linux::NativeImportGateEvent& event,
                                 re2dj::platform::linux::NativeImportGateResult* result,
                                 void* context)
{
    if (result == nullptr || context == nullptr)
    {
        return false;
    }
    const auto* stack = reinterpret_cast<const std::uint32_t*>(
        static_cast<std::uintptr_t>(event.stack_pointer));
    const char* module_name = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(stack[1]));
    if (std::strcmp(module_name, "kernel32") != 0)
    {
        return false;
    }
    FirstImportContext* first = static_cast<FirstImportContext*>(context);
    first->return_address = stack[0];
    *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(first->return_address)) = 0xCC;
    first->matched = true;
    result->eax = 1;
    result->stack_bytes_to_pop = sizeof(std::uint32_t);
    return true;
}

// Ends the guest process from its first import, as ExitProcess(7) would.
bool ExitFromFirstImport(const re2dj::platform::linux::NativeImportGateEvent&,
                         re2dj::platform::linux::NativeImportGateResult* result,
                         void* context)
{
    ++static_cast<HandlerContext*>(context)->calls;
    result->exit_process = true;
    result->exit_code = 7;
    return true;
}

bool ReadFile(const char* path, std::vector<std::uint8_t>* bytes)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    if (size <= 0)
    {
        return false;
    }
    bytes->resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    return stream.read(reinterpret_cast<char*>(bytes->data()), size).good();
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2)
    {
        std::vector<std::uint8_t> original;
        re2dj::exe::PeImageInfo original_info;
        std::string original_error;
        re2dj::platform::linux::NativeInProcessRunResult original_result;
        FirstImportContext first;
        const bool loaded = ReadFile(argv[1], &original) &&
                            ReadInfo(original, &original_info, &original_error);
        const bool stopped = loaded &&
                             !re2dj::platform::linux::RunNativePeInProcess(
                                 original,
                                 original_info,
                                 static_cast<std::uint32_t>(original_info.image_base),
                                 &CompleteFirstKernel32Import,
                                 &first,
                                 &original_result,
                                 &original_error) &&
                             first.matched && original_result.fault.status_code == SIGTRAP &&
                             original_result.fault.instruction_pointer == first.return_address + 1;
        if (!stopped)
        {
            std::fprintf(stderr, "linux-first-import-probe: %s signal=%u eip=0x%08x\n",
                         original_error.c_str(), original_result.fault.status_code,
                         original_result.fault.instruction_pointer);
            return 3;
        }
        std::printf("linux-first-import-probe: return=0x%08x eip=0x%08x\n",
                    first.return_address, original_result.fault.instruction_pointer);
        return 0;
    }
    if (argc != 1)
    {
        std::fprintf(stderr, "usage: re2dj_linux_native_in_process_probe [PE32-path]\n");
        return 4;
    }
    std::vector<std::uint8_t> image = MakeSyntheticPe32();
    re2dj::exe::PeImageInfo info;
    std::string error;
    re2dj::platform::linux::NativeInProcessRunResult result;
    HandlerContext handler;
    const bool normal = ReadInfo(image, &info, &error) &&
                        re2dj::platform::linux::RunNativePeInProcess(
                            image,
                            info,
                            kRequestedBase,
                            &CompleteSyntheticImport,
                            &handler,
                            &result,
                            &error) &&
                        handler.calls == 2 && result.exit_code == 51 &&
                        result.fault.status_code == 0;
    if (!normal)
    {
        std::fprintf(stderr,
                     "linux-native-in-process-probe: %s calls=%u exit=%u fault=%u\n",
                     error.c_str(),
                     handler.calls,
                     result.exit_code,
                     result.fault.status_code);
        return 1;
    }

    constexpr std::uint32_t kDynamicThunkGate = 0xF1000002;
    re2dj::platform::linux::NativeDynamicThunk dynamic_thunk;
    DynamicThunkContext dynamic_context{kDynamicThunkGate};
    error.clear();
    const bool dynamic_ready =
        re2dj::platform::linux::ConfigureNativeImportGateHandler(&CompleteDynamicThunk,
                                                                   &dynamic_context) &&
        re2dj::platform::linux::CreateNativeDynamicThunk(kDynamicThunkGate,
                                                          &dynamic_thunk,
                                                          &error);
    if (dynamic_ready)
    {
        __asm__ volatile("call *%0" : : "r"(dynamic_thunk.memory) : "eax", "ecx", "edx", "memory");
    }
    const bool dynamic_complete = dynamic_ready && dynamic_context.called;
    re2dj::platform::linux::ReleaseNativeDynamicThunk(&dynamic_thunk);
    re2dj::platform::linux::ClearNativeImportGateHandler();
    if (!dynamic_complete)
    {
        std::fprintf(stderr, "linux-native-dynamic-thunk-probe: %s called=%u\n",
                     error.c_str(), dynamic_context.called ? 1U : 0U);
        return 5;
    }

    constexpr std::uint32_t kDynamicStdcallGate = 0xF1000003;
    constexpr std::array<std::uint32_t, 7> kDynamicStdcallArguments = {
        0x11111111, 0x22222222, 0x33333333, 0x44444444,
        0x55555555, 0x66666666, 0x77777777};
    re2dj::platform::linux::NativeDynamicThunk dynamic_stdcall_thunk;
    DynamicStdcallContext dynamic_stdcall_context{kDynamicStdcallGate};
    error.clear();
    const bool dynamic_stdcall_ready =
        re2dj::platform::linux::ConfigureNativeImportGateHandler(
            &CompleteDynamicStdcallThunk, &dynamic_stdcall_context) &&
        re2dj::platform::linux::CreateNativeDynamicThunk(kDynamicStdcallGate,
                                                          &dynamic_stdcall_thunk,
                                                          &error);
    using DynamicStdcallFunction = std::uint32_t(__attribute__((stdcall)) *)(
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t);
    std::uint32_t stack_before = 0;
    std::uint32_t stack_after = 0;
    std::uint32_t dynamic_stdcall_result = 0;
    if (dynamic_stdcall_ready)
    {
        __asm__ volatile("movl %%esp, %0" : "=r"(stack_before));
        dynamic_stdcall_result =
            reinterpret_cast<DynamicStdcallFunction>(dynamic_stdcall_thunk.memory)(
                kDynamicStdcallArguments[0],
                kDynamicStdcallArguments[1],
                kDynamicStdcallArguments[2],
                kDynamicStdcallArguments[3],
                kDynamicStdcallArguments[4],
                kDynamicStdcallArguments[5],
                kDynamicStdcallArguments[6]);
        __asm__ volatile("movl %%esp, %0" : "=r"(stack_after));
    }
    const bool dynamic_stdcall_complete =
        dynamic_stdcall_ready && dynamic_stdcall_context.called &&
        dynamic_stdcall_context.arguments == kDynamicStdcallArguments &&
        dynamic_stdcall_result == 0xFFFFFFFF && stack_before == stack_after;
    re2dj::platform::linux::ReleaseNativeDynamicThunk(&dynamic_stdcall_thunk);
    re2dj::platform::linux::ClearNativeImportGateHandler();
    if (!dynamic_stdcall_complete)
    {
        std::fprintf(stderr,
                     "linux-native-dynamic-stdcall-probe: %s called=%u result=%08x "
                     "stack=%08x/%08x\n",
                     error.c_str(),
                     dynamic_stdcall_context.called ? 1U : 0U,
                     dynamic_stdcall_result,
                     stack_before,
                     stack_after);
        return 7;
    }

    std::vector<std::uint8_t> trace_image = MakeSyntheticPe32();
    trace_image[0x408] = 0x0F;
    trace_image[0x409] = 0x0B;
    TraceHandlerContext trace_handler{{}, {}, kRequestedBase, info.size_of_image};
    result = {};
    error.clear();
    const bool trace_fault = ReadInfo(trace_image, &info, &error) &&
                             !re2dj::platform::linux::RunNativePeInProcess(
                                 trace_image,
                                 info,
                                 kRequestedBase,
                                 &CompleteTraceImport,
                                 &trace_handler,
                                 &result,
                                 &error);
    re2dj::platform::linux::FinalizeNativeInstructionTrace(&trace_handler.trace);
    const bool trace_complete = trace_fault && trace_handler.handled &&
                                trace_handler.trace.armed && trace_handler.trace.started &&
                                !trace_handler.trace.limit_reached &&
                                trace_handler.trace.frame_count == 1 &&
                                trace_handler.trace.frames[0].instruction_pointer ==
                                    kRequestedBase + kEntryRva + 8 &&
                                result.fault.status_code == SIGILL &&
                                result.fault.instruction_pointer ==
                                    kRequestedBase + kEntryRva + 8 &&
                                result.fault_observation.fs_base != 0 &&
                                result.fault_observation.seh_frame_address == 0xFFFFFFFFU &&
                                !result.fault_observation.seh_frame_observed;
    if (!trace_complete)
    {
        std::fprintf(stderr,
                     "linux-native-instruction-trace-probe: %s signal=%u eip=%08x "
                     "armed=%u started=%u limit=%u frames=%u window=%u stack=%u/%u run=%u\n",
                     trace_handler.error.empty() ? error.c_str() : trace_handler.error.c_str(),
                     result.fault.status_code,
                     result.fault.instruction_pointer,
                     trace_handler.trace.armed ? 1U : 0U,
                     trace_handler.trace.started ? 1U : 0U,
                     trace_handler.trace.limit_reached ? 1U : 0U,
                     trace_handler.trace.frame_count,
                     result.fault_observation.instruction_window_observed ? 1U : 0U,
                     result.fault_observation.stack_words_observed ? 1U : 0U,
                     result.fault_observation.stack_word_count,
                     trace_fault ? 1U : 0U);
        if (result.fault_observation.instruction_window_observed)
        {
            std::fprintf(stderr,
                         "  trace-eip=0x%08x window=0x%08x bytes=%02x%02x\n",
                         trace_handler.trace.frames[0].instruction_pointer,
                         result.fault_observation.instruction_window_address,
                         result.fault_observation.instruction_window[64],
                         result.fault_observation.instruction_window[65]);
        }
        return 6;
    }

    // The guest never returns from an import that ends the process: the run
    // completes without a fault, reporting exit code 7 after one import.
    std::vector<std::uint8_t> exit_image = MakeSyntheticPe32();
    re2dj::platform::linux::NativeInProcessRunResult exit_result;
    HandlerContext exit_handler;
    error.clear();
    const bool exited = ReadInfo(exit_image, &info, &error) &&
                        re2dj::platform::linux::RunNativePeInProcess(exit_image,
                                                                     info,
                                                                     kRequestedBase,
                                                                     &ExitFromFirstImport,
                                                                     &exit_handler,
                                                                     &exit_result,
                                                                     &error) &&
                        exit_result.process_exited && exit_result.exit_code == 7 &&
                        exit_handler.calls == 1 && exit_result.fault.status_code == 0;
    if (!exited)
    {
        std::fprintf(stderr,
                     "linux-native-exit-probe: %s exited=%u exit=%u calls=%u signal=%u\n",
                     error.c_str(),
                     exit_result.process_exited ? 1U : 0U,
                     exit_result.exit_code,
                     exit_handler.calls,
                     exit_result.fault.status_code);
        return 8;
    }

    if (!re2dj::platform::linux::RunNativeGuestThreadProbe("x86") ||
        !re2dj::platform::linux::RunNativeGuestThreadFaultProbe("x86"))
    {
        return 9;
    }

    std::printf("linux-native-in-process-probe: imports=2 dynamic=2 exit=51 signal=%u "
                "process-exit=%u\n",
                result.fault.status_code,
                exit_result.exit_code);
    return 0;
}
