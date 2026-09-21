#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace re2dj::platform::linux
{

constexpr std::size_t kNativeInstructionTraceMaximumFrames = 128;

struct NativeInstructionTraceFrame
{
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
};

struct NativeInstructionTrace
{
    bool armed = false;
    bool started = false;
    bool limit_reached = false;
    std::uint32_t breakpoint = 0;
    std::uint32_t frame_count = 0;
    std::array<NativeInstructionTraceFrame, kNativeInstructionTraceMaximumFrames> frames = {};
};

struct NativeGuestFault
{
    std::uint32_t status_code = 0;
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t fault_address = 0;
    std::uint32_t signal_code = 0;
    std::uint32_t cpu_error_code = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
};

bool ArmNativeInstructionTrace(NativeInstructionTrace* trace,
                               std::uint32_t breakpoint,
                               std::uint32_t image_base,
                               std::uint32_t image_size,
                               std::string* error);
bool ResumeNativeInstructionTrace(std::uint32_t return_address);
void FinalizeNativeInstructionTrace(NativeInstructionTrace* trace);

class NativeProcessBootstrap
{
public:
    NativeProcessBootstrap();
    ~NativeProcessBootstrap();

    NativeProcessBootstrap(const NativeProcessBootstrap&) = delete;
    NativeProcessBootstrap& operator=(const NativeProcessBootstrap&) = delete;

    bool Initialize(std::uint32_t image_base, std::string* error);
    bool RunTlsCallback(std::uint32_t callback,
                        std::uint32_t image_base,
                        NativeGuestFault* fault,
                        std::string* error);
    bool RunEntry(std::uint32_t entry,
                  std::uint32_t* result,
                  NativeGuestFault* fault,
                  std::string* error);
    std::uint32_t GuestStackBase() const;
    std::uint32_t GuestStackLimit() const;
    bool IsGuestStackRange(std::uint32_t address, std::uint32_t size) const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_
