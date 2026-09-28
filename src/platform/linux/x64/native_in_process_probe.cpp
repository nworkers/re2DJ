#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/mman.h>

#include "../native_dynamic_thunk.h"
#include "../native_guest_module_set.h"
#include "../native_in_process_runner.h"
#include "../native_instruction_trace.h"
#include "../native_thread_probe.h"
#include "native_compat_mode.h"
#include "re2dj/hle/modules/kernel32_module.h"
#include "../../native_probe_fixture.h"

using namespace re2dj::platform::native_probe;

namespace
{

namespace linux_platform = re2dj::platform::linux;

struct HandlerContext
{
    std::uint32_t calls = 0;
};

bool CompleteSyntheticImport(const linux_platform::NativeImportGateEvent& event,
                             linux_platform::NativeImportGateResult* result,
                             void* context)
{
    if (result == nullptr || context == nullptr)
    {
        return false;
    }
    std::uint32_t argument = 0;
    std::memcpy(&argument,
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(event.stack_pointer + 4)),
                sizeof(argument));
    HandlerContext* handler = static_cast<HandlerContext*>(context);
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

bool ReadInfo(const std::vector<std::uint8_t>& image,
              re2dj::exe::PeImageInfo* info,
              std::string* error)
{
    return re2dj::exe::ReadPeImageInfo(image.data(), image.size(), info, error);
}

// Runs the synthetic PE32 to completion: relocation to kRequestedBase, the TLS
// callback's FS and TEB checks, a named and an ordinal import, exit code 51.
bool RunSyntheticToExit(const char* label)
{
    const std::vector<std::uint8_t> image = MakeSyntheticPe32();
    re2dj::exe::PeImageInfo info;
    std::string error;
    linux_platform::NativeInProcessRunResult result;
    HandlerContext handler;
    const bool completed = ReadInfo(image, &info, &error) &&
                           linux_platform::RunNativePeInProcess(image,
                                                                info,
                                                                kRequestedBase,
                                                                &CompleteSyntheticImport,
                                                                &handler,
                                                                &result,
                                                                &error) &&
                           handler.calls == 2 && result.exit_code == 51 &&
                           result.fault.status_code == 0;
    if (!completed)
    {
        std::fprintf(stderr,
                     "linux-x64-in-process-probe %s: %s calls=%u exit=%u signal=%u eip=0x%08x\n",
                     label,
                     error.c_str(),
                     handler.calls,
                     result.exit_code,
                     result.fault.status_code,
                     result.fault.instruction_pointer);
    }
    return completed;
}

// Ends the guest process from its first import, as ExitProcess(7) would.
bool ExitFromFirstImport(const linux_platform::NativeImportGateEvent&,
                         linux_platform::NativeImportGateResult* result,
                         void* context)
{
    ++static_cast<HandlerContext*>(context)->calls;
    result->exit_process = true;
    result->exit_code = 7;
    return true;
}

// The guest never returns from the exiting import: the run completes without
// a fault, reporting exit code 7 after exactly one import.
bool RunGuestProcessExit()
{
    const std::vector<std::uint8_t> image = MakeSyntheticPe32();
    re2dj::exe::PeImageInfo info;
    std::string error;
    linux_platform::NativeInProcessRunResult result;
    HandlerContext handler;
    const bool completed = ReadInfo(image, &info, &error) &&
                           linux_platform::RunNativePeInProcess(image,
                                                                info,
                                                                kRequestedBase,
                                                                &ExitFromFirstImport,
                                                                &handler,
                                                                &result,
                                                                &error) &&
                           result.process_exited && result.exit_code == 7 &&
                           handler.calls == 1 && result.fault.status_code == 0;
    if (!completed)
    {
        std::fprintf(stderr,
                     "linux-x64-exit-probe: %s exited=%u exit=%u calls=%u signal=%u\n",
                     error.c_str(),
                     result.process_exited ? 1U : 0U,
                     result.exit_code,
                     handler.calls,
                     result.fault.status_code);
        return false;
    }
    std::printf("linux-x64-exit-probe: ExitProcess(%u) after %u import\n",
                result.exit_code,
                handler.calls);
    return true;
}

struct TraceHandlerContext
{
    linux_platform::NativeInstructionTrace trace;
    std::string error;
    std::uint32_t image_base = 0;
    std::uint32_t image_size = 0;
    bool handled = false;
};

// Arms the trace at the first import's return address and completes it.
bool CompleteTraceImport(const linux_platform::NativeImportGateEvent& event,
                         linux_platform::NativeImportGateResult* result,
                         void* context)
{
    auto* trace = static_cast<TraceHandlerContext*>(context);
    if (result == nullptr || trace == nullptr || trace->handled ||
        !linux_platform::ArmNativeInstructionTrace(&trace->trace,
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

// Single-steps from the first import's return into a ud2 placed at entry+8,
// as the i386 probe does: one frame, then SIGILL at the same address.
bool RunInstructionTrace()
{
    std::vector<std::uint8_t> image = MakeSyntheticPe32();
    image[0x408] = 0x0F;
    image[0x409] = 0x0B;
    re2dj::exe::PeImageInfo info;
    std::string error;
    if (!ReadInfo(image, &info, &error))
    {
        std::fprintf(stderr, "linux-x64-trace-probe: %s\n", error.c_str());
        return false;
    }
    TraceHandlerContext trace{{}, {}, kRequestedBase, info.size_of_image};
    linux_platform::NativeInProcessRunResult result;
    const bool faulted = !linux_platform::RunNativePeInProcess(image,
                                                               info,
                                                               kRequestedBase,
                                                               &CompleteTraceImport,
                                                               &trace,
                                                               &result,
                                                               &error);
    linux_platform::FinalizeNativeInstructionTrace(&trace.trace);
    const std::uint32_t expected = kRequestedBase + kEntryRva + 8;
    const bool completed = faulted && trace.handled && trace.trace.armed &&
                           trace.trace.started && !trace.trace.limit_reached &&
                           trace.trace.frame_count == 1 &&
                           trace.trace.frames[0].instruction_pointer == expected &&
                           result.fault.status_code == SIGILL &&
                           result.fault.instruction_pointer == expected;
    if (!completed)
    {
        std::fprintf(stderr,
                     "linux-x64-trace-probe: %s signal=%u eip=0x%08x armed=%u started=%u "
                     "limit=%u frames=%u\n",
                     trace.error.empty() ? error.c_str() : trace.error.c_str(),
                     result.fault.status_code,
                     result.fault.instruction_pointer,
                     trace.trace.armed ? 1U : 0U,
                     trace.trace.started ? 1U : 0U,
                     trace.trace.limit_reached ? 1U : 0U,
                     trace.trace.frame_count);
        return false;
    }
    std::printf("linux-x64-trace-probe: frames=%u first=0x%08x signal=SIGILL\n",
                trace.trace.frame_count,
                trace.trace.frames[0].instruction_pointer);
    return true;
}

bool DispatchFacade(const linux_platform::NativeImportGateEvent& event,
                    linux_platform::NativeImportGateResult* result,
                    void* context)
{
    const auto* modules = static_cast<const linux_platform::NativeGuestModuleSet*>(context);
    std::string error;
    return modules->Dispatch(event, result, &error);
}

// Maps the kernel32 facade, then has 32-bit guest code call its GetVersion
// export thunk directly and through a dynamic thunk bound to the same gate.
bool RunFacadeCalls()
{
    linux_platform::NativeGuestModuleSet modules;
    re2dj::runtime::ImportGateTable gates(re2dj::runtime::GuestAddress(0xF1000000U), 16);
    std::string error;
    if (!modules.Add(re2dj::hle::modules::MakeKernel32ModuleDescriptor(),
                     &gates,
                     linux_platform::NativeImportGateBridgeAddress(),
                     linux_platform::NativeImportGateCleanupAddress(),
                     linux_platform::kDefaultNativeGuestModuleBase,
                     &error))
    {
        std::fprintf(stderr, "linux-x64-facade-probe: %s\n", error.c_str());
        return false;
    }
    const auto* module = modules.registry().FindModule("kernel32");
    const auto* get_version =
        module == nullptr ? nullptr : modules.registry().FindExport(module->base, "GetVersion");
    const re2dj::runtime::ImportGate* gate = modules.FindGate("kernel32.dll", "GetVersion");
    if (get_version == nullptr || gate == nullptr)
    {
        std::fprintf(stderr, "linux-x64-facade-probe: GetVersion is not registered\n");
        return false;
    }
    const std::uint32_t thunk = get_version->thunk_address.value();
    const bool thunk_in_image = thunk >= module->base.value() &&
                                thunk - module->base.value() < module->image_size;

    linux_platform::NativeDynamicThunk dynamic;
    linux_platform::NativeLowMemory code;
    if (!linux_platform::CreateNativeDynamicThunk(gate->address.value(), &dynamic, &error) ||
        !linux_platform::MapNativeLowMemory(4096, PROT_READ | PROT_WRITE, &code, &error))
    {
        linux_platform::ReleaseNativeDynamicThunk(&dynamic);
        std::fprintf(stderr, "linux-x64-facade-probe: %s\n", error.c_str());
        return false;
    }
    // mov eax, target; call eax; ret — once for the facade thunk at offset 0
    // and once for the dynamic thunk at offset 16.
    auto* bytes = static_cast<std::uint8_t*>(code.memory);
    const std::uint32_t targets[2] = {thunk, dynamic.address};
    for (std::uint32_t index = 0; index < 2; ++index)
    {
        std::uint8_t* stub = bytes + index * 16;
        stub[0] = 0xB8;
        std::memcpy(stub + 1, &targets[index], sizeof(targets[index]));
        stub[5] = 0xFF;
        stub[6] = 0xD0;
        stub[7] = 0xC3;
    }
    bool completed = mprotect(code.memory, code.size, PROT_READ | PROT_EXEC) == 0;

    linux_platform::NativeCompatModeRuntime runtime;
    completed = completed &&
                runtime.Initialize(kRequestedBase, linux_platform::NativeCompatModeOptions{}, &error);
    std::uint32_t results[2] = {};
    for (std::uint32_t index = 0; completed && index < 2; ++index)
    {
        linux_platform::NativeCompatModeCall call;
        call.entry = code.address + index * 16;
        call.handler = &DispatchFacade;
        call.handler_context = &modules;
        linux_platform::NativeCompatModeRunResult run;
        linux_platform::NativeGuestFault fault;
        completed = runtime.Run(call, &run, &fault, &error);
        results[index] = run.eax;
    }
    linux_platform::ReleaseNativeLowMemory(&code);
    linux_platform::ReleaseNativeDynamicThunk(&dynamic);

    completed = completed && thunk_in_image &&
                results[0] == re2dj::hle::modules::kKernel32GuestVersion &&
                results[1] == re2dj::hle::modules::kKernel32GuestVersion;
    if (!completed)
    {
        std::fprintf(stderr,
                     "linux-x64-facade-probe: %s thunk=0x%08x in_image=%u "
                     "facade=0x%08x dynamic=0x%08x\n",
                     error.c_str(), thunk, thunk_in_image ? 1U : 0U, results[0], results[1]);
        return false;
    }
    std::printf("linux-x64-facade-probe: kernel32=0x%08x GetVersion=0x%08x -> 0x%08x "
                "(facade and dynamic thunk)\n",
                module->base.value(), thunk, results[0]);
    return true;
}

}  // namespace

int main()
{
    if (!RunSyntheticToExit("first run"))
    {
        return 1;
    }

    // Replace the second import call with ud2 so the entry faults after its
    // first import.
    std::vector<std::uint8_t> fault_image = MakeSyntheticPe32();
    fault_image[0x408] = 0x0F;
    fault_image[0x409] = 0x0B;
    re2dj::exe::PeImageInfo info;
    std::string error;
    linux_platform::NativeInProcessRunResult result;
    HandlerContext handler;
    const bool faulted = ReadInfo(fault_image, &info, &error) &&
                         !linux_platform::RunNativePeInProcess(fault_image,
                                                               info,
                                                               kRequestedBase,
                                                               &CompleteSyntheticImport,
                                                               &handler,
                                                               &result,
                                                               &error);
    const bool fault_complete = faulted && error.empty() && handler.calls == 1 &&
                                result.fault.status_code == SIGILL &&
                                result.fault.instruction_pointer ==
                                    kRequestedBase + kEntryRva + 8 &&
                                result.fault.eax == 42 &&
                                result.fault_observation.instruction_window_observed &&
                                result.fault_observation.fs_base != 0 &&
                                result.fault_observation.seh_frame_address == 0xFFFFFFFFU &&
                                !result.fault_observation.seh_frame_observed;
    if (!fault_complete)
    {
        std::fprintf(stderr,
                     "linux-x64-in-process-probe fault: %s calls=%u signal=%u eip=0x%08x "
                     "eax=0x%08x fs=0x%08x seh=0x%08x\n",
                     error.c_str(),
                     handler.calls,
                     result.fault.status_code,
                     result.fault.instruction_pointer,
                     result.fault.eax,
                     result.fault_observation.fs_base,
                     result.fault_observation.seh_frame_address);
        return 2;
    }

    if (!RunSyntheticToExit("second run"))
    {
        return 3;
    }

    if (!RunFacadeCalls())
    {
        return 4;
    }

    if (!RunInstructionTrace())
    {
        return 5;
    }

    if (!RunGuestProcessExit() || !RunSyntheticToExit("run after exit"))
    {
        return 6;
    }

    if (!linux_platform::RunNativeGuestThreadProbe("x64") ||
        !linux_platform::RunNativeGuestThreadFaultProbe("x64") ||
        !RunSyntheticToExit("run after threads"))
    {
        return 7;
    }

    std::printf("linux-x64-in-process-probe: imports=2 exit=51 fault=SIGILL@0x%08x "
                "fs=0x%08x rerun=ok\n",
                result.fault.instruction_pointer,
                result.fault_observation.fs_base);
    return 0;
}
