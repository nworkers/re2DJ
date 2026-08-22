#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{

std::uint32_t gate_calls = 0;

extern "C" __declspec(noinline) std::uint32_t __stdcall ProbeGate(std::uint32_t value)
{
    ++gate_calls;
    return value + 1;
}

}  // namespace

int main()
{
    static_assert(sizeof(void*) == 4, "native helper probe must be built for Win32 x86");

    BOOL is_wow64 = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &is_wow64) || is_wow64 == FALSE)
    {
        std::fprintf(stderr, "native-helper-probe: process is not running under WOW64\n");
        return 1;
    }

    // push 41; mov eax, ProbeGate; call eax; ret
    std::uint8_t guest_code[] = {
        0x6A, 0x29,
        0xB8, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,
        0xC3,
    };
    const std::uintptr_t gate_address = reinterpret_cast<std::uintptr_t>(&ProbeGate);
    static_assert(sizeof(gate_address) == 4);
    std::memcpy(guest_code + 3, &gate_address, sizeof(gate_address));

    void* memory = VirtualAlloc(nullptr,
                                sizeof(guest_code),
                                MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE);
    if (memory == nullptr)
    {
        std::fprintf(stderr, "native-helper-probe: VirtualAlloc failed\n");
        return 2;
    }
    std::memcpy(memory, guest_code, sizeof(guest_code));

    DWORD previous_protection = 0;
    if (!VirtualProtect(memory,
                        sizeof(guest_code),
                        PAGE_EXECUTE_READ,
                        &previous_protection) ||
        !FlushInstructionCache(GetCurrentProcess(), memory, sizeof(guest_code)))
    {
        std::fprintf(stderr, "native-helper-probe: executable mapping setup failed\n");
        VirtualFree(memory, 0, MEM_RELEASE);
        return 3;
    }

    using GuestEntry = std::uint32_t(__cdecl*)();
#pragma warning(suppress : 4191)
    const GuestEntry entry = reinterpret_cast<GuestEntry>(memory);
    const std::uint32_t result = entry();
    VirtualFree(memory, 0, MEM_RELEASE);

    std::printf("native-helper-probe: x86=yes wow64=yes gate-calls=%u result=%u\n",
                gate_calls,
                result);
    return gate_calls == 1 && result == 42 ? 0 : 4;
}
