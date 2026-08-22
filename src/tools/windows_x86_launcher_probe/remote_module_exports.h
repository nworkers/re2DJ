#ifndef RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_REMOTE_MODULE_EXPORTS_H_
#define RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_REMOTE_MODULE_EXPORTS_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace re2dj::tools::windows_x86_launcher_probe
{

// Result of resolving one export of a module mapped in another process.
// A forwarded export is a name string, not executable code, so `address` stays
// zero and `forwarded` is true; callers must not breakpoint such exports.
struct RemoteExportResolution
{
    std::uintptr_t address = 0;
    bool forwarded = false;
};

// Result of locating the export at or below one address in a remote module.
struct RemoteNearestExport
{
    char function[128] = {};
    std::uint32_t function_rva = 0;
    std::int32_t offset = 0;
    char module[128] = {};
};

// Resolves one named export of a PE32 module mapped in another process by
// parsing the module's export directory from that process's memory. Returns
// false only on read or parse failure; an absent export name is reported as a
// resolved miss through `error`.
bool ResolveRemotePe32Export(HANDLE process,
                             std::uintptr_t module_base,
                             const char* name,
                             RemoteExportResolution* resolution,
                             std::string* error);

// Finds the non-forwarded export whose RVA is the greatest one not above
// `address`, plus the export directory's own module name. Returns false when
// the directory cannot be read or no candidate exists below the address.
bool FindRemotePe32NearestExport(HANDLE process,
                                 std::uintptr_t module_base,
                                 std::uintptr_t address,
                                 RemoteNearestExport* result,
                                 std::string* error);

}  // namespace re2dj::tools::windows_x86_launcher_probe

#endif  // RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_REMOTE_MODULE_EXPORTS_H_
