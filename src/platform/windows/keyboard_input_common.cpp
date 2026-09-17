#define NOMINMAX
#include <windows.h>

#include "keyboard_input_common.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace re2dj::platform::windows
{
namespace
{

std::string Upper(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

int ParseKey(const std::string& input)
{
    const std::string value = Upper(input);
    if (value.empty() || value == "NONE") return 0;
    if (value.size() == 1 && ((value[0] >= 'A' && value[0] <= 'Z') ||
                              (value[0] >= '0' && value[0] <= '9'))) return value[0];
    if (value[0] == 'F' && value.size() <= 3)
    {
        const int number = std::atoi(value.c_str() + 1);
        if (number >= 1 && number <= 24) return VK_F1 + number - 1;
    }
    if (value.rfind("NUMPAD", 0) == 0 && value.size() == 7 &&
        value[6] >= '0' && value[6] <= '9') return VK_NUMPAD0 + value[6] - '0';
    if (value == "TAB") return VK_TAB;
    if (value == "ENTER") return VK_RETURN;
    if (value == "SPACE") return VK_SPACE;
    if (value == "ESCAPE" || value == "ESC") return VK_ESCAPE;
    if (value == "LSHIFT") return VK_LSHIFT;
    if (value == "RSHIFT") return VK_RSHIFT;
    if (value == "SHIFT") return VK_SHIFT;
    if (value == "LCONTROL" || value == "LCTRL") return VK_LCONTROL;
    if (value == "RCONTROL" || value == "RCTRL") return VK_RCONTROL;
    if (value == "CONTROL" || value == "CTRL") return VK_CONTROL;
    if (value == "LMENU" || value == "LALT") return VK_LMENU;
    if (value == "RMENU" || value == "RALT") return VK_RMENU;
    if (value == "ALT") return VK_MENU;
    if (value == "BACKSPACE" || value == "BACK") return VK_BACK;
    if (value == "CAPITAL" || value == "CAPSLOCK" || value == "CAPS") return VK_CAPITAL;
    if (value == "LEFT") return VK_LEFT;
    if (value == "RIGHT") return VK_RIGHT;
    if (value == "UP") return VK_UP;
    if (value == "DOWN") return VK_DOWN;
    if (value == "DECIMAL") return VK_DECIMAL;
    if (value == "INSERT") return VK_INSERT;
    if (value == "DELETE" || value == "DEL") return VK_DELETE;
    if (value == "HOME") return VK_HOME;
    if (value == "END") return VK_END;
    if (value == "PAGEUP" || value == "PGUP" || value == "PRIOR") return VK_PRIOR;
    if (value == "PAGEDOWN" || value == "PGDN" || value == "NEXT") return VK_NEXT;
    return -1;
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
