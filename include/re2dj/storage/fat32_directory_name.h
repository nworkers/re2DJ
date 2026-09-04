#ifndef RE2DJ_STORAGE_FAT32_DIRECTORY_NAME_H_
#define RE2DJ_STORAGE_FAT32_DIRECTORY_NAME_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace re2dj::storage
{

// Name decoding for FAT directory entries, kept apart from the volume so it can
// be exercised on synthetic entries without an image.

// Every FAT directory entry, short or long, occupies this many bytes.
inline constexpr std::size_t kFatDirectoryEntryBytes = 32;

// Characters one long-name slot carries, across its three character fields.
inline constexpr std::size_t kFatLongNameSlotCharacters = 13;

// Attribute byte marking an entry as a long-name slot rather than a file.
inline constexpr std::uint8_t kFatLongNameAttribute = 0x0f;

// Formats the 8.3 name of a short entry, trimming the field padding and
// inserting the dot only when an extension is present.
std::string DecodeFatShortName(const std::uint8_t* entry);

// Checksum of a short entry's eleven name bytes. Every long-name slot of the
// same file repeats it, which is how slots are matched to their entry.
std::uint8_t FatShortNameChecksum(const std::uint8_t* short_entry);

// Collects the long-name slots that precede a short entry. Slots appear in
// reverse order on disk, so the assembler keeps their sequence numbers and
// orders them when decoding.
class FatLongNameAssembler
{
public:
    void Clear();

    // Takes one slot, which the caller has already identified by its
    // `kFatLongNameAttribute` attribute byte. A slot that breaks the sequence
    // or disagrees with the collected checksum discards what came before it.
    void Add(const std::uint8_t* entry);

    // Decodes the collected slots for `short_entry`. Returns false when no
    // usable set was collected, when the checksum does not match the entry, or
    // when a sequence number is missing, in which case the caller keeps the
    // short name.
    bool Decode(const std::uint8_t* short_entry, std::string* name) const;

private:
    std::vector<std::pair<std::uint8_t, std::vector<std::uint16_t>>> parts_;
    std::uint8_t checksum_ = 0;
    bool has_last_ = false;
};

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_FAT32_DIRECTORY_NAME_H_
