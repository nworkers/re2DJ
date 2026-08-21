#ifndef RE2DJ_HDD_HDD_SCAN_H_
#define RE2DJ_HDD_HDD_SCAN_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"

namespace re2dj::hdd
{

struct ExecutableEntry
{
    // '/'-separated path relative to the HDD root, in the case found on disk.
    std::string relative_path;
    std::uintmax_t file_size = 0;
    // False when the file has an .exe name but no readable PE header.
    bool pe_readable = false;
    // Why the header could not be read. Empty when `pe_readable` is true.
    std::string pe_error;
    exe::PeImageInfo pe_info;
};

struct HddScanOptions
{
    // A real dump nests a few levels deep at most. The limit keeps a symlink
    // loop or an unexpectedly large tree from turning a scan into a hang.
    std::size_t max_depth = 8;
    std::size_t max_entries = 200000;
};

struct HddScanResult
{
    std::filesystem::path root;
    std::vector<ExecutableEntry> executables;
    std::size_t directory_count = 0;
    std::size_t file_count = 0;
    // True when the walk stopped early against one of the option limits, which
    // means `executables` may be incomplete.
    bool truncated = false;
};

// Walks the HDD directory and reports every .exe it finds, with PE headers
// already parsed. Entries are ordered most-likely-game-executable first:
// guest-format GUI executables, then guest-format others, then the rest, each
// group by descending file size.
HddScanResult ScanHdd(const HddRoot& root, const HddScanOptions& options = {});

}  // namespace re2dj::hdd

#endif  // RE2DJ_HDD_HDD_SCAN_H_
