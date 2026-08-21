#include "re2dj/hdd/hdd_scan.h"

#include <algorithm>
#include <system_error>
#include <utility>

#include "re2dj/storage/guest_path.h"

namespace re2dj::hdd
{

namespace
{

bool HasExecutableExtension(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    return storage::EqualsIgnoreAsciiCase(extension, ".exe");
}

// Ranks an entry for the "which file is the game" question. Lower sorts first.
int CandidateRank(const ExecutableEntry& entry)
{
    if (!entry.pe_readable || !exe::IsGuestExecutable(entry.pe_info))
    {
        return 2;
    }
    return entry.pe_info.subsystem == exe::kSubsystemWindowsGui ? 0 : 1;
}

void WalkDirectory(const std::filesystem::path& directory,
                   const std::filesystem::path& root,
                   const HddScanOptions& options,
                   std::size_t depth,
                   HddScanResult* result)
{
    if (depth > options.max_depth)
    {
        result->truncated = true;
        return;
    }

    std::error_code code;
    std::filesystem::directory_iterator iterator(directory, code);
    if (code)
    {
        return;
    }

    const std::filesystem::directory_iterator end;
    for (; iterator != end; iterator.increment(code))
    {
        if (code)
        {
            return;
        }
        if (result->directory_count + result->file_count >= options.max_entries)
        {
            result->truncated = true;
            return;
        }

        const std::filesystem::directory_entry& entry = *iterator;

        // symlink_status, not status: following a link could leave the user's
        // HDD directory or loop back into it.
        const std::filesystem::file_status status = entry.symlink_status(code);
        if (code)
        {
            continue;
        }
        if (std::filesystem::is_symlink(status))
        {
            continue;
        }

        if (std::filesystem::is_directory(status))
        {
            ++result->directory_count;
            WalkDirectory(entry.path(), root, options, depth + 1, result);
            continue;
        }
        if (!std::filesystem::is_regular_file(status))
        {
            continue;
        }

        ++result->file_count;
        if (!HasExecutableExtension(entry.path()))
        {
            continue;
        }

        ExecutableEntry executable;
        executable.relative_path =
            entry.path().lexically_relative(root).generic_string();
        executable.file_size = std::filesystem::file_size(entry.path(), code);
        if (code)
        {
            executable.file_size = 0;
        }
        executable.pe_readable =
            exe::ReadPeImageInfo(entry.path(), &executable.pe_info, &executable.pe_error);
        result->executables.push_back(std::move(executable));
    }
}

}  // namespace

HddScanResult ScanHdd(const HddRoot& root, const HddScanOptions& options)
{
    HddScanResult result;
    if (!root.is_open())
    {
        return result;
    }

    result.root = root.root();
    WalkDirectory(result.root, result.root, options, 0, &result);

    const auto more_likely_first = [](const ExecutableEntry& left,
                                      const ExecutableEntry& right)
    {
        const int left_rank = CandidateRank(left);
        const int right_rank = CandidateRank(right);
        if (left_rank != right_rank)
        {
            return left_rank < right_rank;
        }
        return left.file_size > right.file_size;
    };

    std::stable_sort(result.executables.begin(),
                     result.executables.end(),
                     more_likely_first);

    return result;
}

}  // namespace re2dj::hdd
