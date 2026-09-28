#ifndef RE2DJ_STORAGE_GUEST_FIND_H_
#define RE2DJ_STORAGE_GUEST_FIND_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/storage/fat32_chd.h"

// How a guest's FindFirstFileA and GetFileAttributesA see the image's
// directories, for both hosts: which names a pattern matches, what
// WIN32_FIND_DATAA an entry fills, and what attributes a path answers.
// These are the Windows product's VFS rules. They list what the image's
// directory holds, without the "." and ".." entries Windows 11 also reports
// on NTFS, and give files FILE_ATTRIBUTE_NORMAL where NTFS gives
// FILE_ATTRIBUTE_ARCHIVE.
namespace re2dj::storage
{

inline constexpr std::uint32_t kFileAttributeDirectory = 0x00000010U;
inline constexpr std::uint32_t kFileAttributeNormal = 0x00000080U;

// WIN32_FIND_DATAA in the guest's 32-bit layout.
struct GuestFindData
{
    std::uint32_t attributes = 0;
    std::uint32_t creation_time[2] = {};
    std::uint32_t last_access_time[2] = {};
    std::uint32_t last_write_time[2] = {};
    std::uint32_t size_high = 0;
    std::uint32_t size_low = 0;
    std::uint32_t reserved0 = 0;
    std::uint32_t reserved1 = 0;
    char file_name[260] = {};
    char alternate_file_name[14] = {};
    std::uint8_t padding[2] = {};
};
static_assert(sizeof(GuestFindData) == 320);

// A pattern's match: '*' any run, '?' any one character, letters compared
// without case; "*" and "*.*" match everything.
bool MatchesFindPattern(std::string_view pattern, std::string_view name);

// DosDateTimeToFileTime: the FILETIME (100 ns since 1601, no time zone
// applied) of a DOS date and time word, or nothing for a date or time that
// is not a real one.
std::optional<std::uint64_t> DosDateTimeToFileTime(std::uint16_t date, std::uint16_t time);

// An image entry as WIN32_FIND_DATAA: directory or normal, its size, its
// times where the entry has a date (a zero date leaves that time zero), and
// its long name, truncated to fit.
GuestFindData DescribeFindEntry(const Fat32Entry& entry);

// The attributes FindFirstFileA and GetFileAttributesA both give an entry.
std::uint32_t EntryAttributes(bool directory);

inline constexpr std::uint32_t kInvalidFileAttributes = 0xFFFFFFFFU;

// What a host's file lookup finds at a '/'-separated path below the root.
enum class GuestEntryKind
{
    kMissing,
    kFile,
    kDirectory,
};

struct GuestFileAttributes
{
    std::uint32_t attributes = kInvalidFileAttributes;
    // 0 on success, else the Win32 error GetFileAttributesA sets.
    std::uint32_t error = 0;
};

// GetFileAttributesA for a path already resolved to its components below the
// root (none for the root itself), walking them one at a time as Windows 11
// does: a missing last component is ERROR_FILE_NOT_FOUND, a missing or file
// earlier one ERROR_PATH_NOT_FOUND, and a file named with a trailing
// separator ERROR_DIRECTORY. Name syntax (an empty name, forbidden
// characters) is the caller's to check first.
GuestFileAttributes DescribeGuestFileAttributes(const std::vector<std::string>& below_root,
                                                bool trailing_separator,
                                                const std::function<GuestEntryKind(const std::string&)>& lookup);

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_GUEST_FIND_H_
