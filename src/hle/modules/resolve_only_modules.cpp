#include "re2dj/hle/modules/resolve_only_modules.h"

#include <array>
#include <string>
#include <utility>

namespace re2dj::hle::modules
{
namespace
{

GuestModuleDescriptor MakeModule(const char* name,
                                 const char* alias,
                                 std::span<const ResolveOnlyExport> exports)
{
    GuestModuleDescriptor descriptor;
    descriptor.name = name;
    descriptor.aliases = {alias};
    AddResolveOnlyExports(&descriptor, exports);
    return descriptor;
}

// Argument counts follow the Win32 signatures (dsound.h, dinput.h, vfw.h,
// winsock2.h).
// 4th imports DirectSoundCreate by ordinal 1.
constexpr ResolveOnlyExport kDsound[] = {{"DirectSoundCreate", 3, 1}};
constexpr ResolveOnlyExport kDinput[] = {{"DirectInputCreateA", 4}};
constexpr ResolveOnlyExport kAvifil32[] = {
    {"AVIStreamInfoA", 3}, {"AVIStreamOpenFromFileA", 6}, {"AVIStreamGetFrameOpen", 2},
    {"AVIStreamRelease", 1}, {"AVIStreamGetFrame", 2},
};
// 4th imports ws2_32 by ordinal only; the names are the ordinals' exports.
constexpr ResolveOnlyExport kWs2_32[] = {
    {"bind", 3, 2}, {"closesocket", 1, 3}, {"htons", 1, 9}, {"inet_addr", 1, 11},
    {"recvfrom", 6, 17}, {"sendto", 6, 20}, {"socket", 3, 23}, {"WSAStartup", 2, 115},
    {"WSACleanup", 0, 116},
};

}  // namespace

void AddResolveOnlyExports(GuestModuleDescriptor* descriptor,
                           std::span<const ResolveOnlyExport> exports)
{
    for (const ResolveOnlyExport& entry : exports)
    {
        GuestExportDescriptor export_descriptor;
        export_descriptor.name = entry.name;
        if (entry.ordinal != 0)
        {
            export_descriptor.ordinal = entry.ordinal;
        }
        export_descriptor.calling_convention = entry.calling_convention;
        export_descriptor.argument_count = entry.argument_count;
        export_descriptor.handler = &UnimplementedExport;
        descriptor->exports.push_back(std::move(export_descriptor));
    }
}

std::vector<GuestModuleDescriptor> MakeResolveOnlyModuleDescriptors()
{
    std::vector<GuestModuleDescriptor> descriptors;
    descriptors.push_back(MakeModule("dsound.dll", "dsound", kDsound));
    descriptors.push_back(MakeModule("dinput.dll", "dinput", kDinput));
    descriptors.push_back(MakeModule("avifil32.dll", "avifil32", kAvifil32));
    descriptors.push_back(MakeModule("ws2_32.dll", "ws2_32", kWs2_32));
    return descriptors;
}

}  // namespace re2dj::hle::modules
