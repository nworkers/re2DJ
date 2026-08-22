#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/hdd/hdd_scan.h"
#include "re2dj/platform/windows/native_helper_backend.h"
#include "re2dj/target/target_profile.h"

namespace
{

bool ReadFile(const std::filesystem::path& path,
              std::vector<std::uint8_t>* bytes,
              std::string* error)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        *error = "cannot open executable";
        return false;
    }
    const std::streamoff length = stream.tellg();
    if (length <= 0 || static_cast<std::uintmax_t>(length) >
                           (std::numeric_limits<std::size_t>::max)())
    {
        *error = "invalid executable size";
        return false;
    }
    bytes->resize(static_cast<std::size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes->data()),
                static_cast<std::streamsize>(bytes->size()));
    if (!stream)
    {
        *error = "cannot read executable";
        return false;
    }
    return true;
}

void PrintUsage()
{
    std::printf("Usage: re2dj_windows_import_observer --hdd <directory> [--target <id>] [--helper <win32-helper>]\n");
}

bool FindBundledHelper(std::filesystem::path* helper_path, std::string* error)
{
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr,
                                            buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
    {
        *error = "cannot determine observer executable path";
        return false;
    }
    const std::filesystem::path directory =
        std::filesystem::path(buffer.data()).parent_path();
    const std::filesystem::path candidates[] = {
        directory / L"re2dj_native_ipc_helper.exe",
        directory / L"helpers" / L"win32" / L"re2dj_native_ipc_helper.exe",
    };
    for (const std::filesystem::path& candidate : candidates)
    {
        if (std::filesystem::is_regular_file(candidate))
        {
            *helper_path = candidate;
            return true;
        }
    }
    *error = "cannot find bundled re2dj_native_ipc_helper.exe";
    return false;
}

}  // namespace

int main(int argc, char** argv)
{
    std::filesystem::path hdd_path;
    std::filesystem::path helper_path;
    std::string target_id = "ez2dj1stse_unpacked";
    for (int index = 1; index < argc; ++index)
    {
        const std::string option = argv[index];
        if (option == "--hdd" && index + 1 < argc)
        {
            hdd_path = argv[++index];
        }
        else if (option == "--helper" && index + 1 < argc)
        {
            helper_path = argv[++index];
        }
        else if (option == "--target" && index + 1 < argc)
        {
            target_id = argv[++index];
        }
        else
        {
            PrintUsage();
            return 1;
        }
    }
    if (hdd_path.empty())
    {
        PrintUsage();
        return 1;
    }

    re2dj::hdd::HddRoot root;
    std::string error;
    if (!re2dj::hdd::HddRoot::Open(hdd_path, &root, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 2;
    }
    if (helper_path.empty() && !FindBundledHelper(&helper_path, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 2;
    }
    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(root, scan);
    const re2dj::target::TargetProfile* target =
        re2dj::target::FindTargetProfileById(profiles, target_id);
    if (target == nullptr || !target->bring_up_target)
    {
        std::fprintf(stderr, "{\"error\":\"bring-up target not found\"}\n");
        return 2;
    }
    std::filesystem::path executable;
    std::vector<std::uint8_t> bytes;
    if (!root.ResolveFile(target->executable_relative_path, &executable) ||
        !ReadFile(executable, &bytes, &error))
    {
        std::fprintf(stderr, "{\"error\":\"cannot resolve or read bring-up target\"}\n");
        return 2;
    }
    re2dj::exe::PeImageInfo info;
    if (!re2dj::exe::ReadPeImageInfo(bytes.data(), bytes.size(), &info, &error) ||
        !re2dj::exe::IsGuestExecutable(info) ||
        info.image_base > (std::numeric_limits<std::uint32_t>::max)())
    {
        std::fprintf(stderr, "{\"error\":\"invalid PE32 bring-up target\"}\n");
        return 2;
    }
    re2dj::platform::windows::NativeHelperBackend backend(helper_path);
    re2dj::runtime::LoadedPeImage loaded;
    const re2dj::runtime::GuestAddress base(static_cast<std::uint32_t>(info.image_base));
    if (!backend.PrepareImage(bytes, info, base, &loaded, &error) ||
        !backend.Start(&error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        return 3;
    }
    re2dj::runtime::ExecutionEvent event;
    if (!backend.WaitForEvent(&event, &error))
    {
        std::fprintf(stderr, "{\"error\":\"%s\"}\n", error.c_str());
        backend.RequestStop();
        return 3;
    }
    if (event.kind != re2dj::runtime::ExecutionEventKind::kImportGate)
    {
        std::fprintf(stderr, "{\"kind\":\"non-import\",\"event_id\":%llu,\"status\":%u}\n", static_cast<unsigned long long>(event.event_id), event.status_code);
        backend.RequestStop();
        return 4;
    }
    const auto gate = std::find_if(loaded.imports.begin(), loaded.imports.end(),
        [&event](const re2dj::runtime::ImportGate& candidate) { return candidate.address == event.gate_address; });
    std::printf("{\"event_id\":%llu,\"ip\":\"0x%08x\",\"sp\":\"0x%08x\",\"module\":\"%s\",\"name\":\"%s\",\"ordinal\":%u}\n", static_cast<unsigned long long>(event.event_id), event.instruction_pointer.value(), event.stack_pointer.value(), gate == loaded.imports.end() ? "" : gate->module.c_str(), gate == loaded.imports.end() ? "" : gate->name.c_str(), gate == loaded.imports.end() ? 0U : static_cast<unsigned>(gate->ordinal));
    re2dj::runtime::ImportCompletion completion;
    completion.event_id = event.event_id;
    completion.action = re2dj::runtime::ImportCompletionAction::kStop;
    backend.CompleteImport(completion, &error);
    return 0;
}
