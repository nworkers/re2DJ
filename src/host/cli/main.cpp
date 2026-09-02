// Command-line host for re2DJ.
//
// The original HDD contents arrive as a directory path, never as a disk image
// and never from a fixed location inside the repository. Everything this entry
// point does is orchestration: it validates the directory, scans it, resolves a
// target profile, and enters an available platform execution backend.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/storage/guest_path.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/target/target_profile.h"
#include "re2dj/version.h"

#if defined(__linux__)
#include "re2dj/platform/linux/original_runner.h"
#elif defined(_WIN32)
#include "re2dj/platform/windows/original_process_backend.h"
#endif

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
    std::filesystem::path linux_helper;
    std::filesystem::path io_config;
    std::string target_id;
    std::string resolve_path;
    float audio_gain_db = 0.0f;
    unsigned demo_volume = 3;
    bool audio_gain_explicit = false;
    bool demo_volume_explicit = false;
    bool fullscreen_explicit = false;
    bool audio_volume_trace = false;
    bool fullscreen = false;
    bool list_targets = false;
    bool run = false;
    bool positional_target = false;
    bool target_option_explicit = false;
    bool show_help = false;
    bool show_version = false;
};

bool FindChdImage(const std::filesystem::path& input,
                  std::filesystem::path* image,
                  std::string* error)
{
    if (image == nullptr || error == nullptr || input.empty())
    {
        if (error != nullptr)
        {
            *error = "CHD path is empty";
        }
        return false;
    }
    std::error_code code;
    if (std::filesystem::is_regular_file(input, code))
    {
        if (re2dj::storage::EqualsIgnoreAsciiCase(input.extension().string(), ".chd"))
        {
            *image = std::filesystem::weakly_canonical(input, code);
            if (code)
            {
                *image = input;
            }
            return true;
        }
        *error = "CHD input is a regular file but does not have a .chd extension";
        return false;
    }
    if (code || !std::filesystem::is_directory(input, code))
    {
        *error = "CHD input directory does not exist: " + input.string();
        return false;
    }
    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator iterator(input, code), end;
         !code && iterator != end;
         iterator.increment(code))
    {
        if (!iterator->is_regular_file(code) || code)
        {
            continue;
        }
        const std::string extension = iterator->path().extension().string();
        if (re2dj::storage::EqualsIgnoreAsciiCase(extension, ".chd"))
        {
            candidates.push_back(iterator->path());
        }
    }
    if (code || candidates.empty())
    {
        *error = "no .chd image was found under " + input.string();
        return false;
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > 1)
    {
        *error = "more than one .chd image was found under " + input.string();
        return false;
    }
    *image = std::filesystem::weakly_canonical(candidates.front(), code);
    if (code)
    {
        *image = candidates.front();
    }
    return true;
}

bool PrepareChdStaging(const re2dj::storage::Fat32Volume& volume,
                       const std::string& executable_relative_path,
                       std::filesystem::path* staging_root,
                       std::string* error)
{
    if (staging_root == nullptr || error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "invalid CHD staging output";
        }
        return false;
    }
    std::error_code code;
    const auto base = std::filesystem::temp_directory_path(code);
    if (code)
    {
        *error = "cannot determine temporary directory for CHD staging";
        return false;
    }
    const std::filesystem::path root = base / "re2dj" / "chd" / "ez2dj4th";
    std::filesystem::create_directories(root / "EZ2DJ" / "BG", code);
    std::filesystem::create_directories(root / "EZ2DJ" / "SOUND", code);
    std::filesystem::create_directories(root / "EZ2DJ" / "SYSTEM", code);
    if (code)
    {
        *error = "cannot create CHD staging directory: " + code.message();
        return false;
    }
    const std::vector<std::pair<std::string, std::filesystem::path>> files = {
        {executable_relative_path, root / "EZ2DJ" / "EZ2DJ.EXE"},
        {"EZ2DJ/EZ2DJ.INI", root / "EZ2DJ" / "EZ2DJ.INI"},
        {"EZ2DJ/FONTKR.DAT", root / "EZ2DJ" / "FONTKR.DAT"},
        {"EZ2DJ/FONTEN.DAT", root / "EZ2DJ" / "FONTEN.DAT"},
    };
    for (const auto& [source, destination] : files)
    {
        if (!volume.MaterializeFile(source, destination, error))
        {
            return false;
        }
    }
    *staging_root = root;
    return true;
}

int ResolveOneChdPath(const re2dj::storage::Fat32Volume& volume,
                      const std::string& text)
{
    re2dj::storage::GuestPath parsed;
    if (!re2dj::storage::ParseGuestPath(text, &parsed) ||
        !re2dj::storage::NormalizeGuestPath(&parsed))
    {
        std::fprintf(stderr, "error: cannot parse guest path '%s'\n", text.c_str());
        return kExitUsage;
    }
    if (parsed.drive_letter != 'D' || parsed.components.empty() ||
        !re2dj::storage::EqualsIgnoreAsciiCase(parsed.components.front(), "ez2dj"))
    {
        std::fprintf(stderr, "error: CHD guest path must be under D:\\ez2dj\n");
        return kExitUsage;
    }
    const std::string guest_path = re2dj::storage::GuestPathToString(parsed);
    parsed.components.erase(parsed.components.begin());
    const std::string relative = "EZ2DJ/" +
                                  re2dj::storage::GuestPathToRelativeString(parsed);
    re2dj::storage::Fat32Entry entry;
    std::string error;
    const bool found = volume.Find(relative, &entry, &error);
    std::printf("guest path : %s\n", guest_path.c_str());
    std::printf("relative   : %s\n", relative.c_str());
    std::printf("chd path   : %s\n", found ? relative.c_str() : "<not found>");
    return found ? kExitOk : kExitHddError;
}

void PrintUsage()
{
    std::printf(
        "re2DJ %s - run the original EZ2DJ executable on modern hosts\n"
        "\n"
        "Usage:\n"
        "  re2dj <profile-id> [options]\n"
        "  re2dj --hdd <directory> [options]\n"
        "\n"
        "Options:\n"
        "  --hdd <directory>   Extracted original HDD contents. For a CHD profile,\n"
        "                      this may instead be a directory containing one .chd.\n"
        "  --target <id>       Target profile to select. Defaults to the first\n"
        "                      detected candidate.\n"
        "  --list-targets      List target profiles found in the directory.\n"
        "  --resolve <path>    Resolve one guest path (for example\n"
        "                      \"C:\\\\EZ2DJ\\\\DATA\\\\SONG.EZ\") and exit.\n"
        "  --run               Start the selected guest executable.\n"
        "  --linux-helper <path>\n"
        "                      32-bit native helper used by --run on Linux.\n"
        "  --audio-gain-db <dB>\n"
        "                      Windows output gain (-24..+18, default 0).\n"
        "  --demo-volume <0..3>\n"
        "                      Windows title/demo profile (default 3 = 0 dB).\n"
        "  --audio-volume-trace\n"
        "                      Record bounded DirectSound/WINMM volume evidence.\n"
        "  --fullscreen        Use monitor-sized borderless fullscreen on Windows.\n"
        "  --windowed          Override a profile's fullscreen default on Windows.\n"
        "  --io-config <path>  Windows EZ2DJ keyboard I/O mapping INI.\n"
        "  --version           Print the version and exit.\n"
        "  --help              Print this message and exit.\n"
        "\n"
        "The HDD directory is read only. Supported execution paths route\n"
        "guest writes to a separate overlay directory.\n",
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
        else if (argument == "--linux-helper")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            options->linux_helper = std::filesystem::path(value);
        }
        else if (argument == "--audio-gain-db")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            try
            {
                std::size_t parsed = 0;
                options->audio_gain_db = std::stof(value, &parsed);
                if (parsed != value.size() || !std::isfinite(options->audio_gain_db) ||
                    options->audio_gain_db < -24.0f || options->audio_gain_db > 18.0f)
                {
                    throw std::out_of_range("audio gain");
                }
            }
            catch (const std::exception&)
            {
                std::fprintf(stderr,
                             "error: --audio-gain-db must be between -24 and +18 dB\n");
                return false;
            }
            options->audio_gain_explicit = true;
        }
        else if (argument == "--audio-volume-trace")
        {
            options->audio_volume_trace = true;
        }
        else if (argument == "--demo-volume")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            try
            {
                std::size_t parsed = 0;
                const unsigned long parsed_value = std::stoul(value, &parsed);
                if (parsed != value.size() || parsed_value > 3)
                {
                    throw std::out_of_range("demo volume");
                }
                options->demo_volume = static_cast<unsigned>(parsed_value);
            }
            catch (const std::exception&)
            {
                std::fprintf(stderr, "error: --demo-volume must be between 0 and 3\n");
                return false;
            }
            options->demo_volume_explicit = true;
        }
        else if (argument == "--fullscreen")
        {
            options->fullscreen = true;
            options->fullscreen_explicit = true;
        }
        else if (argument == "--windowed")
        {
            options->fullscreen = false;
            options->fullscreen_explicit = true;
        }
        else if (argument == "--io-config")
        {
            std::string value;
            if (!TakeValue(argc, argv, &index, argument, &value))
            {
                return false;
            }
            options->io_config = std::filesystem::path(value);
        }
        else if (argument == "--target")
        {
            if (!TakeValue(argc, argv, &index, argument, &options->target_id))
            {
                return false;
            }
            options->target_option_explicit = true;
        }
        else if (argument == "--resolve")
        {
            if (!TakeValue(argc, argv, &index, argument, &options->resolve_path))
            {
                return false;
            }
        }
        else if (!argument.empty() && argument.front() != '-')
        {
            if (options->positional_target)
            {
                std::fprintf(stderr, "error: only one profile id may be specified\n");
                return false;
            }
            options->positional_target = true;
            if (!options->target_option_explicit)
            {
                options->target_id = std::string(argument);
            }
            options->run = true;
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
    std::printf("  %c %-22s %-24s %s%s\n",
                selected ? '*' : ' ',
                profile.id.c_str(),
                profile.executable_relative_path.c_str(),
                profile.detected ? "detected" : "built-in",
                profile.bring_up_target ? ", bring-up only" : "");
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

int RunChdTarget(const Options& options,
                 const std::filesystem::path& chd_path,
                 const re2dj::target::BuiltInTargetProfile& built_in)
{
    std::unique_ptr<re2dj::storage::Fat32Volume> volume;
    std::string error;
    if (!re2dj::storage::Fat32Volume::Open(chd_path, &volume, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return kExitHddError;
    }
    constexpr std::string_view kExecutablePath = "EZ2DJ/EZ2DJ.EXE";
    re2dj::storage::Fat32Entry executable_entry;
    if (!volume->Find(kExecutablePath, &executable_entry, &error) || executable_entry.directory)
    {
        std::fprintf(stderr, "error: CHD does not contain %.*s: %s\n",
                     static_cast<int>(kExecutablePath.size()),
                     kExecutablePath.data(),
                     error.c_str());
        return kExitHddError;
    }
    std::vector<std::uint8_t> executable_bytes;
    if (!volume->ReadFile(kExecutablePath, &executable_bytes, &error))
    {
        std::fprintf(stderr, "error: cannot read %.*s from CHD: %s\n",
                     static_cast<int>(kExecutablePath.size()),
                     kExecutablePath.data(),
                     error.c_str());
        return kExitHddError;
    }
    re2dj::exe::PeImageInfo executable_info;
    if (!re2dj::exe::ReadPeImageInfo(executable_bytes.data(),
                                     executable_bytes.size(),
                                     &executable_info,
                                     &error) ||
        !re2dj::exe::IsGuestExecutable(executable_info))
    {
        std::fprintf(stderr, "error: CHD executable is not a PE32 guest image: %s\n",
                     error.empty() ? "unexpected executable format" : error.c_str());
        return kExitHddError;
    }

    re2dj::target::TargetProfile profile = built_in.profile;
    profile.executable_relative_path = std::string(kExecutablePath);
    profile.working_directory_relative_path = "EZ2DJ";
    profile.detected = false;

    std::printf("chd image   : %s\n", chd_path.string().c_str());
    std::printf("filesystem  : FAT32 label=%s data_lba=%llu clusters=%u\n",
                volume->info().volume_label.c_str(),
                static_cast<unsigned long long>(volume->info().data_lba),
                volume->info().cluster_count);
    std::printf("scanned     : CHD-backed FAT32 lookup (directory materialization deferred)\n");
    std::printf("executables : 1 selected profile executable\n");
    if (!options.resolve_path.empty())
    {
        return ResolveOneChdPath(*volume, options.resolve_path);
    }

    std::printf("\ntargets:\n");
    PrintProfile(profile, true);
    if (options.list_targets)
    {
        return kExitOk;
    }
    std::printf("\nselected target : %s\n", profile.id.c_str());
    std::printf("display name    : %s\n", profile.display_name.c_str());
    std::printf("executable      : %s\n", profile.executable_relative_path.c_str());
    std::printf("working dir     : %s\n", profile.working_directory_relative_path.c_str());
    std::printf("guest path      : <not known for this dump>\n");
    std::printf("format hint     : %s\n",
                std::string(re2dj::target::ExecutableFormatHintName(profile.format_hint)).c_str());
    std::printf("machine         : %s\n",
                std::string(re2dj::exe::MachineName(executable_info.machine)).c_str());
    std::printf("magic           : %s\n",
                std::string(re2dj::exe::MagicName(executable_info.magic)).c_str());
    std::printf("subsystem       : %s\n",
                std::string(re2dj::exe::SubsystemName(executable_info.subsystem)).c_str());
    std::printf("image base      : 0x%08llx\n",
                static_cast<unsigned long long>(executable_info.image_base));
    std::printf("entry point rva : 0x%08x\n", executable_info.entry_point_rva);
    const std::string entry_section(re2dj::exe::EntryPointSectionName(executable_info));
    std::printf("entry section   : %s%s\n",
                entry_section.empty() ? "<outside every section>" : entry_section.c_str(),
                re2dj::exe::HasEntryPointOutsideTextSection(executable_info)
                    ? "  (outside .text - likely a protection stub)"
                    : "");
    std::printf("sections        : %u\n",
                static_cast<unsigned>(executable_info.sections.size()));
    if (!profile.note.empty())
    {
        std::printf("\nnote: %s\n", profile.note.c_str());
    }
    if (!options.run)
    {
        std::printf("\nNothing was executed. Pass --run to stage the PE and enter the available execution backend.\n");
        return kExitOk;
    }

#if defined(_WIN32)
    std::filesystem::path staging_root;
    if (!PrepareChdStaging(*volume, profile.executable_relative_path, &staging_root, &error))
    {
        std::fprintf(stderr, "error: cannot stage CHD executable: %s\n", error.c_str());
        return kExitHddError;
    }
    re2dj::platform::windows::OriginalProcessOptions run_options;
    run_options.hdd_directory = staging_root;
    run_options.chd_image = chd_path;
    run_options.target_id = profile.id;
    run_options.executable_relative_path = profile.executable_relative_path;
    run_options.hle_profile_id = profile.hle_profile_id;
    run_options.profile_defaults = profile.run_defaults;
    if (options.audio_gain_explicit)
    {
        run_options.profile_defaults.audio_gain_db = options.audio_gain_db;
    }
    if (options.demo_volume_explicit)
    {
        run_options.profile_defaults.demo_volume = options.demo_volume;
    }
    if (options.fullscreen_explicit)
    {
        run_options.profile_defaults.fullscreen = options.fullscreen;
    }
    run_options.audio_volume_trace = options.audio_volume_trace;
    run_options.io_config = options.io_config;
    const int result = re2dj::platform::windows::RunOriginalProcess(run_options, &error);
    if (result < 0)
    {
        std::fprintf(stderr, "error: Windows execution failed: %s\n", error.c_str());
        return kExitNotImplemented;
    }
    return result;
#else
    std::fprintf(stderr,
                 "error: CHD-backed original-process execution is currently connected only to the Windows x86 launcher\n");
    return kExitNotImplemented;
#endif
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
#if !defined(_WIN32)
    if (options.audio_gain_explicit || options.demo_volume_explicit ||
        options.audio_volume_trace || options.fullscreen_explicit ||
        !options.io_config.empty())
    {
        std::fprintf(stderr, "error: selected execution options are currently supported only on Windows\n");
        return kExitNotImplemented;
    }
#endif
    const re2dj::target::BuiltInTargetProfile* shortcut =
        options.target_id.empty()
            ? nullptr
            : re2dj::target::FindBuiltInTargetProfileById(options.target_id);
    if (shortcut != nullptr &&
        shortcut->profile.run_defaults.hdd_input_kind ==
            re2dj::target::HddInputKind::kMameChd)
    {
        const std::filesystem::path input =
            options.hdd_directory.empty()
                ? std::filesystem::current_path() /
                      shortcut->profile.run_defaults.default_hdd_image_relative_path
                : options.hdd_directory;
        std::filesystem::path chd_path;
        std::string chd_error;
        if (!FindChdImage(input, &chd_path, &chd_error))
        {
            std::fprintf(stderr, "error: %s\n", chd_error.c_str());
            return kExitHddError;
        }
        return RunChdTarget(options, chd_path, *shortcut);
    }
    if (options.hdd_directory.empty() && !options.target_id.empty())
    {
        const re2dj::target::BuiltInTargetProfile* directory_shortcut =
            re2dj::target::FindBuiltInTargetProfileById(options.target_id);
        if (directory_shortcut == nullptr ||
            directory_shortcut->profile.run_defaults.default_hdd_directory_relative_path.empty())
        {
            std::fprintf(stderr,
                         "error: profile '%s' has no default HDD directory; pass --hdd <directory>\n",
                         options.target_id.c_str());
            return kExitUsage;
        }
        options.hdd_directory = std::filesystem::current_path() /
                                directory_shortcut->profile.run_defaults
                                    .default_hdd_directory_relative_path;
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
        re2dj::target::BuildTargetProfiles(root, scan);

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
    std::printf("display name    : %s\n", selected->display_name.c_str());
    std::printf("executable      : %s\n", selected->executable_relative_path.c_str());
    std::printf("working dir     : %s\n",
                selected->working_directory_relative_path.empty()
                    ? "<hdd root>"
                    : selected->working_directory_relative_path.c_str());
    if (selected->guest_drive_letter != '\0')
    {
        std::printf("guest path      : %c:%s\n",
                    selected->guest_drive_letter,
                    selected->guest_directory.c_str());
    }
    else
    {
        std::printf("guest path      : <not known for this dump>\n");
    }
    std::printf("format hint     : %s\n",
                std::string(re2dj::target::ExecutableFormatHintName(selected->format_hint))
                    .c_str());

    const re2dj::hdd::ExecutableEntry* selected_entry = nullptr;
    for (const re2dj::hdd::ExecutableEntry& entry : scan.executables)
    {
        if (entry.relative_path != selected->executable_relative_path)
        {
            continue;
        }
        selected_entry = &entry;
        const re2dj::exe::PeImageInfo& info = entry.pe_info;
        std::printf("machine         : %s\n", std::string(re2dj::exe::MachineName(info.machine)).c_str());
        std::printf("magic           : %s\n", std::string(re2dj::exe::MagicName(info.magic)).c_str());
        std::printf("subsystem       : %s\n",
                    std::string(re2dj::exe::SubsystemName(info.subsystem)).c_str());
        std::printf("image base      : 0x%08llx\n",
                    static_cast<unsigned long long>(info.image_base));
        std::printf("entry point rva : 0x%08x\n", info.entry_point_rva);
        const std::string entry_section(re2dj::exe::EntryPointSectionName(info));
        std::printf("entry section   : %s%s\n",
                    entry_section.empty() ? "<outside every section>" : entry_section.c_str(),
                    re2dj::exe::HasEntryPointOutsideTextSection(info)
                        ? "  (outside .text - likely a protection stub)"
                        : "");
        std::printf("sections        : %u\n", static_cast<unsigned>(info.sections.size()));
        break;
    }

    if (!selected->note.empty())
    {
        std::printf("\nnote: %s\n", selected->note.c_str());
    }

    if (!options.run)
    {
        std::printf(
            "\nNothing was executed. Pass --run to enter the available execution\n"
            "backend, or use re2dj_pe_analyzer for a full header dump.\n");
        return kExitOk;
    }

#if defined(__linux__)
    if (options.linux_helper.empty())
    {
        std::fprintf(stderr,
                     "\nerror: --linux-helper <path> is required with --run on Linux.\n");
        return kExitUsage;
    }
    if (selected_entry == nullptr)
    {
        std::fprintf(stderr, "\nerror: selected executable metadata is unavailable.\n");
        return kExitHddError;
    }

    std::filesystem::path executable_path;
    if (!root.ResolveFile(selected->executable_relative_path, &executable_path))
    {
        std::fprintf(stderr, "\nerror: selected executable is no longer available.\n");
        return kExitHddError;
    }

    re2dj::platform::linux::OriginalRunResult run_result;
    if (!re2dj::platform::linux::RunOriginalUntilBoundary(executable_path,
                                                           selected_entry->pe_info,
                                                           options.linux_helper,
                                                           &run_result,
                                                           &error))
    {
        std::fprintf(stderr, "\nerror: Linux execution failed: %s\n", error.c_str());
        return kExitNotImplemented;
    }

    std::printf("\nload base       : 0x%08x\n", run_result.load_base.value());
    std::printf("entry point     : 0x%08x\n", run_result.entry_point.value());
    switch (run_result.boundary)
    {
    case re2dj::platform::linux::OriginalRunBoundary::kImportGate:
        if (run_result.by_ordinal)
        {
            std::printf("first boundary  : import %s!#%u\n",
                        run_result.module.c_str(),
                        static_cast<unsigned>(run_result.ordinal));
        }
        else
        {
            std::printf("first boundary  : import %s!%s\n",
                        run_result.module.c_str(),
                        run_result.name.c_str());
        }
        std::printf("gate / eip / esp: 0x%08x / 0x%08x / 0x%08x\n",
                    run_result.gate_address.value(),
                    run_result.instruction_pointer.value(),
                    run_result.stack_pointer.value());
        std::fprintf(stderr,
                     "execution stopped cleanly at the first unimplemented Win32 import.\n");
        return kExitNotImplemented;
    case re2dj::platform::linux::OriginalRunBoundary::kProcessExit:
        std::printf("first boundary  : process exit (guest status 0x%08x)\n",
                    run_result.status_code);
        return kExitOk;
    case re2dj::platform::linux::OriginalRunBoundary::kFault:
        std::fprintf(stderr,
                     "first boundary  : guest fault (host signal/status %u, eip 0x%08x)\n",
                     run_result.status_code,
                     run_result.instruction_pointer.value());
        return kExitNotImplemented;
    case re2dj::platform::linux::OriginalRunBoundary::kStopped:
        std::fprintf(stderr, "first boundary  : guest stopped\n");
        return kExitNotImplemented;
    }
#elif defined(_WIN32)
    if (options.fullscreen && !selected->run_defaults.hle_d3d3)
    {
        std::fprintf(stderr,
                     "\nerror: --fullscreen is not supported by profile '%s'.\n",
                     selected->id.c_str());
        return kExitNotImplemented;
    }
    if ((options.audio_gain_explicit || options.audio_volume_trace) &&
        !selected->run_defaults.hle_directsound)
    {
        std::fprintf(stderr,
                     "\nerror: audio options are not supported by profile '%s'.\n",
                     selected->id.c_str());
        return kExitNotImplemented;
    }
    if (options.demo_volume_explicit && !selected->run_defaults.demo_volume.has_value())
    {
        std::fprintf(stderr,
                     "\nerror: --demo-volume is not supported by profile '%s'.\n",
                     selected->id.c_str());
        return kExitNotImplemented;
    }
    if (!options.io_config.empty() && !selected->run_defaults.lptdi.legacy_io_ports)
    {
        std::fprintf(stderr,
                     "\nerror: --io-config is not supported by profile '%s'.\n",
                     selected->id.c_str());
        return kExitNotImplemented;
    }
    re2dj::platform::windows::OriginalProcessOptions run_options;
    run_options.hdd_directory = root.root();
    run_options.target_id = selected->id;
    run_options.hle_profile_id = selected->hle_profile_id;
    run_options.profile_defaults = selected->run_defaults;
    if (options.audio_gain_explicit)
    {
        run_options.profile_defaults.audio_gain_db = options.audio_gain_db;
    }
    if (options.demo_volume_explicit)
    {
        run_options.profile_defaults.demo_volume = options.demo_volume;
    }
    if (options.fullscreen_explicit)
    {
        run_options.profile_defaults.fullscreen = options.fullscreen;
    }
    run_options.audio_volume_trace = options.audio_volume_trace;
    run_options.io_config = options.io_config;
    const int run_result =
        re2dj::platform::windows::RunOriginalProcess(run_options, &error);
    if (run_result < 0)
    {
        std::fprintf(stderr, "\nerror: Windows execution failed: %s\n", error.c_str());
        return kExitNotImplemented;
    }
    return run_result;
#else
    std::fprintf(stderr,
                 "\nerror: --run is not connected to an execution backend on this host.\n");
    return kExitNotImplemented;
#endif
}
