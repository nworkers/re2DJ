#include "re2dj/input/virtual_keys.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace re2dj::input
{

int ParseKeyName(std::string_view name)
{
    std::string value(name);
    for (char& character : value)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    if (value.empty() || value == "NONE") return 0;
    if (value.size() == 1 && ((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= '0' && value[0] <= '9')))
        return value[0];
    if (value[0] == 'F' && value.size() <= 3)
    {
        // atoi, as the Windows host always read it, so "F1X" is still F1.
        const int number = std::atoi(value.c_str() + 1);
        if (number >= 1 && number <= 24) return kVkF1 + number - 1;
    }
    if (value.rfind("NUMPAD", 0) == 0 && value.size() == 7 && value[6] >= '0' && value[6] <= '9')
        return kVkNumpad0 + value[6] - '0';
    if (value == "TAB") return kVkTab;
    if (value == "ENTER") return kVkReturn;
    if (value == "SPACE") return kVkSpace;
    if (value == "ESCAPE" || value == "ESC") return kVkEscape;
    if (value == "LSHIFT") return kVkLShift;
    if (value == "RSHIFT") return kVkRShift;
    if (value == "SHIFT") return kVkShift;
    if (value == "LCONTROL" || value == "LCTRL") return kVkLControl;
    if (value == "RCONTROL" || value == "RCTRL") return kVkRControl;
    if (value == "CONTROL" || value == "CTRL") return kVkControl;
    if (value == "LMENU" || value == "LALT") return kVkLMenu;
    if (value == "RMENU" || value == "RALT") return kVkRMenu;
    if (value == "ALT") return kVkMenu;
    if (value == "BACKSPACE" || value == "BACK") return kVkBack;
    if (value == "CAPITAL" || value == "CAPSLOCK" || value == "CAPS") return kVkCapital;
    if (value == "LEFT") return kVkLeft;
    if (value == "RIGHT") return kVkRight;
    if (value == "UP") return kVkUp;
    if (value == "DOWN") return kVkDown;
    if (value == "DECIMAL") return kVkDecimal;
    if (value == "INSERT") return kVkInsert;
    if (value == "DELETE" || value == "DEL") return kVkDelete;
    if (value == "HOME") return kVkHome;
    if (value == "END") return kVkEnd;
    if (value == "PAGEUP" || value == "PGUP" || value == "PRIOR") return kVkPrior;
    if (value == "PAGEDOWN" || value == "PGDN" || value == "NEXT") return kVkNext;
    return -1;
}

}  // namespace re2dj::input
