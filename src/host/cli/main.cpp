// Command-line host for re2DJ.
//
// The original HDD contents arrive as a directory path, never as a disk image
// and never from a fixed location inside the repository. Everything this entry
// point does is orchestration: it validates the directory, scans it, resolves a
// target profile, and reports the result. Loading and execution belong to the
// runtime layer, which does not exist yet.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/storage/guest_path.h"
#include "re2dj/target/target_profile.h"
#include "re2dj/version.h"

namespace
{

// Exit codes. Kept distinct so scripts can tell a bad invocation from a bad
// dump from a feature that simply is not built yet.
constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitHddError = 2;
constexpr int kExitNotImplemented = 3;

struct Options
{
    std::filesystem::path hdd_directory;
    std::string target_id;
    std::string resolve_path;
    bool list_targets = false;
    bool run = false;
    bool show_help = false;
    bool show_version = false;
};

void PrintUsage()
{
    std::printf(
        "re2DJ %s - run the original EZ2DJ executable on modern hosts\n"
        "\n"
        "Usage:\n"
        "  re2dj --hdd <directory> [options]\n"
        "\n"
        "Options:\n"
        "  --hdd <directory>   Extracted original HDD contents. Required.\n"
        "  --target <id>       Target profile to select. Defaults to the first\n"
        "                      detected candidate.\n"
        "  --list-targets      List target profiles found in the directory.\n"
        "  --resolve <path>    Resolve one guest path (for example\n"
        "                      \"C:\\\\EZ2DJ\\\\DATA\\\\SONG.EZ\") and exit.\n"
        "  --run               Start the guest. Not implemented yet.\n"
        "  --version           Print the version and exit.\n"
        "  --help              Print this message and exit.\n"
        "\n"
        "The HDD directory is read only. Guest writes will go to a separate\n"
        "overlay directory once the runtime layer exists.\n",
        std::string(re2dj::VersionString()).c_str());
}

bool TakeValue(int argc, char** argv, int* index, std::string_view name, std::string* out)
{
    if (*index + 1 >= argc)
    {
        std::fprintf(stderr, "error: %s requires a value\n", std::string(name).c_str());
        return false;
    }
    ++(*index);
    *out = argv[*index];
    return true;
}

bool ParseOptions(int argc, char** argv, Options* options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            options->show_help = true;
        }
        else if (argument == "--version")
        {
            options->show_version = true;
        }
        else if (argument == "--list-targets")
        {
            options->list_targets = true;
        }
        else if (argument == "--run")
        {
            options->run = true;
        }
        else if (argument == "--hdd")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            options->hdd_directory = std::filesystem::path(value);
        }
        else if (argument == "--target")
        {
            if (!TakeValue(argc, argv, &index, argument, &options->target_id))
            {
                return false;
            }
        }
        else if (argument == "--resolve")
        {
            if (!TakeValue(argc, argv, &index, argument, &options->resolve_path))
            {
                return false;
            }
        }
        else
        {
            std::fprintf(stderr, "error: unknown argument '%s'\n", argv[index]);
            return false;
        }
    }
    return true;
}

void PrintProfile(const re2dj::target::TargetProfile& profile, bool selected)
{
    std::printf("  %c %-20s %s\n",
                selected ? '*' : ' ',
                profile.id.c_str(),
                profile.executable_relative_path.c_str());
}

int ResolveOnePath(const re2dj::hdd::HddRoot& root, const std::string& text)
{
    re2dj::storage::GuestPath parsed;
    if (!re2dj::storage::ParseGuestPath(text, &parsed))
    {
        std::fprintf(stderr, "error: cannot parse guest path '%s'\n", text.c_str());
        return kExitUsage;
    }
    if (!re2dj::storage::NormalizeGuestPath(&parsed))
    {
        std::fprintf(stderr,
                     "error: guest path '%s' escapes the HDD directory\n",
                     text.c_str());
        return kExitUsage;
    }

    const std::string relative = re2dj::storage::GuestPathToRelativeString(parsed);
    std::filesystem::path resolved;
    if (!root.Resolve(relative, &resolved))
    {
        std::printf("guest path : %s\n", re2dj::storage::GuestPathToString(parsed).c_str());
        std::printf("relative   : %s\n", relative.c_str());
        std::printf("host path  : <not found>\n");
        return kExitHddError;
    }

    std::printf("guest path : %s\n", re2dj::storage::GuestPathToString(parsed).c_str());
    std::printf("relative   : %s\n", relative.c_str());
    std::printf("host path  : %s\n", resolved.string().c_str());
    return kExitOk;
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!ParseOptions(argc, argv, &options))
    {
        return kExitUsage;
    }
    if (options.show_version)
    {
        std::printf("%s\n", std::string(re2dj::VersionString()).c_str());
        return kExitOk;
    }
    if (options.show_help || argc == 1)
    {
        PrintUsage();
        return options.show_help ? kExitOk : kExitUsage;
    }
    if (options.hdd_directory.empty())
    {
        std::fprintf(stderr, "error: --hdd <directory> is required\n");
        return kExitUsage;
    }

    re2dj::hdd::HddRoot root;
    std::string error;
    if (!re2dj::hdd::HddRoot::Open(options.hdd_directory, &root, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return kExitHddError;
    }

    if (!options.resolve_path.empty())
    {
        return ResolveOnePath(root, options.resolve_path);
    }

    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(scan);

    std::printf("hdd root   : %s\n", scan.root.string().c_str());
    std::printf("scanned    : %zu directories, %zu files%s\n",
                scan.directory_count,
                scan.file_count,
                scan.truncated ? " (truncated)" : "");
    std::printf("executables: %zu\n", scan.executables.size());

    if (profiles.empty())
    {
        std::fprintf(stderr,
                     "error: no 32-bit x86 PE32 executable found under %s\n",
                     scan.root.string().c_str());
        return kExitHddError;
    }

    const re2dj::target::TargetProfile* selected = nullptr;
    if (options.target_id.empty())
    {
        selected = &profiles.front();
    }
    else
    {
        selected = re2dj::target::FindTargetProfileById(profiles, options.target_id);
        if (selected == nullptr)
        {
            std::fprintf(stderr,
                         "error: no target profile with id '%s'\n",
                         options.target_id.c_str());
            options.list_targets = true;
        }
    }

    std::printf("\ntargets:\n");
    for (const re2dj::target::TargetProfile& profile : profiles)
    {
        PrintProfile(profile, selected == &profile);
    }

    if (selected == nullptr)
    {
        return kExitUsage;
    }
    if (options.list_targets)
    {
        return kExitOk;
    }

    std::printf("\nselected target : %s\n", selected->id.c_str());
    std::printf("executable      : %s\n", selected->executable_relative_path.c_str());
    std::printf("working dir     : %s\n",
                selected->working_directory_relative_path.empty()
                    ? "<hdd root>"
                    : selected->working_directory_relative_path.c_str());
    std::printf("format hint     : %s\n",
                std::string(re2dj::target::ExecutableFormatHintName(selected->format_hint))
                    .c_str());

    for (const re2dj::hdd::ExecutableEntry& entry : scan.executables)
    {
        if (entry.relative_path != selected->executable_relative_path)
        {
            continue;
        }
        const re2dj::exe::PeImageInfo& info = entry.pe_info;
        std::printf("machine         : %s\n", std::string(re2dj::exe::MachineName(info.machine)).c_str());
        std::printf("magic           : %s\n", std::string(re2dj::exe::MagicName(info.magic)).c_str());
        std::printf("subsystem       : %s\n",
                    std::string(re2dj::exe::SubsystemName(info.subsystem)).c_str());
        std::printf("image base      : 0x%08llx\n",
                    static_cast<unsigned long long>(info.image_base));
        std::printf("entry point rva : 0x%08x\n", info.entry_point_rva);
        std::printf("sections        : %u\n", static_cast<unsigned>(info.sections.size()));
        break;
    }

    if (!options.run)
    {
        std::printf(
            "\nNothing was executed. Pass --run once the runtime layer lands, or\n"
            "use re2dj_pe_analyzer for a full header dump.\n");
        return kExitOk;
    }

    std::fprintf(stderr,
                 "\nerror: the execution backend is not implemented yet.\n"
                 "       The loader, guest address space, and Win32 HLE layer are\n"
                 "       still design-only. See ARCHITECTURE.md sections 7 and 8.\n");
    return kExitNotImplemented;
}
