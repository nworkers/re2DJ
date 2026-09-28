#ifndef RE2DJ_HLE_WSPRINTF_H_
#define RE2DJ_HLE_WSPRINTF_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace re2dj::hle
{

// The most characters wsprintfA writes before its terminator, as measured on
// Windows 11: a longer result is cut there and the terminator follows.
inline constexpr std::size_t kWsprintfMaximumOutput = 1024;

// Reads the variadic arguments in order: the next 32-bit word, and a guest
// ANSI string at an address other than 0. Each returns false when it cannot.
using WsprintfWordReader = std::function<bool(std::uint32_t* word)>;
using WsprintfStringReader = std::function<bool(std::uint32_t address, std::string* text)>;

// user32 wsprintfA's formatting, as measured on Windows 11 (design 419):
//   %[-#]...[0][width][.precision][h|l|I64]type
// '-' and '#' in any order, then '0'; any other character in a flag's place
// is taken as the type. Types d i u x X p s c and their options follow the
// measurements (h truncates only d and i to 16 bits; p is X with precision 8;
// # adds 0x/0X outside the width; a NULL %s is empty). An unknown type
// writes itself without padding or taking an argument; a format ending
// inside a specification ends the output there. Wide text (l or w with s or
// c, S, C) is not modelled and fails with error set.
bool FormatWsprintf(std::string_view format,
                    const WsprintfWordReader& next_word,
                    const WsprintfStringReader& read_string,
                    std::string* output,
                    std::string* error);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_WSPRINTF_H_
