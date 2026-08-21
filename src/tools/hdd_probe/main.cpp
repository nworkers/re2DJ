// Non-executing probe for a user-supplied EZ2DJ HDD directory.
//
// It answers the first question the project has to answer about any dump: what
// is in there, and which file is the game. Nothing is loaded or run.

#include <cstdio>
#include <filesystem>
#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/target/target_profile.h"
#include "re2dj/version.h"

namespace
{

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitHddError = 2;

void PrintUsage()
{
    std::printf(
        "re2dj_hdd_probe %s - inspect an extracted EZ2DJ HDD directory\n"
        "\n"
        "Usage:\n"
        "  re2dj_hdd_probe <directory> [--depth <n>]\n",
        std::string(re2dj::VersionString()).c_str());
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return kExitUsage;
    }

    std::filesystem::path directory;
    re2dj::hdd::HddScanOptions options;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return kExitOk;
        }
        if (argument == "--depth")
        {
            if (index + 1 >= argc)
            {
                std::fprintf(stderr, "error: --depth requires a value\n");
                return kExitUsage;
            }
            options.max_depth = static_cast<std::size_t>(std::stoul(argv[++index]));
            continue;
        }
        if (!directory.empty())
        {
            std::fprintf(stderr, "error: unexpected argument '%s'\n", argv[index]);
            return kExitUsage;
        }
        directory = std::filesystem::path(argument);
    }

    if (directory.empty())
    {
        std::fprintf(stderr, "error: a directory path is required\n");
        return kExitUsage;
    }

    re2dj::hdd::HddRoot root;
    std::string error;
    if (!re2dj::hdd::HddRoot::Open(directory, &root, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return kExitHddError;
    }

    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root, options);

    std::printf("root        : %s\n", scan.root.string().c_str());
    std::printf("directories : %zu\n", scan.directory_count);
    std::printf("files       : %zu\n", scan.file_count);
    std::printf("truncated   : %s\n", scan.truncated ? "yes" : "no");
    std::printf("executables : %zu\n\n", scan.executables.size());

    for (const re2dj::hdd::ExecutableEntry& entry : scan.executables)
    {
        std::printf("%s\n", entry.relative_path.c_str());
        std::printf("    size      : %llu bytes\n",
                    static_cast<unsigned long long>(entry.file_size));
        if (!entry.pe_readable)
        {
            std::printf("    pe        : unreadable (%s)\n", entry.pe_error.c_str());
            continue;
        }
        const re2dj::exe::PeImageInfo& info = entry.pe_info;
        std::printf("    pe        : %s %s %s%s\n",
                    std::string(re2dj::exe::MagicName(info.magic)).c_str(),
                    std::string(re2dj::exe::MachineName(info.machine)).c_str(),
                    std::string(re2dj::exe::SubsystemName(info.subsystem)).c_str(),
                    info.is_dll ? " dll" : "");
        std::printf("    base/entry: 0x%08llx / 0x%08x\n",
                    static_cast<unsigned long long>(info.image_base),
                    info.entry_point_rva);
        std::printf("    guest fmt : %s\n",
                    re2dj::exe::IsGuestExecutable(info) ? "yes" : "no");
        const std::string entry_section(re2dj::exe::EntryPointSectionName(info));
        std::printf("    entry sect: %s%s\n",
                    entry_section.empty() ? "<none>" : entry_section.c_str(),
                    re2dj::exe::HasEntryPointOutsideTextSection(info)
                        ? "  (outside .text - likely a protection stub)"
                        : "");
    }

    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(root, scan);
    std::printf("\ntarget profiles: %zu\n", profiles.size());
    for (const re2dj::target::TargetProfile& profile : profiles)
    {
        std::printf("    %-22s %-24s %s%s\n",
                    profile.id.c_str(),
                    profile.executable_relative_path.c_str(),
                    profile.detected ? "detected" : "built-in",
                    profile.bring_up_target ? ", bring-up only" : "");
    }

    return profiles.empty() ? kExitHddError : kExitOk;
}
