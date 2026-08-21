#ifndef RE2DJ_TESTS_UNIT_SYNTHETIC_PE32_H_
#define RE2DJ_TESTS_UNIT_SYNTHETIC_PE32_H_

#include <cstddef>
#include <cstdint>
#include <vector>

// A hand-built PE32 image, shared by the header-reader tests and the HDD scan
// tests. Building one here keeps the suite free of a checked-in binary, which
// the repository rules forbid anyway.
namespace re2dj::test
{

// Offsets inside the image, exposed so a test can corrupt one field and check
// that the reader rejects it.
inline constexpr std::size_t kSyntheticPeOffset = 0x80;
inline constexpr std::size_t kSyntheticFileHeaderOffset = kSyntheticPeOffset + 4;
inline constexpr std::size_t kSyntheticOptionalOffset = kSyntheticFileHeaderOffset + 20;
// 96 fixed optional-header bytes plus 16 data directories.
inline constexpr std::uint16_t kSyntheticOptionalSize = 224;
inline constexpr std::size_t kSyntheticSectionTableOffset =
    kSyntheticOptionalOffset + kSyntheticOptionalSize;

void PutU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value);
void PutU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value);

// A well-formed 32-bit x86 PE32 GUI executable with two sections and an import
// directory entry.
std::vector<std::uint8_t> MakeSyntheticPe32Image();

}  // namespace re2dj::test

#endif  // RE2DJ_TESTS_UNIT_SYNTHETIC_PE32_H_
