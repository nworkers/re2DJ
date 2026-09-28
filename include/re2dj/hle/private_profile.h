#ifndef RE2DJ_HLE_PRIVATE_PROFILE_H_
#define RE2DJ_HLE_PRIVATE_PROFILE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The private profile (INI) rules both hosts answer the guest with. The
// reading rules are Windows 11's, as measured with a 32-bit program (design
// 405); the Linux facade applies them to the guest's files, the Windows
// product leaves reading to Windows itself.
namespace re2dj::hle
{

// The value of key in section, found as Windows 11 finds it:
// - section and key names compare without case and without the spaces
//   around them;
// - only a section name's first occurrence is searched, and a key's first
//   line in it counts;
// - lines starting with ';' and lines without '=' hold no key;
// - the value is the text after the first '=', without the spaces around it.
// Nothing when the section or the key is missing.
std::optional<std::string> FindPrivateProfileValue(std::string_view text,
                                                   std::string_view section,
                                                   std::string_view key);

// GetPrivateProfileIntA's number for a value found: an empty value gives
// default_value; otherwise one leading quote is skipped, then an optional
// sign and decimal digits are read (hexadecimal after a lowercase "0x") up to
// the first other character, wrapping at 2^32. A value with no digits is 0.
std::uint32_t ParsePrivateProfileInt(std::string_view value, std::uint32_t default_value);

// The key names of section's first occurrence in file order, or nothing
// when the section is missing.
std::optional<std::vector<std::string>> ListPrivateProfileKeys(std::string_view text, std::string_view section);
// The section names in file order, each as written between the brackets
// without its surrounding spaces.
std::vector<std::string> ListPrivateProfileSections(std::string_view text);

// GetPrivateProfileStringA's text for a value found: one pair of matching
// quotes (" or ') around the whole value is removed.
std::string PrivateProfileStringValue(std::string_view value);
// Its text for a default: the default without trailing spaces; none is empty.
std::string PrivateProfileStringDefault(std::string_view default_value);

// What GetPrivateProfileStringA writes into a buffer of size bytes and
// returns, as measured on Windows 11: one string is cut to size - 1 bytes;
// a list (NUL after each name, one more at the end) is cut to size - 2
// bytes of names and ends with two NULs. truncated reports a cut, and a
// string exactly size - 1 long counts as one too.
struct PrivateProfileCopy
{
    std::vector<std::uint8_t> bytes;
    std::uint32_t length = 0;
    bool truncated = false;
};
PrivateProfileCopy CopyPrivateProfileString(std::string_view text, std::uint32_t size);
PrivateProfileCopy CopyPrivateProfileList(const std::vector<std::string>& names, std::uint32_t size);

// The DemoVolume setting both products answer from their own configuration
// (Task 086) instead of the guest's file, and its default.
inline constexpr std::uint32_t kDefaultDemoVolume = 3;
inline constexpr std::uint32_t kMaximumDemoVolume = 3;

// The value the product answers in place of the file's for this key, or
// nothing to read the file: [GAMEASSIGNMENTS] DemoVolume while demo_volume is
// within 0..3.
std::optional<std::uint32_t> PrivateProfileIntOverride(std::string_view section,
                                                       std::string_view key,
                                                       std::uint32_t demo_volume);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_PRIVATE_PROFILE_H_
