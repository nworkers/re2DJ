#ifndef RE2DJ_HLE_MODULES_RESOLVE_ONLY_MODULES_H_
#define RE2DJ_HLE_MODULES_RESOLVE_ONLY_MODULES_H_

#include <cstdint>
#include <span>
#include <vector>

#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules
{

// One export the guest resolves but has not been seen to call: its Win32
// argument count and, for DLLs imported by ordinal, the ordinal.
struct ResolveOnlyExport
{
    const char* name = nullptr;
    std::uint32_t argument_count = 0;
    std::uint16_t ordinal = 0;
    CallingConvention calling_convention = CallingConvention::kStdcall;
};

// Appends resolve-only exports (UnimplementedExport) to a descriptor.
void AddResolveOnlyExports(GuestModuleDescriptor* descriptor,
                           std::span<const ResolveOnlyExport> exports);

// The DLLs 4th's protection envelope resolves when it rebuilds the original
// program's import table, beyond kernel32, user32, advapi32, and winmm:
// gdi32, dsound, dinput, ddraw, avifil32, and ws2_32. Every export is
// resolve-only until a call is observed.
std::vector<GuestModuleDescriptor> MakeResolveOnlyModuleDescriptors();

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_RESOLVE_ONLY_MODULES_H_
