#ifndef RE2DJ_HLE_HEX_BYTES_H_
#define RE2DJ_HLE_HEX_BYTES_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Bytes carried between host processes as text, such as a launcher's
// STARTUPINFO reserved area handed to a child run on the command line.
namespace re2dj::hle
{

// Bytes as lowercase hex.
std::string EncodeHexBytes(const std::vector<std::uint8_t>& bytes);
// Hex back to bytes; false for text that is not whole bytes.
bool DecodeHexBytes(std::string_view text, std::vector<std::uint8_t>* bytes);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_HEX_BYTES_H_
