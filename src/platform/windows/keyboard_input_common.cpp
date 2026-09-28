#define NOMINMAX
#include <windows.h>

#include "keyboard_input_common.h"

#include "re2dj/input/virtual_keys.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace re2dj::platform::windows
{
namespace
{

namespace input = re2dj::input;

// The core's key codes are winuser.h's.
static_assert(input::kVkLButton == VK_LBUTTON && input::kVkRButton == VK_RBUTTON && input::kVkMButton == VK_MBUTTON);
static_assert(input::kVkBack == VK_BACK && input::kVkTab == VK_TAB && input::kVkReturn == VK_RETURN);
static_assert(input::kVkShift == VK_SHIFT && input::kVkControl == VK_CONTROL && input::kVkMenu == VK_MENU);
static_assert(input::kVkPause == VK_PAUSE && input::kVkCapital == VK_CAPITAL && input::kVkEscape == VK_ESCAPE);
static_assert(input::kVkSpace == VK_SPACE && input::kVkPrior == VK_PRIOR && input::kVkNext == VK_NEXT);
static_assert(input::kVkEnd == VK_END && input::kVkHome == VK_HOME && input::kVkLeft == VK_LEFT);
static_assert(input::kVkUp == VK_UP && input::kVkRight == VK_RIGHT && input::kVkDown == VK_DOWN);
static_assert(input::kVkInsert == VK_INSERT && input::kVkDelete == VK_DELETE);
static_assert(input::kVkLWin == VK_LWIN && input::kVkRWin == VK_RWIN && input::kVkNumpad0 == VK_NUMPAD0);
static_assert(input::kVkMultiply == VK_MULTIPLY && input::kVkAdd == VK_ADD && input::kVkSubtract == VK_SUBTRACT);
static_assert(input::kVkDecimal == VK_DECIMAL && input::kVkDivide == VK_DIVIDE && input::kVkF1 == VK_F1);
static_assert(input::kVkNumLock == VK_NUMLOCK && input::kVkScroll == VK_SCROLL);
static_assert(input::kVkLShift == VK_LSHIFT && input::kVkRShift == VK_RSHIFT);
static_assert(input::kVkLControl == VK_LCONTROL && input::kVkRControl == VK_RCONTROL);
static_assert(input::kVkLMenu == VK_LMENU && input::kVkRMenu == VK_RMENU);
static_assert(input::kVkOem1 == VK_OEM_1 && input::kVkOemPlus == VK_OEM_PLUS && input::kVkOemComma == VK_OEM_COMMA);
static_assert(input::kVkOemMinus == VK_OEM_MINUS && input::kVkOemPeriod == VK_OEM_PERIOD && input::kVkOem2 == VK_OEM_2);
static_assert(input::kVkOem3 == VK_OEM_3 && input::kVkOem4 == VK_OEM_4 && input::kVkOem5 == VK_OEM_5);
static_assert(input::kVkOem6 == VK_OEM_6 && input::kVkOem7 == VK_OEM_7);

int ParseKey(const std::string& input)
{
    return input::ParseKeyName(input);
}

}  // namespace

bool ParseKeyboardKeyName(const char* name, int* key)
{
    if (name == nullptr || key == nullptr) return false;
    *key = ParseKey(name);
    return *key >= 0;
}

bool ReadKeyboardKeyBinding(const char* path,
                            const char* section,
                            const char* name,
                            int* key,
                            bool* present,
                            std::string* error)
{
    if (path == nullptr || section == nullptr || name == nullptr || key == nullptr ||
        present == nullptr || error == nullptr)
    {
        return false;
    }
    // No key name parses to this, so reading it back means the file has no such
    // entry and the caller's default stands.
    constexpr const char* kAbsent = "\x01";
    char value[32] = {};
    GetPrivateProfileStringA(section, name, kAbsent, value, sizeof(value), path);
    *present = std::strcmp(value, kAbsent) != 0;
    if (!*present)
    {
        return true;
    }
    *key = ParseKey(value);
    if (*key >= 0)
    {
        return true;
    }
    *error = std::string("unknown key name for ") + section + "." + name + ": " + value;
    return false;
}

bool IsKeyboardKeyPressed(int key)
{
    return key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

}  // namespace re2dj::platform::windows
