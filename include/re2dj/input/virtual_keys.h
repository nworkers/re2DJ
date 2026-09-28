#ifndef RE2DJ_INPUT_VIRTUAL_KEYS_H_
#define RE2DJ_INPUT_VIRTUAL_KEYS_H_

#include <string_view>

// Win32 virtual-key codes (winuser.h), the numbers keys are known by in the
// guest's GetAsyncKeyState and in key bindings, without the Windows headers,
// so either host can name a key. The Windows host checks them against
// winuser.h. Letters and digits are their uppercase ASCII codes.
namespace re2dj::input
{

inline constexpr int kVkLButton = 0x01;
inline constexpr int kVkRButton = 0x02;
inline constexpr int kVkMButton = 0x04;
inline constexpr int kVkBack = 0x08;
inline constexpr int kVkTab = 0x09;
inline constexpr int kVkReturn = 0x0D;
inline constexpr int kVkShift = 0x10;
inline constexpr int kVkControl = 0x11;
inline constexpr int kVkMenu = 0x12;
inline constexpr int kVkPause = 0x13;
inline constexpr int kVkCapital = 0x14;
inline constexpr int kVkEscape = 0x1B;
inline constexpr int kVkSpace = 0x20;
inline constexpr int kVkPrior = 0x21;
inline constexpr int kVkNext = 0x22;
inline constexpr int kVkEnd = 0x23;
inline constexpr int kVkHome = 0x24;
inline constexpr int kVkLeft = 0x25;
inline constexpr int kVkUp = 0x26;
inline constexpr int kVkRight = 0x27;
inline constexpr int kVkDown = 0x28;
inline constexpr int kVkInsert = 0x2D;
inline constexpr int kVkDelete = 0x2E;
inline constexpr int kVkLWin = 0x5B;
inline constexpr int kVkRWin = 0x5C;
inline constexpr int kVkNumpad0 = 0x60;
inline constexpr int kVkMultiply = 0x6A;
inline constexpr int kVkAdd = 0x6B;
inline constexpr int kVkSubtract = 0x6D;
inline constexpr int kVkDecimal = 0x6E;
inline constexpr int kVkDivide = 0x6F;
inline constexpr int kVkF1 = 0x70;
inline constexpr int kVkNumLock = 0x90;
inline constexpr int kVkScroll = 0x91;
inline constexpr int kVkLShift = 0xA0;
inline constexpr int kVkRShift = 0xA1;
inline constexpr int kVkLControl = 0xA2;
inline constexpr int kVkRControl = 0xA3;
inline constexpr int kVkLMenu = 0xA4;
inline constexpr int kVkRMenu = 0xA5;
inline constexpr int kVkOem1 = 0xBA;
inline constexpr int kVkOemPlus = 0xBB;
inline constexpr int kVkOemComma = 0xBC;
inline constexpr int kVkOemMinus = 0xBD;
inline constexpr int kVkOemPeriod = 0xBE;
inline constexpr int kVkOem2 = 0xBF;
inline constexpr int kVkOem3 = 0xC0;
inline constexpr int kVkOem4 = 0xDB;
inline constexpr int kVkOem5 = 0xDC;
inline constexpr int kVkOem6 = 0xDD;
inline constexpr int kVkOem7 = 0xDE;

// A key name as the binding tables and INI files write it (case-insensitive:
// a letter or digit, F1..F24, NUMPAD0..9, TAB, ENTER, SPACE, ESC, the shift,
// control and alt keys, arrows, and the editing keys): its virtual-key code,
// 0 for NONE or an empty name (unbound), or -1 for a name it does not know.
int ParseKeyName(std::string_view name);

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_VIRTUAL_KEYS_H_
