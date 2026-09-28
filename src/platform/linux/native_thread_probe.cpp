#include "native_thread_probe.h"

#include <signal.h>
#include <sys/mman.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../native_probe_fixture.h"
#include "native_guest_threads.h"
#include "native_import_bridge.h"
#include "native_in_process_runner.h"
#include "native_low_memory.h"
#include "native_process_bootstrap.h"

namespace re2dj::platform::linux
{
namespace
{

using native_probe::kRequestedBase;

// The gate the thread's own import thunk pushes; no module declares it.
constexpr std::uint32_t kThreadGate = 0x7EEE0000U;
constexpr std::uint32_t kThreadArgument = 0x55;
constexpr std::uint32_t kThreadImportBonus = 100;
// Code offsets in the low page.
constexpr std::uint32_t kThreadProc = 0;
constexpr std::uint32_t kThunk = 32;
constexpr std::uint32_t kFaultProc = 64;
// How long the main thread waits for the guest thread, in 1 ms steps.
constexpr int kWaitSteps = 2000;

void Put32(std::uint8_t* bytes, std::uint32_t offset, std::uint32_t value)
{
    std::memcpy(bytes + offset, &value, sizeof(value));
}

// The thread's code and the word it stores its TEB in:
//   ThreadProc: push 55h; call thunk; mov edx, fs:[18h]; mov [teb], edx; ret 4
//   thunk:      push kThreadGate; call bridge; pop ecx; add esp, [cleanup]; jmp ecx
//   FaultProc:  ud2
struct ThreadCode
{
    NativeLowMemory code;
    NativeLowMemory data;

    ~ThreadCode()
    {
        ReleaseNativeLowMemory(&code);
        ReleaseNativeLowMemory(&data);
    }

    bool Build(std::string* error)
    {
        if (!MapNativeLowMemory(4096, PROT_READ | PROT_WRITE, &code, error) ||
            !MapNativeLowMemory(4096, PROT_READ | PROT_WRITE, &data, error))
        {
            return false;
        }
        const auto bridge = static_cast<std::uint32_t>(NativeImportGateBridgeAddress());
        const auto cleanup = static_cast<std::uint32_t>(NativeImportGateCleanupAddress());
        auto* bytes = static_cast<std::uint8_t*>(code.memory);
        const std::uint32_t base = code.address;
        bytes[kThreadProc + 0] = 0x6A;
        bytes[kThreadProc + 1] = static_cast<std::uint8_t>(kThreadArgument);
        bytes[kThreadProc + 2] = 0xE8;
        Put32(bytes, kThreadProc + 3, (base + kThunk) - (base + kThreadProc + 7));
        const std::uint8_t read_teb[] = {0x64, 0x8B, 0x15, 0x18, 0x00, 0x00, 0x00};
        std::memcpy(bytes + kThreadProc + 7, read_teb, sizeof(read_teb));
        bytes[kThreadProc + 14] = 0x89;
        bytes[kThreadProc + 15] = 0x15;
        Put32(bytes, kThreadProc + 16, data.address);
        bytes[kThreadProc + 20] = 0xC2;
        bytes[kThreadProc + 21] = 0x04;
        bytes[kThreadProc + 22] = 0x00;
        bytes[kThunk + 0] = 0x68;
        Put32(bytes, kThunk + 1, kThreadGate);
        bytes[kThunk + 5] = 0xE8;
        Put32(bytes, kThunk + 6, bridge - (base + kThunk + 10));
        bytes[kThunk + 10] = 0x59;
        bytes[kThunk + 11] = 0x03;
        bytes[kThunk + 12] = 0x25;
        Put32(bytes, kThunk + 13, cleanup);
        bytes[kThunk + 17] = 0xFF;
        bytes[kThunk + 18] = 0xE1;
        bytes[kFaultProc + 0] = 0x0F;
        bytes[kFaultProc + 1] = 0x0B;
        if (mprotect(code.memory, code.size, PROT_READ | PROT_EXEC) != 0)
        {
            *error = "cannot protect the thread probe code";
            return false;
        }
        return true;
    }

    std::uint32_t StoredTeb() const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, data.memory, sizeof(value));
        return value;
    }
};

struct ThreadProbe
{
    const ThreadCode* code = nullptr;
    std::uint32_t start = 0;
    std::uint32_t main_calls = 0;
    std::uint32_t thread_calls = 0;
    std::uint32_t main_teb = 0;
    std::uint32_t thread_teb = 0;
    std::uint32_t import_teb = 0;
    bool started = false;
    bool finished = false;
    std::uint32_t token = 0;
    std::uint32_t exit_code = 0;
    std::string error;
};

std::uint32_t Argument(const NativeImportGateEvent& event)
{
    std::uint32_t argument = 0;
    std::memcpy(&argument, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(event.stack_pointer + 4)),
                sizeof(argument));
    return argument;
}

void OnThreadExit(void* context, std::uint32_t token, std::uint32_t exit_code)
{
    auto* probe = static_cast<ThreadProbe*>(context);
    probe->finished = true;
    probe->token = token;
    probe->exit_code = exit_code;
}

// The synthetic PE's ProbeGate(41) starts the thread and waits for it, and
// ordinal 7 (42) answers 43; the thread's own gate answers its argument
// plus kThreadImportBonus.
bool HandleThreadProbe(const NativeImportGateEvent& event, NativeImportGateResult* result, void* context)
{
    auto* probe = static_cast<ThreadProbe*>(context);
    const std::uint32_t argument = Argument(event);
    result->stack_bytes_to_pop = 4;
    if (event.gate_address == kThreadGate)
    {
        ++probe->thread_calls;
        const NativeGuestThread* thread = CurrentNativeGuestThread();
        probe->import_teb = thread == nullptr ? 0 : thread->teb;
        result->eax = argument + kThreadImportBonus;
        return true;
    }
    ++probe->main_calls;
    if (probe->main_calls == 1 && argument == 41)
    {
        const NativeGuestThread* main = CurrentNativeGuestThread();
        probe->main_teb = main == nullptr ? 0 : main->teb;
        NativeGuestThreadStart start;
        start.start = probe->start;
        start.parameter = 0;
        start.token = 0x0F08;
        start.on_exit = &OnThreadExit;
        start.exit_context = probe;
        probe->started = StartNativeGuestThread(start, &probe->thread_teb, &probe->error);
        for (int step = 0; probe->started && !probe->finished && step < kWaitSteps; ++step)
        {
            WaitNativeGuestThread(1);
        }
        result->eax = 42;
        return probe->finished;
    }
    if (probe->main_calls == 2 && argument == 42)
    {
        result->eax = 43;
        result->edx = 1;
        return true;
    }
    return false;
}

bool RunProbe(ThreadProbe* probe, std::uint32_t offset, NativeInProcessRunResult* result, std::string* error)
{
    const std::vector<std::uint8_t> image = native_probe::MakeSyntheticPe32();
    exe::PeImageInfo info;
    probe->start = probe->code->code.address + offset;
    return exe::ReadPeImageInfo(image.data(), image.size(), &info, error) &&
           RunNativePeInProcess(image, info, kRequestedBase, &HandleThreadProbe, probe, result, error);
}

}  // namespace

bool RunNativeGuestThreadProbe(const char* width)
{
    ThreadCode code;
    std::string error;
    ThreadProbe probe;
    probe.code = &code;
    NativeInProcessRunResult result;
    const bool completed = code.Build(&error) && RunProbe(&probe, kThreadProc, &result, &error) &&
                           result.exit_code == 51 && result.fault.status_code == 0;
    const bool threaded = completed && probe.started && probe.finished && probe.token == 0x0F08 &&
                          probe.thread_calls == 1 && probe.main_calls == 2 &&
                          probe.exit_code == kThreadArgument + kThreadImportBonus && probe.thread_teb != 0 &&
                          probe.thread_teb != probe.main_teb && code.StoredTeb() == probe.thread_teb &&
                          probe.import_teb == probe.thread_teb;
    if (!threaded)
    {
        std::fprintf(stderr,
                     "linux-%s-thread-probe: %s %s exit=%u signal=%u eip=0x%08x started=%u finished=%u "
                     "calls=%u/%u code=0x%x teb=0x%08x/0x%08x/0x%08x main=0x%08x\n",
                     width, error.c_str(), probe.error.c_str(), result.exit_code, result.fault.status_code,
                     result.fault.instruction_pointer, probe.started ? 1U : 0U, probe.finished ? 1U : 0U,
                     probe.main_calls, probe.thread_calls, probe.exit_code, probe.thread_teb,
                     code.StoredTeb(), probe.import_teb, probe.main_teb);
        return false;
    }
    std::printf("linux-%s-thread-probe: thread teb=0x%08x main teb=0x%08x exit=0x%x\n", width,
                probe.thread_teb, probe.main_teb, probe.exit_code);
    return true;
}

bool RunNativeGuestThreadFaultProbe(const char* width)
{
    ThreadCode code;
    std::string error;
    ThreadProbe probe;
    probe.code = &code;
    NativeInProcessRunResult result;
    const bool built = code.Build(&error);
    const bool faulted = built && !RunProbe(&probe, kFaultProc, &result, &error) && error.empty() &&
                         probe.started && !probe.finished && result.fault.status_code == SIGILL &&
                         result.fault.instruction_pointer == code.code.address + kFaultProc;
    if (!faulted)
    {
        std::fprintf(stderr, "linux-%s-thread-fault-probe: %s %s signal=%u eip=0x%08x started=%u finished=%u\n",
                     width, error.c_str(), probe.error.c_str(), result.fault.status_code,
                     result.fault.instruction_pointer, probe.started ? 1U : 0U, probe.finished ? 1U : 0U);
        return false;
    }
    std::printf("linux-%s-thread-fault-probe: SIGILL@0x%08x ended the run\n", width,
                result.fault.instruction_pointer);
    return true;
}

}  // namespace re2dj::platform::linux
