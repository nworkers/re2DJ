#ifndef RE2DJ_TOOLS_CHD_PROBE_CHD_EXTRACT_H_
#define RE2DJ_TOOLS_CHD_PROBE_CHD_EXTRACT_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "re2dj/storage/fat32_chd.h"

namespace re2dj::tools
{

// Converts one FAT32 entry name into a host path component. FAT long names
// decode to UTF-8, and a plain std::string reaches std::filesystem::path as the
// host's narrow encoding instead, which on Windows is an ANSI code page that
// cannot represent every name an original image carries.
std::filesystem::path ChdEntryHostName(std::string_view utf8_name);

struct ChdExtractStats
{
    std::size_t directories = 0;
    std::size_t files = 0;
    std::size_t failures = 0;
    std::uint64_t bytes = 0;
};

// Writes one directory of a read-only FAT32 CHD volume, and everything below
// it, into `output`. `inner_path` names the starting directory inside the
// image with '/' or '\' separators; empty means the volume root.
//
// A file the volume cannot read is counted in `stats->failures` and reported
// through `report_failure`, and the walk continues: original images do carry
// damaged entries, and one of them must not cost the caller the rest of the
// tree. The return value is false only when the walk could not start or a
// host directory could not be created, so a caller distinguishes "extracted
// with holes" from "extracted nothing".
//
// `report_directory` is called once per directory entered, and is the only
// progress reporting: a line per file is unreadable across a whole image.
// Either callback may be empty.
bool ExtractChdDirectory(
    const storage::Fat32Volume& volume,
    std::string_view inner_path,
    const std::filesystem::path& output,
    const std::function<void(std::string_view inner, std::size_t entries)>& report_directory,
    const std::function<void(std::string_view inner, std::string_view error)>& report_failure,
    ChdExtractStats* stats,
    std::string* error);

}  // namespace re2dj::tools

#endif  // RE2DJ_TOOLS_CHD_PROBE_CHD_EXTRACT_H_
