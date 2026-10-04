#define NOMINMAX
#include <windows.h>

#include <immintrin.h>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

// Task 452: can a 64-bit Windows process run 32-bit guest code in
// compatibility mode (CS 0x23) the way the Linux x64 backend does, and what
// does the guest FS (the TEB) become without an LDT? Each question runs in a
// child process of its own, so a crash answers one question without hiding
// the others. Research only; nothing in the product uses this.
//
// MSVC x64 has no inline asm and every transition must sit below 4 GiB
// (compatibility-mode EIP and far-pointer offsets are 32-bit), so the code is
// machine-code bytes copied to a fixed low page. The assembly of each block
// is kept next to its bytes (GNU as, Intel syntax; B = 0x10000000).
namespace
{

constexpr std::uintptr_t kBase = 0x10000000;
constexpr std::uintptr_t kTeb = kBase + 0x1000;
constexpr std::uintptr_t kStackBase = 0x10100000;
constexpr std::size_t kStackSize = 0x100000;

// Data slots shared with the code below.
constexpr std::uintptr_t kHostRsp = kBase + 0x800;
constexpr std::uintptr_t kGuestEsp = kBase + 0x808;
constexpr std::uintptr_t kTarget = kBase + 0x80C;
constexpr std::uintptr_t kResultEax = kBase + 0x810;
constexpr std::uintptr_t kResultEdx = kBase + 0x814;
constexpr std::uintptr_t kFsAtEntry = kBase + 0x818;
constexpr std::uintptr_t kFsInCompat = kBase + 0x81C;
constexpr std::uintptr_t kIterations = kBase + 0x820;
constexpr std::uintptr_t kMismatches = kBase + 0x824;
constexpr std::uintptr_t kExpect = kBase + 0x828;
// The resume stub's state: ESP, EIP (a qword, upper half zero), the seven
// general-purpose registers from EAX to EDI, and an FS base to write first
// (0 for none).
constexpr std::uintptr_t kResumeEsp = kBase + 0x840;
constexpr std::uintptr_t kResumeEip = kBase + 0x848;
constexpr std::uintptr_t kResumeRegisters = kBase + 0x850;
constexpr std::uintptr_t kResumeFsBase = kBase + 0x870;

struct CodeBlock
{
    std::uintptr_t offset;
    std::vector<std::uint8_t> bytes;
};

const CodeBlock kCode[] = {
    // 0x000 enter, void(void) under the Microsoft x64 ABI:
    //   push rbx/rbp/rsi/rdi/r12-r15; sub rsp, 8
    //   mov word ptr [FS_ENTRY], fs; mov [HOST_RSP], rsp
    //   mov ax, 0x2b; mov ds, ax; mov es, ax
    //   mov esp, [GUEST_ESP]; push 0x23; push B+0x200; retfq
    {0x000, {0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x08,
             0x8c, 0x24, 0x25, 0x18, 0x08, 0x00, 0x10, 0x48, 0x89, 0x24, 0x25, 0x00, 0x08, 0x00, 0x10,
             0x66, 0xb8, 0x2b, 0x00, 0x8e, 0xd8, 0x8e, 0xc0, 0x8b, 0x24, 0x25, 0x08, 0x08, 0x00, 0x10,
             0x6a, 0x23, 0x68, 0x00, 0x02, 0x00, 0x10, 0x48, 0xcb}},
    // 0x100 landing in 64-bit mode:
    //   mov rsp, [HOST_RSP]; add rsp, 8; pop r15-r12/rdi/rsi/rbp/rbx; ret
    {0x100, {0x48, 0x8b, 0x24, 0x25, 0x00, 0x08, 0x00, 0x10, 0x48, 0x83, 0xc4, 0x08, 0x41, 0x5f, 0x41, 0x5e,
             0x41, 0x5d, 0x41, 0x5c, 0x5f, 0x5e, 0x5d, 0x5b, 0xc3}},
    // 0x180 uint64(void): mov ax, 0x53; mov fs, ax; rdfsbase rax; ret
    {0x180, {0x66, 0xb8, 0x53, 0x00, 0x8e, 0xe0, 0xf3, 0x48, 0x0f, 0xae, 0xc0, 0xc3}},
    // 0x300 resume, the VEH's 64-bit continuation back into CS 0x23:
    //   mov rax, [RESUME_FSBASE]; test rax, rax; jz 1f; wrfsbase rax
    //   1: mov esp, [RESUME_ESP]; push 0x23; push qword ptr [RESUME_EIP]
    //   mov eax/ecx/edx/ebx/ebp/esi/edi, [RESUME_REGISTERS + 0..0x18]; retfq
    {0x300, {0x48, 0x8b, 0x04, 0x25, 0x70, 0x08, 0x00, 0x10, 0x48, 0x85, 0xc0, 0x74, 0x05,
             0xf3, 0x48, 0x0f, 0xae, 0xd0, 0x8b, 0x24, 0x25, 0x40, 0x08, 0x00, 0x10, 0x6a, 0x23,
             0xff, 0x34, 0x25, 0x48, 0x08, 0x00, 0x10, 0x8b, 0x04, 0x25, 0x50, 0x08, 0x00, 0x10,
             0x8b, 0x0c, 0x25, 0x54, 0x08, 0x00, 0x10, 0x8b, 0x14, 0x25, 0x58, 0x08, 0x00, 0x10,
             0x8b, 0x1c, 0x25, 0x5c, 0x08, 0x00, 0x10, 0x8b, 0x2c, 0x25, 0x60, 0x08, 0x00, 0x10,
             0x8b, 0x34, 0x25, 0x64, 0x08, 0x00, 0x10, 0x8b, 0x3c, 0x25, 0x68, 0x08, 0x00, 0x10, 0x48, 0xcb}},
    // 0x200 compatibility-mode stub:
    //   mov word ptr [FS_COMPAT], fs; mov eax, [TARGET]; call eax
    //   mov [RES_EAX], eax; mov [RES_EDX], edx; jmp far 0x33:B+0x100
    {0x200, {0x8c, 0x25, 0x1c, 0x08, 0x00, 0x10, 0xa1, 0x0c, 0x08, 0x00, 0x10, 0xff, 0xd0,
             0xa3, 0x10, 0x08, 0x00, 0x10, 0x89, 0x15, 0x14, 0x08, 0x00, 0x10,
             0xea, 0x00, 0x01, 0x00, 0x10, 0x33, 0x00}},
    // 0x400 G1: mov eax, 0x12345678; mov edx, 0x9abcdef0; ret
    {0x400, {0xb8, 0x78, 0x56, 0x34, 0x12, 0xba, 0xf0, 0xde, 0xbc, 0x9a, 0xc3}},
    // 0x420 G2: mov eax, fs:[0x18]; mov edx, fs:[0]; ret
    {0x420, {0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0x64, 0x8b, 0x15, 0x00, 0x00, 0x00, 0x00, 0xc3}},
    // 0x440 G3: mov ecx, [ITERATIONS]; mov esi, [EXPECT]; xor edx, edx
    //   1: mov eax, fs:[0x18]; cmp eax, esi; je 2f; inc edx; 2: dec ecx; jnz 1b
    //   mov [MISMATCHES], edx; mov eax, fs:[0x18]; ret
    {0x440, {0x8b, 0x0d, 0x20, 0x08, 0x00, 0x10, 0x8b, 0x35, 0x28, 0x08, 0x00, 0x10, 0x31, 0xd2,
             0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0x39, 0xf0, 0x74, 0x01, 0x42, 0x49, 0x75, 0xf2,
             0x89, 0x15, 0x24, 0x08, 0x00, 0x10, 0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0xc3}},
    // 0x480 G4: mov eax, fs:[0x18]; mov [SCRATCH], eax; int3
    //   mov eax, fs:[0x18]; mov edx, [SCRATCH]; ret
    {0x480, {0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0xa3, 0x30, 0x08, 0x00, 0x10, 0xcc,
             0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0x8b, 0x15, 0x30, 0x08, 0x00, 0x10, 0xc3}},
    // 0x4c0 G5: mov eax, fs:[0x18]; mov [SCRATCH], eax; xor ebx, ebx
    //   mov eax, [ebx]; mov eax, fs:[0x18]; mov edx, [SCRATCH]; ret
    {0x4c0, {0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0xa3, 0x30, 0x08, 0x00, 0x10, 0x31, 0xdb, 0x8b, 0x03,
             0x64, 0xa1, 0x18, 0x00, 0x00, 0x00, 0x8b, 0x15, 0x30, 0x08, 0x00, 0x10, 0xc3}},
};

constexpr std::uint64_t kResumeStub = kBase + 0x300;
constexpr std::uint32_t kG1 = kBase + 0x400;
constexpr std::uint32_t kG2 = kBase + 0x420;
constexpr std::uint32_t kG3 = kBase + 0x440;
constexpr std::uint32_t kG4 = kBase + 0x480;
constexpr std::uint32_t kG5 = kBase + 0x4c0;
// Faulting instructions the VEH steps over, with their lengths.
constexpr std::uint32_t kG2TebRead = kG2;
constexpr std::uint32_t kG2SehRead = kG2 + 6;
constexpr std::uint32_t kG4Breakpoint = kG4 + 11;
constexpr std::uint32_t kG5Load = kG5 + 13;
// A breakpoint in 32-bit code arrives as STATUS_WX86_BREAKPOINT.
constexpr DWORD kWx86Breakpoint = 0x4000001F;
// With the FS base reset to 0, an fs: access lands in the first 64 KiB,
// which Windows never maps.
constexpr std::uint64_t kNullRegionEnd = 0x10000;

template <typename T>
T& Slot(std::uintptr_t address)
{
    return *reinterpret_cast<T*>(address);
}

struct VehRecord
{
    std::atomic<int> count{0};
    std::atomic<int> repairs{0};
    DWORD code = 0;
    std::uint64_t rip = 0;
    std::uint64_t rsp = 0;
    WORD seg_cs = 0;
    WORD seg_fs = 0;
    WORD seg_ds = 0;
};

VehRecord g_veh;
// The FS base the VEH restores when an fs: access faults in the null region;
// 0 leaves such a fault alone.
std::uint64_t g_repair_base = 0;

// Resumes the interrupted compatibility-mode code at eip through the 64-bit
// stub: returning the context with CS 0x23 is not honoured (the kernel
// resumes in CS 0x33), so the VEH continues in 64-bit mode at the stub,
// which far-returns into CS 0x23 with the guest registers.
LONG ResumeThroughStub(CONTEXT* context, std::uint64_t eip, std::uint64_t fs_base)
{
    Slot<std::uint32_t>(kResumeEsp) = static_cast<std::uint32_t>(context->Rsp);
    Slot<std::uint64_t>(kResumeEip) = eip;
    const DWORD64 registers[] = {context->Rax, context->Rcx, context->Rdx, context->Rbx,
                                 context->Rbp, context->Rsi, context->Rdi};
    for (std::size_t index = 0; index < 7; ++index)
    {
        Slot<std::uint32_t>(kResumeRegisters + index * 4) = static_cast<std::uint32_t>(registers[index]);
    }
    Slot<std::uint64_t>(kResumeFsBase) = fs_base;
    context->Rip = kResumeStub;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Whether the instruction at eip carries an FS override among its legacy
// prefixes; a plain null dereference faults in the same region.
bool HasFsPrefix(std::uint64_t eip)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(eip);
    for (int index = 0; index < 15; ++index)
    {
        switch (bytes[index])
        {
        case 0x64:
            return true;
        case 0x26: case 0x2e: case 0x36: case 0x3e: case 0x65: case 0x66: case 0x67: case 0xf0: case 0xf2:
        case 0xf3:
            continue;
        default:
            return false;
        }
    }
    return false;
}

LONG CALLBACK ProbeHandler(EXCEPTION_POINTERS* pointers)
{
    CONTEXT* context = pointers->ContextRecord;
    const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
    const DWORD code = record->ExceptionCode;
    if (context->SegCs != 0x23)
    {
        std::printf("  [veh] unexpected code=0x%08lx rip=0x%llx cs=0x%02x\n", code,
                    static_cast<unsigned long long>(context->Rip), context->SegCs);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const std::uint64_t rip = context->Rip;
    if (code == EXCEPTION_ACCESS_VIOLATION && g_repair_base != 0 && record->NumberParameters >= 2 &&
        record->ExceptionInformation[1] < kNullRegionEnd && HasFsPrefix(rip))
    {
        // The FS base was reset: write the TEB again and retry the access.
        g_veh.repairs.fetch_add(1);
        return ResumeThroughStub(context, rip, g_repair_base);
    }
    g_veh.count.fetch_add(1);
    g_veh.code = code;
    g_veh.rip = rip;
    g_veh.rsp = context->Rsp;
    g_veh.seg_cs = context->SegCs;
    g_veh.seg_fs = context->SegFs;
    g_veh.seg_ds = context->SegDs;
    const bool breakpoint = code == EXCEPTION_BREAKPOINT || code == kWx86Breakpoint;
    if (breakpoint && (rip == kG4Breakpoint || rip == kG4Breakpoint + 1))
    {
        return ResumeThroughStub(context, kG4Breakpoint + 1, 0);
    }
    if (code == EXCEPTION_ACCESS_VIOLATION)
    {
        if (rip == kG2TebRead)
        {
            context->Rax = 0xDEADDEAD;
            return ResumeThroughStub(context, rip + 6, 0);
        }
        if (rip == kG2SehRead)
        {
            context->Rdx = 0xDEADDEAD;
            return ResumeThroughStub(context, rip + 7, 0);
        }
        if (rip == kG5Load)
        {
            return ResumeThroughStub(context, rip + 2, 0);
        }
    }
    std::printf("  [veh] unhandled code=0x%08lx rip=0x%llx cs=0x%02x\n", code, static_cast<unsigned long long>(rip),
                context->SegCs);
    return EXCEPTION_CONTINUE_SEARCH;
}

bool PrepareLowMemory()
{
    void* code = VirtualAlloc(reinterpret_cast<void*>(kBase), 0x2000, MEM_RESERVE | MEM_COMMIT,
                              PAGE_EXECUTE_READWRITE);
    void* stack = VirtualAlloc(reinterpret_cast<void*>(kStackBase), kStackSize, MEM_RESERVE | MEM_COMMIT,
                               PAGE_READWRITE);
    if (code != reinterpret_cast<void*>(kBase) || stack != reinterpret_cast<void*>(kStackBase))
    {
        std::printf("  low memory: cannot allocate at 0x%08llx / 0x%08llx (error %lu)\n",
                    static_cast<unsigned long long>(kBase), static_cast<unsigned long long>(kStackBase),
                    GetLastError());
        return false;
    }
    for (const CodeBlock& block : kCode)
    {
        std::memcpy(reinterpret_cast<void*>(kBase + block.offset), block.bytes.data(), block.bytes.size());
    }
    FlushInstructionCache(GetCurrentProcess(), code, 0x1000);
    // A minimal TEB: an empty SEH chain at +0 and the self pointer at +0x18.
    Slot<std::uint32_t>(kTeb) = 0xFFFFFFFFu;
    Slot<std::uint32_t>(kTeb + 0x18) = static_cast<std::uint32_t>(kTeb);
    return true;
}

struct GuestResult
{
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    WORD fs_at_entry = 0;
    WORD fs_in_compat = 0;
};

// The VEH runs on the guest stack. Host code there that probes its frame
// (__chkstk, as printf does) walks down from the 64-bit TEB's StackLimit, so
// the limits cover the guest stack while it runs unless stack_limits is false.
GuestResult RunGuest(std::uint32_t target, bool stack_limits = true)
{
    NT_TIB* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    void* const saved_base = tib->StackBase;
    void* const saved_limit = tib->StackLimit;
    if (stack_limits)
    {
        tib->StackBase = reinterpret_cast<void*>(kStackBase + kStackSize);
        tib->StackLimit = reinterpret_cast<void*>(kStackBase);
    }
    Slot<std::uint32_t>(kGuestEsp) = static_cast<std::uint32_t>(kStackBase + kStackSize - 16);
    Slot<std::uint32_t>(kTarget) = target;
    Slot<std::uint32_t>(kResultEax) = 0;
    Slot<std::uint32_t>(kResultEdx) = 0;
    reinterpret_cast<void (*)()>(kBase)();
    tib->StackBase = saved_base;
    tib->StackLimit = saved_limit;
    GuestResult result;
    result.eax = Slot<std::uint32_t>(kResultEax);
    result.edx = Slot<std::uint32_t>(kResultEdx);
    result.fs_at_entry = Slot<WORD>(kFsAtEntry);
    result.fs_in_compat = Slot<WORD>(kFsInCompat);
    return result;
}

bool CpuHasFsGsBase()
{
    int registers[4] = {};
    __cpuidex(registers, 7, 0);
    return (registers[1] & 1) != 0;
}

// Kept free of C++ objects for __try.
bool TryReadFsBase(std::uint64_t* base)
{
    __try
    {
        *base = _readfsbase_u64();
        return true;
    }
    __except (GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ? EXCEPTION_EXECUTE_HANDLER
                                                                   : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
}

bool TryWriteFsBase(std::uint64_t base)
{
    __try
    {
        _writefsbase_u64(base);
        return true;
    }
    __except (GetExceptionCode() == EXCEPTION_ILLEGAL_INSTRUCTION ? EXCEPTION_EXECUTE_HANDLER
                                                                   : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
}

void PrintVeh(const char* label)
{
    std::printf("  %s: veh=%d code=0x%08lx rip=0x%08llx rsp=0x%08llx cs=0x%02x ds=0x%02x fs=0x%02x repairs=%d\n",
                label, g_veh.count.load(), g_veh.code, static_cast<unsigned long long>(g_veh.rip),
                static_cast<unsigned long long>(g_veh.rsp), g_veh.seg_cs, g_veh.seg_ds, g_veh.seg_fs,
                g_veh.repairs.load());
}

void ResetVeh()
{
    g_veh.count.store(0);
    g_veh.repairs.store(0);
    g_veh.code = 0;
}

bool FsGsBaseUsable(std::uint64_t* original)
{
    const bool usable = CpuHasFsGsBase() && TryReadFsBase(original);
    if (!usable)
    {
        std::printf("  rdfsbase unavailable; this question needs Q3\n");
    }
    return usable;
}

double Seconds(const LARGE_INTEGER& start, const LARGE_INTEGER& end)
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
}

class LoadThreads
{
public:
    explicit LoadThreads(unsigned count)
    {
        for (unsigned index = 0; index < count; ++index)
        {
            threads_.emplace_back([this] {
                volatile std::uint64_t spin = 0;
                while (!stop_.load(std::memory_order_relaxed))
                {
                    spin = spin + 1;
                }
            });
        }
    }
    ~LoadThreads()
    {
        stop_.store(true);
        for (std::thread& thread : threads_)
        {
            thread.join();
        }
    }
    LoadThreads(const LoadThreads&) = delete;
    LoadThreads& operator=(const LoadThreads&) = delete;

private:
    std::atomic<bool> stop_{false};
    std::vector<std::thread> threads_;
};

// Q1: into CS 0x23 and back.
int CaseTransition()
{
    const GuestResult result = RunGuest(kG1);
    std::printf("  eax=0x%08x edx=0x%08x fs(64-bit entry)=0x%02x fs(compat)=0x%02x\n", result.eax, result.edx,
                result.fs_at_entry, result.fs_in_compat);
    const bool ok = result.eax == 0x12345678u && result.edx == 0x9abcdef0u;
    std::printf("Q1 %s: 32-bit code ran in CS 0x23 and returned to CS 0x33\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

// Q2: the FS a guest sees with nothing changed.
int CaseDefaultFs()
{
    const std::uint64_t teb = reinterpret_cast<std::uint64_t>(NtCurrentTeb());
    std::uint64_t base = 0;
    const bool known = CpuHasFsGsBase() && TryReadFsBase(&base);
    std::printf("  TEB(64)=0x%llx fsbase(64-bit)=%s0x%llx\n", static_cast<unsigned long long>(teb),
                known ? "" : "unknown ", static_cast<unsigned long long>(base));
    ResetVeh();
    const GuestResult result = RunGuest(kG2);
    std::printf("  fs(compat)=0x%02x fs:[0x18]=0x%08x fs:[0]=0x%08x (0xdeaddead = faulted)\n",
                result.fs_in_compat, result.eax, result.edx);
    PrintVeh("faults");
    std::printf("Q2 INFO: default fs in compatibility mode %s\n",
                result.eax != 0xDEADDEADu ? "is readable (see values)" : "does not reach a TEB");
    return 0;
}

// Q3: user-mode rdfsbase/wrfsbase.
int CaseFsGsBase()
{
    const bool cpu = CpuHasFsGsBase();
    std::uint64_t original = 0;
    const bool read = cpu && TryReadFsBase(&original);
    bool written = false;
    std::uint64_t observed = 0;
    if (read)
    {
        written = TryWriteFsBase(kTeb) && TryReadFsBase(&observed) && observed == kTeb;
        TryWriteFsBase(original);
    }
    std::printf("  cpuid.7.ebx.fsgsbase=%d rdfsbase=%s original=0x%llx wrfsbase(0x%llx)->0x%llx\n", cpu ? 1 : 0,
                read ? "ok" : "#UD", static_cast<unsigned long long>(original),
                static_cast<unsigned long long>(kTeb), static_cast<unsigned long long>(observed));
    std::printf("Q3 %s: user-mode rdfsbase/wrfsbase\n", written ? "PASS" : "FAIL");
    return written ? 0 : 1;
}

// Q4: whether a wrfsbase value survives in 64-bit code (a sleep, then pure
// spinning under load) and in a guest reading it for seconds under load; the
// guest repairs a reset base through the VEH and counts the repairs.
int CasePersistence()
{
    std::uint64_t original = 0;
    if (!FsGsBaseUsable(&original))
    {
        return 1;
    }
    int sleep_lost = 0;
    for (int index = 0; index < 20; ++index)
    {
        _writefsbase_u64(kTeb);
        Sleep(25);
        if (_readfsbase_u64() != kTeb)
        {
            ++sleep_lost;
        }
    }
    std::printf("  host: fs base lost across Sleep(25) in %d of 20 tries\n", sleep_lost);

    const unsigned threads = std::thread::hardware_concurrency() * 2;
    LoadThreads* load = new LoadThreads(threads);
    {
        LARGE_INTEGER start{}, now{};
        QueryPerformanceCounter(&start);
        _writefsbase_u64(kTeb);
        double lost_after = -1.0;
        int losses = 0;
        do
        {
            QueryPerformanceCounter(&now);
            if (_readfsbase_u64() != kTeb)
            {
                if (lost_after < 0.0)
                {
                    lost_after = Seconds(start, now);
                }
                ++losses;
                _writefsbase_u64(kTeb);
            }
        } while (Seconds(start, now) < 3.0);
        std::printf("  host: 3s of spinning with %u load threads: %d losses, first after %.4fs\n", threads, losses,
                    lost_after);
    }

    ResetVeh();
    g_repair_base = kTeb;
    _writefsbase_u64(kTeb);
    Slot<std::uint32_t>(kIterations) = 3000000000u;
    Slot<std::uint32_t>(kExpect) = static_cast<std::uint32_t>(kTeb);
    Slot<std::uint32_t>(kMismatches) = 0xFFFFFFFFu;
    LARGE_INTEGER start{}, end{};
    QueryPerformanceCounter(&start);
    const GuestResult result = RunGuest(kG3);
    QueryPerformanceCounter(&end);
    delete load;
    g_repair_base = 0;
    _writefsbase_u64(original);
    const std::uint32_t mismatches = Slot<std::uint32_t>(kMismatches);
    std::printf("  guest: %u reads in %.2fs under load, repairs=%d, other faults=%d, mismatches=%u, "
                "last fs:[0x18]=0x%08x\n",
                Slot<std::uint32_t>(kIterations), Seconds(start, end), g_veh.repairs.load(), g_veh.count.load(),
                mismatches, result.eax);
    const bool held = sleep_lost == 0 && g_veh.repairs.load() == 0;
    const bool repaired = mismatches == 0 && result.eax == kTeb && g_veh.count.load() == 0;
    std::printf("Q4 %s: a wrfsbase TEB %s; with VEH repair the guest %s\n", held ? "PASS" : "FAIL",
                held ? "holds across preemption" : "does NOT hold across preemption",
                repaired ? "always read the TEB" : "still saw wrong values");
    return held ? 0 : 1;
}

// Q5: exceptions raised in compatibility mode through the VEH and back. The
// resume stub writes no FS base, so a reset by the round trip shows up as a
// repair of the following fs: read.
int CaseExceptions()
{
    std::uint64_t original = 0;
    if (!FsGsBaseUsable(&original))
    {
        return 1;
    }
    bool ok = true;
    const struct
    {
        const char* label;
        std::uint32_t target;
    } runs[] = {{"int3", kG4}, {"access violation", kG5}};
    for (const auto& run : runs)
    {
        ResetVeh();
        g_repair_base = kTeb;
        _writefsbase_u64(kTeb);
        const GuestResult result = RunGuest(run.target);
        g_repair_base = 0;
        PrintVeh(run.label);
        std::printf("  %s: fs:[0x18] before=0x%08x after=0x%08x\n", run.label, result.edx, result.eax);
        const bool delivered = g_veh.count.load() == 1 && g_veh.seg_cs == 0x23;
        const bool resumed = result.edx == kTeb && result.eax == kTeb;
        std::printf("  %s: delivered in CS 0x23=%s, resumed in CS 0x23 through the stub=%s, FS base reset by the "
                    "round trip=%s\n",
                    run.label, delivered ? "yes" : "no", resumed ? "yes" : "no",
                    g_veh.repairs.load() > 0 ? "yes" : "no");
        ok = ok && delivered && resumed;
    }
    _writefsbase_u64(original);
    std::printf("Q5 %s: compatibility-mode exceptions reach the VEH and resume through a 64-bit stub\n",
                ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

// Q6: where selector 0x53 points.
int CaseSelector53()
{
    std::uint64_t original = 0;
    if (!FsGsBaseUsable(&original))
    {
        return 1;
    }
    const std::uint64_t teb = reinterpret_cast<std::uint64_t>(NtCurrentTeb());
    const std::uint64_t base = reinterpret_cast<std::uint64_t (*)()>(kBase + 0x180)();
    _writefsbase_u64(original);
    std::printf("  TEB(64)=0x%llx original fs base=0x%llx, after loading 0x53=0x%llx (TEB+0x2000=0x%llx)\n",
                static_cast<unsigned long long>(teb), static_cast<unsigned long long>(original),
                static_cast<unsigned long long>(base), static_cast<unsigned long long>(teb + 0x2000));
    std::printf("Q6 INFO: selector 0x53 gives fs base 0x%llx\n", static_cast<unsigned long long>(base));
    return 0;
}

// Q5 control: Q2's faults with the 64-bit TEB's stack limits left alone.
// Dispatch itself accepts the guest stack; only handler code that probes its
// frame there needs the limits (RunGuest).
int CaseForeignStack()
{
    ResetVeh();
    const GuestResult result = RunGuest(kG2, false);
    std::printf("  returned: fs:[0x18]=0x%08x veh=%d\n", result.eax, g_veh.count.load());
    return 0;
}

int RunCase(int number)
{
    if (!PrepareLowMemory())
    {
        return 2;
    }
    AddVectoredExceptionHandler(1, &ProbeHandler);
    switch (number)
    {
    case 1:
        return CaseTransition();
    case 2:
        return CaseDefaultFs();
    case 3:
        return CaseFsGsBase();
    case 4:
        return CasePersistence();
    case 5:
        return CaseExceptions();
    case 6:
        return CaseSelector53();
    case 7:
        return CaseForeignStack();
    default:
        return 2;
    }
}

int RunAll()
{
    wchar_t executable[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    for (int number = 1; number <= 7; ++number)
    {
        std::printf("[case %d]\n", number);
        std::wstring command = L"\"" + std::wstring(executable) + L"\" --case " + std::to_wstring(number);
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        if (!CreateProcessW(executable, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
                            &process))
        {
            std::printf("  cannot start (error %lu)\n", GetLastError());
            continue;
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(process.hProcess, &exit_code);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::printf("  exit code 0x%08lx\n", exit_code);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    // Unbuffered, so a case that crashes still shows how far it got.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc == 3 && std::strcmp(argv[1], "--case") == 0)
    {
        return RunCase(std::atoi(argv[2]));
    }
    return RunAll();
}
