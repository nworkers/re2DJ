#include "re2dj/hle/modules/gdi32_module.h"

#include <array>
#include <string>
#include <utility>

#include "re2dj/hle/modules/resolve_only_modules.h"

namespace re2dj::hle::modules
{
namespace
{

// GetStockObject(i): the stock object's handle. Stock objects are shared and
// never freed, so the handle is a constant; an unknown index gives NULL
// without touching the last error. Measured on Windows 11 (WOW64).
bool GetStockObject(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        if (error != nullptr)
        {
            *error = result == nullptr ? "gdi32 result is null"
                                       : "gdi32 GetStockObject argument shape is invalid";
        }
        return false;
    }
    *result = {};
    result->eax = StockObjectHandle(call.arguments[0]);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

GuestExportDescriptor MakeExport(std::string name, std::uint32_t argument_count, ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

// wingdi.h signatures.
constexpr ResolveOnlyExport kGdi32ResolveOnly[] = {
    {"SetTextColor", 2}, {"DeleteObject", 1}, {"CreateSolidBrush", 1},
    {"StretchDIBits", 13}, {"SelectPalette", 3}, {"CreatePalette", 1},
    {"BitBlt", 9}, {"SelectObject", 2}, {"CreateCompatibleDC", 1},
    {"CreateDIBSection", 6}, {"SetBkMode", 2},
};

}  // namespace

std::uint32_t StockObjectHandle(std::uint32_t index)
{
    // WHITE_BRUSH (0) through DC_PEN (19); index 9 is unused. The values are
    // those Windows 11 hands a 32-bit process.
    static constexpr std::array<std::uint32_t, 20> kStockObjects = {
        0x00900010U, 0x00900014U, 0x00900012U, 0x00900013U, 0x00900011U,
        0x00900015U, 0x00B00018U, 0x00B00017U, 0x00B00016U, 0x00000000U,
        0x018A0831U, 0x008A0024U, 0x008A0023U, 0x028A0021U, 0x008A0022U,
        0x0088000BU, 0x018A0832U, 0x000A0834U, 0x0190001CU, 0x00B00019U,
    };
    return index < kStockObjects.size() ? kStockObjects[index] : 0;
}

GuestModuleDescriptor MakeGdi32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "gdi32.dll";
    descriptor.aliases = {"gdi32"};
    descriptor.exports.push_back(MakeExport("GetStockObject", 1, &GetStockObject));
    AddResolveOnlyExports(&descriptor, kGdi32ResolveOnly);
    return descriptor;
}

}  // namespace re2dj::hle::modules
