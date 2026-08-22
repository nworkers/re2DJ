#include <string>
#include <vector>

#include "re2dj/platform/linux/native_helper_backend.h"

namespace re2dj::platform::windows
{

using NativeHelperBackend = re2dj::platform::linux::NativeHelperBackend;

}  // namespace re2dj::platform::windows

#define RE2DJ_PLATFORM_WINDOWS_NATIVE_HELPER_BACKEND_H_
#define wmain LinuxProbeMain
#include "../windows/native_ipc_host_probe.cpp"
#undef wmain

int main(int argc, char** argv)
{
    std::vector<std::wstring> arguments;
    std::vector<wchar_t*> pointers;
    arguments.reserve(static_cast<std::size_t>(argc));
    pointers.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index)
    {
        const char* source = argv[index];
        std::wstring value;
        while (*source != '\0')
        {
            value.push_back(static_cast<unsigned char>(*source));
            ++source;
        }
        arguments.push_back(std::move(value));
    }
    for (std::wstring& argument : arguments)
    {
        pointers.push_back(argument.data());
    }
    return LinuxProbeMain(argc, pointers.data());
}
