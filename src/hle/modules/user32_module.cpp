#include "re2dj/hle/modules/user32_module.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace re2dj::hle::modules
{
namespace
{

// No export of this module creates a window, so the calling thread has no
// active window and NULL is the accurate result. Once a window-creating export
// is added, this must query the window service instead.
bool GetActiveWindow(const ImportCall&, ImportReturn* result, std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = "user32 result is null";
        }
        return false;
    }
    *result = {};
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// MessageBoxA(hWnd, lpText, lpCaption, uType). No platform service shows the
// box yet, so the result is the default button's ID, as if the user accepted
// it; see MessageBoxDefaultButton.
bool MessageBoxA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        if (error != nullptr)
        {
            *error = result == nullptr ? "user32 result is null"
                                       : "user32 MessageBoxA argument shape is invalid";
        }
        return false;
    }
    *result = {};
    result->eax = MessageBoxDefaultButton(call.arguments[3]);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

GuestExportDescriptor MakeExport(std::string name,
                                 std::uint32_t argument_count,
                                 ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

}  // namespace

std::uint32_t MessageBoxDefaultButton(std::uint32_t type)
{
    // Button sets by MB_TYPEMASK value, in the order the box shows them.
    static constexpr std::array<std::array<std::uint32_t, 3>, 7> kButtons = {{
        {kIdOk, 0, 0},                      // MB_OK
        {kIdOk, kIdCancel, 0},              // MB_OKCANCEL
        {kIdAbort, kIdRetry, kIdIgnore},    // MB_ABORTRETRYIGNORE
        {kIdYes, kIdNo, kIdCancel},         // MB_YESNOCANCEL
        {kIdYes, kIdNo, 0},                 // MB_YESNO
        {kIdRetry, kIdCancel, 0},           // MB_RETRYCANCEL
        {kIdCancel, kIdTryAgain, kIdContinue},  // MB_CANCELTRYCONTINUE
    }};
    const std::uint32_t set = type & 0xFU;
    const auto& buttons = kButtons[set < kButtons.size() ? set : 0];
    // MB_DEFBUTTON1..3 select a position; an empty or unknown position falls
    // back to the first button.
    const std::uint32_t position = (type & 0xF00U) >> 8;
    return position < buttons.size() && buttons[position] != 0 ? buttons[position]
                                                                : buttons[0];
}

GuestModuleDescriptor MakeUser32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "user32.dll";
    descriptor.aliases = {"user32"};
    descriptor.exports.push_back(MakeExport("GetActiveWindow", 0, &GetActiveWindow));
    descriptor.exports.push_back(MakeExport("MessageBoxA", 4, &MessageBoxA));
    return descriptor;
}

}  // namespace re2dj::hle::modules
