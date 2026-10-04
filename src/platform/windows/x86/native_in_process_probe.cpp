#include <array>
#include <cstddef>
#include <initializer_list>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define NOMINMAX
#include <windows.h>

#include "../../native/native_dynamic_thunk.h"
#include "../../native/native_guest_seh.h"
#include "../../native/native_host_services.h"
#include "../../native/native_low_memory.h"
#include "../../native/native_process_bootstrap.h"
#include "../../native/native_in_process_runner.h"
#include "../../native/native_instruction_trace.h"
#include "../../native/native_thread_probe.h"
#include "../../native/native_probe_fixture.h"

using namespace re2dj::platform::native_probe;

namespace
{

struct HandlerContext
{
    std::uint32_t calls = 0;
};

bool CompleteSyntheticImport(const re2dj::platform::native::NativeImportGateEvent& event,
                             re2dj::platform::native::NativeImportGateResult* result,
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
    re2dj::platform::native::NativeInstructionTrace trace;
    std::string error;
    std::uint32_t image_base = 0;
    std::uint32_t image_size = 0;
    bool handled = false;
};

bool CompleteTraceImport(const re2dj::platform::native::NativeImportGateEvent& event,
                         re2dj::platform::native::NativeImportGateResult* result,
                         void* context)
{
    auto* trace = static_cast<TraceHandlerContext*>(context);
    if (result == nullptr || trace == nullptr || trace->handled ||
        !re2dj::platform::native::ArmNativeInstructionTrace(&trace->trace,
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

bool CompleteDynamicThunk(const re2dj::platform::native::NativeImportGateEvent& event,
                          re2dj::platform::native::NativeImportGateResult* result,
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
    const re2dj::platform::native::NativeImportGateEvent& event,
    re2dj::platform::native::NativeImportGateResult* result,
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

bool CompleteFirstKernel32Import(const re2dj::platform::native::NativeImportGateEvent& event,
                                 re2dj::platform::native::NativeImportGateResult* result,
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
bool ExitFromFirstImport(const re2dj::platform::native::NativeImportGateEvent&,
                         re2dj::platform::native::NativeImportGateResult* result,
                         void* context)
{
    ++static_cast<HandlerContext*>(context)->calls;
    result->exit_process = true;
    result->exit_code = 7;
    return true;
}

// Guest code that installs an SEH frame and faults twice, a ud2 and a read
// through a null pointer, each two bytes long:
//   entry:   push handler; push fs:[0]; mov fs:[0], esp
//            ud2; xor ecx, ecx; mov eax, [ecx]
//            mov eax, [marker]; pop fs:[0]; add esp, 4; ret
//   handler: mov eax, [esp + 12]; add [eax + eip], 2
//            inc [marker]; mov eax, [decline]; ret
// The handler answers [decline]: 0 continues, 1 searches on.
struct GuestSehCode
{
    re2dj::platform::native::NativeLowMemory code;
    re2dj::platform::native::NativeLowMemory data;
    std::uint32_t entry = 0;

    ~GuestSehCode()
    {
        re2dj::platform::native::ReleaseNativeLowMemory(&code);
        re2dj::platform::native::ReleaseNativeLowMemory(&data);
    }

    bool Build(std::string* error)
    {
        using re2dj::platform::native::HostProtection;
        if (!re2dj::platform::native::MapNativeLowMemory(4096, HostProtection::kReadWrite, &code, error) ||
            !re2dj::platform::native::MapNativeLowMemory(4096, HostProtection::kReadWrite, &data, error))
        {
            return false;
        }
        const std::uint32_t marker = data.address;
        const std::uint32_t decline = data.address + 4;
        const std::uint32_t handler = code.address + 64;
        const std::uint32_t eip_offset = static_cast<std::uint32_t>(offsetof(re2dj::platform::native::Win32Context32, eip));
        std::vector<std::uint8_t> bytes;
        const auto u8 = [&](std::initializer_list<std::uint8_t> values) { bytes.insert(bytes.end(), values); };
        const auto u32 = [&](std::uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        };
        u8({0x68});
        u32(handler);
        u8({0x64, 0xFF, 0x35, 0x00, 0x00, 0x00, 0x00});
        u8({0x64, 0x89, 0x25, 0x00, 0x00, 0x00, 0x00});
        u8({0x0F, 0x0B});
        u8({0x31, 0xC9});
        u8({0x8B, 0x01});
        u8({0xA1});
        u32(marker);
        u8({0x64, 0x8F, 0x05, 0x00, 0x00, 0x00, 0x00});
        u8({0x83, 0xC4, 0x04});
        u8({0xC3});
        bytes.resize(64, 0xCC);
        u8({0x8B, 0x44, 0x24, 0x0C});
        u8({0x83, 0x80});
        u32(eip_offset);
        u8({0x02});
        u8({0xFF, 0x05});
        u32(marker);
        u8({0xA1});
        u32(decline);
        u8({0xC3});
        std::memcpy(code.memory, bytes.data(), bytes.size());
        entry = code.address;
        return re2dj::platform::native::HostProtect(code.memory, code.size, HostProtection::kReadExecute);
    }

    void SetDecline(std::uint32_t value)
    {
        std::memcpy(static_cast<std::uint8_t*>(data.memory) + 4, &value, sizeof(value));
        const std::uint32_t zero = 0;
        std::memcpy(data.memory, &zero, sizeof(zero));
    }
};

bool RunGuestSehProbe()
{
    GuestSehCode code;
    std::string error;
    if (!code.Build(&error))
    {
        std::fprintf(stderr, "windows-guest-seh-probe: %s\n", error.c_str());
        return false;
    }
    // Both faults handled: the entry returns the marker, 2.
    std::uint32_t result = 0;
    re2dj::platform::native::NativeGuestFault fault;
    std::uint32_t resumed = 0;
    {
        re2dj::platform::native::NativeProcessBootstrap bootstrap;
        code.SetDecline(0);
        const bool ran = bootstrap.Initialize(kRequestedBase, &error) &&
                         bootstrap.RunEntry(code.entry, &result, &fault, &error);
        resumed = bootstrap.SehDispatchCount();
        if (!ran || result != 2 || resumed != 2 || fault.status_code != 0)
        {
            std::fprintf(stderr, "windows-guest-seh-probe: %s ran=%u result=%u resumed=%u status=0x%08x eip=0x%08x\n",
                         error.c_str(), ran ? 1U : 0U, result, resumed, fault.status_code, fault.instruction_pointer);
            return false;
        }
    }
    // The handler declines: the run ends with the ud2 itself.
    {
        re2dj::platform::native::NativeProcessBootstrap bootstrap;
        code.SetDecline(1);
        fault = {};
        const bool ran = bootstrap.Initialize(kRequestedBase, &error) &&
                         bootstrap.RunEntry(code.entry, &result, &fault, &error);
        const std::uint32_t ud2 = code.entry + 19;
        if (ran || fault.kind != re2dj::platform::native::NativeFaultKind::kIllegalInstruction ||
            fault.instruction_pointer != ud2)
        {
            std::fprintf(stderr, "windows-guest-seh-decline-probe: %s ran=%u kind=%u status=0x%08x eip=0x%08x/0x%08x\n",
                         error.c_str(), ran ? 1U : 0U, static_cast<unsigned>(fault.kind), fault.status_code,
                         fault.instruction_pointer, ud2);
            return false;
        }
    }
    std::printf("windows-guest-seh-probe: resumed=%u, declined ended at the ud2\n", resumed);
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
        re2dj::platform::native::NativeInProcessRunResult original_result;
        FirstImportContext first;
        const bool loaded = ReadFile(argv[1], &original) &&
                            ReadInfo(original, &original_info, &original_error);
        const bool stopped = loaded &&
                             !re2dj::platform::native::RunNativePeInProcess(
                                 original,
                                 original_info,
                                 static_cast<std::uint32_t>(original_info.image_base),
                                 &CompleteFirstKernel32Import,
                                 &first,
                                 &original_result,
                                 &original_error) &&
                             first.matched && original_result.fault.kind == re2dj::platform::native::NativeFaultKind::kBreakpoint &&
                             original_result.fault.instruction_pointer == first.return_address + 1;
        if (!stopped)
        {
            std::fprintf(stderr, "windows-first-import-probe: %s status=%u eip=0x%08x\n",
                         original_error.c_str(), original_result.fault.status_code,
                         original_result.fault.instruction_pointer);
            return 3;
        }
        std::printf("windows-first-import-probe: return=0x%08x eip=0x%08x\n",
                    first.return_address, original_result.fault.instruction_pointer);
        return 0;
    }
    if (argc != 1)
    {
        std::fprintf(stderr, "usage: re2dj_windows_native_in_process_probe [PE32-path]\n");
        return 4;
    }
    const DWORD host_thread_id = GetCurrentThreadId();
    const DWORD host_tls = TlsAlloc();
    TlsSetValue(host_tls, const_cast<DWORD*>(&host_thread_id));

    std::vector<std::uint8_t> image = MakeSyntheticPe32();
    re2dj::exe::PeImageInfo info;
    std::string error;
    re2dj::platform::native::NativeInProcessRunResult result;
    HandlerContext handler;
    const bool normal = ReadInfo(image, &info, &error) &&
                        re2dj::platform::native::RunNativePeInProcess(
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
                     "windows-native-in-process-probe: %s calls=%u exit=%u fault=%u\n",
                     error.c_str(),
                     handler.calls,
                     result.exit_code,
                     result.fault.status_code);
        return 1;
    }

    constexpr std::uint32_t kDynamicThunkGate = 0xF1000002;
    re2dj::platform::native::NativeDynamicThunk dynamic_thunk;
    DynamicThunkContext dynamic_context{kDynamicThunkGate};
    error.clear();
    const bool dynamic_ready =
        re2dj::platform::native::ConfigureNativeImportGateHandler(&CompleteDynamicThunk,
                                                                   &dynamic_context) &&
        re2dj::platform::native::CreateNativeDynamicThunk(kDynamicThunkGate,
                                                          &dynamic_thunk,
                                                          &error);
    if (dynamic_ready)
    {
        void* thunk = dynamic_thunk.memory;
        __asm
        {
            call thunk
        }
    }
    const bool dynamic_complete = dynamic_ready && dynamic_context.called;
    re2dj::platform::native::ReleaseNativeDynamicThunk(&dynamic_thunk);
    re2dj::platform::native::ClearNativeImportGateHandler();
    if (!dynamic_complete)
    {
        std::fprintf(stderr, "windows-native-dynamic-thunk-probe: %s called=%u\n",
                     error.c_str(), dynamic_context.called ? 1U : 0U);
        return 5;
    }

    constexpr std::uint32_t kDynamicStdcallGate = 0xF1000003;
    constexpr std::array<std::uint32_t, 7> kDynamicStdcallArguments = {
        0x11111111, 0x22222222, 0x33333333, 0x44444444,
        0x55555555, 0x66666666, 0x77777777};
    re2dj::platform::native::NativeDynamicThunk dynamic_stdcall_thunk;
    DynamicStdcallContext dynamic_stdcall_context{kDynamicStdcallGate};
    error.clear();
    const bool dynamic_stdcall_ready =
        re2dj::platform::native::ConfigureNativeImportGateHandler(
            &CompleteDynamicStdcallThunk, &dynamic_stdcall_context) &&
        re2dj::platform::native::CreateNativeDynamicThunk(kDynamicStdcallGate,
                                                          &dynamic_stdcall_thunk,
                                                          &error);
    using DynamicStdcallFunction = std::uint32_t(__stdcall*)(
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
        __asm mov stack_before, esp
        dynamic_stdcall_result =
            reinterpret_cast<DynamicStdcallFunction>(dynamic_stdcall_thunk.memory)(
                kDynamicStdcallArguments[0],
                kDynamicStdcallArguments[1],
                kDynamicStdcallArguments[2],
                kDynamicStdcallArguments[3],
                kDynamicStdcallArguments[4],
                kDynamicStdcallArguments[5],
                kDynamicStdcallArguments[6]);
        __asm mov stack_after, esp
    }
    const bool dynamic_stdcall_complete =
        dynamic_stdcall_ready && dynamic_stdcall_context.called &&
        dynamic_stdcall_context.arguments == kDynamicStdcallArguments &&
        dynamic_stdcall_result == 0xFFFFFFFF && stack_before == stack_after;
    re2dj::platform::native::ReleaseNativeDynamicThunk(&dynamic_stdcall_thunk);
    re2dj::platform::native::ClearNativeImportGateHandler();
    if (!dynamic_stdcall_complete)
    {
        std::fprintf(stderr,
                     "windows-native-dynamic-stdcall-probe: %s called=%u result=%08x "
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
                             !re2dj::platform::native::RunNativePeInProcess(
                                 trace_image,
                                 info,
                                 kRequestedBase,
                                 &CompleteTraceImport,
                                 &trace_handler,
                                 &result,
                                 &error);
    re2dj::platform::native::FinalizeNativeInstructionTrace(&trace_handler.trace);
    const bool trace_complete = trace_fault && trace_handler.handled &&
                                trace_handler.trace.armed && trace_handler.trace.started &&
                                !trace_handler.trace.limit_reached &&
                                trace_handler.trace.frame_count == 1 &&
                                trace_handler.trace.frames[0].instruction_pointer ==
                                    kRequestedBase + kEntryRva + 8 &&
                                result.fault.kind == re2dj::platform::native::NativeFaultKind::kIllegalInstruction &&
                                result.fault.instruction_pointer ==
                                    kRequestedBase + kEntryRva + 8 &&
                                result.fault_observation.fs_base != 0 &&
                                result.fault_observation.seh_frame_address == 0xFFFFFFFFU &&
                                !result.fault_observation.seh_frame_observed;
    if (!trace_complete)
    {
        std::fprintf(stderr,
                     "windows-native-instruction-trace-probe: %s status=%u eip=%08x "
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
    re2dj::platform::native::NativeInProcessRunResult exit_result;
    HandlerContext exit_handler;
    error.clear();
    const bool exited = ReadInfo(exit_image, &info, &error) &&
                        re2dj::platform::native::RunNativePeInProcess(exit_image,
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
                     "windows-native-exit-probe: %s exited=%u exit=%u calls=%u status=%u\n",
                     error.c_str(),
                     exit_result.process_exited ? 1U : 0U,
                     exit_result.exit_code,
                     exit_handler.calls,
                     exit_result.fault.status_code);
        return 8;
    }

    if (!re2dj::platform::native::RunNativeGuestThreadProbe("windows-x86") ||
        !re2dj::platform::native::RunNativeGuestThreadFaultProbe("windows-x86"))
    {
        return 9;
    }

    // Guest SEH on Windows goes from the vectored handler through the
    // delivery stack (design 448): a handler that skips the faulting
    // instruction resumes the guest, one that declines ends the run with the
    // original fault.
    if (!RunGuestSehProbe())
    {
        return 11;
    }

    // The shadow TEB kept the guest's writes off the host's real TEB
    // (design 448): this thread's ID and a host TLS slot are as they were.
    if (GetCurrentThreadId() != host_thread_id || TlsGetValue(host_tls) != &host_thread_id)
    {
        std::fprintf(stderr, "windows-native-shadow-teb-probe: thread=%lu/%lu tls=%p\n",
                     static_cast<unsigned long>(GetCurrentThreadId()), static_cast<unsigned long>(host_thread_id),
                     TlsGetValue(host_tls));
        return 10;
    }

    std::printf("windows-native-in-process-probe: imports=2 dynamic=2 exit=51 status=%u "
                "process-exit=%u\n",
                result.fault.status_code,
                exit_result.exit_code);
    return 0;
}
