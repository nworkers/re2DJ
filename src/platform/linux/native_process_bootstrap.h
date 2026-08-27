#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_

#include <cstdint>
#include <string>

namespace re2dj::platform::linux
{

struct NativeGuestFault
{
    std::uint32_t status_code = 0;
    std::uint32_t instruction_pointer = 0;
    std::uint32_t stack_pointer = 0;
};

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

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_PROCESS_BOOTSTRAP_H_
