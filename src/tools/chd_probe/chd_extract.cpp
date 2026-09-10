#include "chd_extract.h"

#include <fstream>
#include <ios>
#include <system_error>
#include <vector>

namespace re2dj::tools
{

namespace
{

std::string JoinInner(std::string_view directory, std::string_view name)
{
    if (directory.empty())
    {
        return std::string(name);
    }
    std::string joined(directory);
    joined.push_back('/');
    joined.append(name);
    return joined;
}

// Renders a path for a message as UTF-8. `path::string()` would convert to the
// host's narrow encoding, which cannot represent every name this walk meets.
std::string PathText(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

bool EnsureDirectory(const std::filesystem::path& path, std::string* error)
{
    std::error_code code;
    std::filesystem::create_directories(path, code);
    if (code && !std::filesystem::is_directory(path))
    {
        if (error != nullptr)
        {
            *error = "cannot create directory " + PathText(path) + ": " + code.message();
        }
        return false;
    }
    return true;
}

bool WriteHostFile(const std::filesystem::path& path,
                   const std::vector<std::uint8_t>& bytes,
                   std::string* error)
{
    // std::ofstream takes the path itself, so it opens through the native wide
    // path on Windows. std::fopen would need the narrow rendering that cannot
    // represent every name.
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        if (error != nullptr)
        {
            *error = "cannot open output file " + PathText(path);
        }
        return false;
    }
    if (!bytes.empty())
    {
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    out.close();
    if (!out)
    {
        if (error != nullptr)
        {
            *error = "cannot write output file " + PathText(path);
        }
        return false;
    }
    return true;
}

using DirectoryReporter =
    std::function<void(std::string_view inner, std::size_t entries)>;
using FailureReporter =
    std::function<void(std::string_view inner, std::string_view error)>;

// Recursion is explicit rather than iterative because a FAT32 path depth is
// bounded by the 255-character path limit the format itself imposes, so it
// cannot grow deep enough to matter.
bool ExtractInto(const storage::Fat32Volume& volume,
                 const std::string& inner,
                 const std::filesystem::path& output,
                 const DirectoryReporter& report_directory,
                 const FailureReporter& report_failure,
                 ChdExtractStats* stats,
                 std::string* error)
{
    std::vector<storage::Fat32Entry> entries;
    std::string read_error;
    if (!volume.ReadDirectory(inner, &entries, &read_error))
    {
        // A directory that cannot be read is reported like an unreadable file
        // rather than ending the walk, for the same reason.
        ++stats->failures;
        if (report_failure)
        {
            report_failure(inner, read_error);
        }
        return true;
    }

    ++stats->directories;
    if (report_directory)
    {
        report_directory(inner, entries.size());
    }

    for (const storage::Fat32Entry& entry : entries)
    {
        const std::string child_inner = JoinInner(inner, entry.name);
        const std::filesystem::path child_output = output / ChdEntryHostName(entry.name);
        if (entry.directory)
        {
            if (!EnsureDirectory(child_output, error))
            {
                return false;
            }
            if (!ExtractInto(volume,
                             child_inner,
                             child_output,
                             report_directory,
                             report_failure,
                             stats,
                             error))
            {
                return false;
            }
            continue;
        }

        std::vector<std::uint8_t> bytes;
        std::string file_error;
        if (!volume.ReadFile(child_inner, &bytes, &file_error))
        {
            ++stats->failures;
            if (report_failure)
            {
                report_failure(child_inner, file_error);
            }
            continue;
        }
        if (!WriteHostFile(child_output, bytes, &file_error))
        {
            // A host write failure is the caller's own filesystem, not a
            // property of the image, so it ends the walk.
            if (error != nullptr)
            {
                *error = file_error;
            }
            return false;
        }
        ++stats->files;
        stats->bytes += bytes.size();
    }
    return true;
}

}  // namespace

// A Korean-named directory in the ez2d2m image proved this is not merely a
// question of a mangled name: its UTF-8 bytes are not valid in the active
// Windows code page, and constructing a path from them never returned. A
// std::u8string is UTF-8 on every platform, so it converts correctly.
std::filesystem::path ChdEntryHostName(std::string_view utf8_name)
{
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8_name.data()), utf8_name.size()));
}

bool ExtractChdDirectory(const storage::Fat32Volume& volume,
                         std::string_view inner_path,
                         const std::filesystem::path& output,
                         const DirectoryReporter& report_directory,
                         const FailureReporter& report_failure,
                         ChdExtractStats* stats,
                         std::string* error)
{
    if (stats == nullptr)
    {
        if (error != nullptr)
        {
            *error = "extraction requires a statistics destination";
        }
        return false;
    }
    storage::Fat32Entry start;
    std::string find_error;
    if (!inner_path.empty() &&
        (!volume.Find(inner_path, &start, &find_error) || !start.directory))
    {
        if (error != nullptr)
        {
            *error = "cannot extract " + std::string(inner_path) + ": " +
                     (find_error.empty() ? "not a directory" : find_error);
        }
        return false;
    }
    if (!EnsureDirectory(output, error))
    {
        return false;
    }
    return ExtractInto(volume,
                       std::string(inner_path),
                       output,
                       report_directory,
                       report_failure,
                       stats,
                       error);
}

}  // namespace re2dj::tools
