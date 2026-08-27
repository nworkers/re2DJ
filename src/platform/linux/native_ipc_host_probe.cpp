#include <string>
#include <vector>

#include <signal.h>

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
    const int baseline_result = LinuxProbeMain(argc, pointers.data());
    if (baseline_result != 0 || argc != 2)
    {
        return baseline_result;
    }

    std::vector<std::uint8_t> fault_image = MakeSyntheticPe32();
    const std::array<std::uint8_t, 18> fault_code = {
        0x64, 0xA1, 0x30, 0x00, 0x00, 0x00,
        0x81, 0x78, 0x08, 0x00, 0x00, 0x00, 0x10,
        0x75, 0x02,
        0x0F, 0x0B,
        0xCC,
    };
    for (std::size_t index = 0; index < fault_code.size(); ++index)
    {
        fault_image[0x400 + index] = fault_code[index];
    }
    re2dj::exe::PeImageInfo info;
    std::string error;
    if (!re2dj::exe::ReadPeImageInfo(fault_image.data(), fault_image.size(), &info, &error))
    {
        std::fprintf(stderr, "linux-native-fault-probe: %s\n", error.c_str());
        return 4;
    }

    re2dj::platform::linux::NativeHelperBackend backend{
        std::filesystem::path(pointers[1])};
    re2dj::runtime::LoadedPeImage loaded;
    re2dj::runtime::ExecutionEvent event = {};
    const bool success =
        backend.PrepareImage(fault_image,
                             info,
                             re2dj::runtime::GuestAddress(kImageBase),
                             &loaded,
                             &error) &&
        backend.Start(&error) &&
        backend.WaitForEvent(&event, &error) &&
        event.kind == re2dj::runtime::ExecutionEventKind::kFault &&
        event.status_code == SIGILL &&
        event.instruction_pointer.value() == kImageBase + kEntryRva + 15 &&
        event.stack_pointer.value() != 0;
    if (!success)
    {
        std::fprintf(stderr,
                     "linux-native-fault-probe: %s status=%u eip=0x%08x esp=0x%08x\n",
                     error.c_str(),
                     event.status_code,
                     event.instruction_pointer.value(),
                     event.stack_pointer.value());
        backend.RequestStop();
        return 5;
    }
    std::printf("linux-native-fault-probe: signal=%u eip=0x%08x esp=0x%08x\n",
                event.status_code,
                event.instruction_pointer.value(),
                event.stack_pointer.value());
    return 0;
}
