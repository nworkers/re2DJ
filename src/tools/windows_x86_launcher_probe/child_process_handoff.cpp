#define NOMINMAX
#include <windows.h>

#include "child_process_handoff.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "../../platform/windows/injected_runtime_loader.h"
#include "../../platform/windows/runtime_export_locator.h"
#include "../windows_original_process_probe/iat_verifier.h"

namespace re2dj::tools::windows_x86_launcher_probe
{
namespace
{

bool ReadWholeFile(const std::filesystem::path& path,
                   std::vector<std::uint8_t>* bytes,
                   std::string* error)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        *error = "cannot open bootstrap child executable";
        return false;
    }
    const std::streamoff size = stream.tellg();
    if (size < 0)
    {
        *error = "cannot determine bootstrap child executable size";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    bytes->resize(static_cast<std::size_t>(size));
    if (!bytes->empty())
    {
        stream.read(reinterpret_cast<char*>(bytes->data()), size);
    }
    if (!stream)
    {
        *error = "cannot read bootstrap child executable";
        return false;
    }
    return true;
}

bool WriteRemoteU32(HANDLE process,
                    std::uintptr_t address,
                    std::uint32_t value,
                    std::string* error)
{
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           &value,
                           sizeof(value),
                           &written) != FALSE &&
        written == sizeof(value))
    {
        return true;
    }
    DWORD old_protect = 0;
    if (VirtualProtectEx(process,
                         reinterpret_cast<void*>(address),
                         sizeof(value),
                         PAGE_READWRITE,
                         &old_protect) == FALSE)
    {
        *error = "cannot patch bootstrap child memory";
        return false;
    }
    written = 0;
    const bool write_succeeded =
        WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           &value,
                           sizeof(value),
                           &written) != FALSE &&
        written == sizeof(value);
    DWORD ignored_protect = 0;
    const bool restore_succeeded =
        VirtualProtectEx(process,
                         reinterpret_cast<void*>(address),
                         sizeof(value),
                         old_protect,
                         &ignored_protect) != FALSE;
    if (!write_succeeded || !restore_succeeded)
    {
        *error = !write_succeeded ? "cannot write bootstrap child memory"
                                  : "cannot restore bootstrap child memory protection";
        return false;
    }
    return true;
}

bool WriteRemoteBytes(HANDLE process,
                      std::uintptr_t address,
                      const std::uint8_t* bytes,
                      std::size_t size,
                      std::string* error)
{
    SIZE_T written = 0;
    if (bytes == nullptr || size == 0 ||
        WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           bytes,
                           size,
                           &written) == FALSE ||
        written != size)
    {
        *error = "cannot copy bootstrap child runtime data";
        return false;
    }
    return true;
}

bool WriteRemoteAnsi(HANDLE process,
                     std::uintptr_t address,
                     const std::string& value,
                     std::string* error)
{
    if (value.size() >= MAX_PATH)
    {
        *error = "bootstrap child runtime path exceeds MAX_PATH configuration buffer";
        return false;
    }
    return WriteRemoteBytes(process,
                            address,
                            reinterpret_cast<const std::uint8_t*>(value.c_str()),
                            value.size() + 1,
                            error);
}

bool FindOptionalIatSlotsByName(const re2dj::exe::PeImageInfo& info,
                                const std::vector<std::uint8_t>& file,
                                const std::string& module,
                                const std::string& function,
                                std::vector<std::uint32_t>* slots,
                                bool* present,
                                std::string* error)
{
    if (re2dj::tools::windows_original_process_probe::FindIatSlotsByName(
            info, file.data(), file.size(), module, function, slots, error))
    {
        *present = true;
        return true;
    }
    if (*error == "requested import is not present")
    {
        error->clear();
        slots->clear();
        *present = false;
        return true;
    }
    return false;
}

bool FindOptionalIatSlotByName(const re2dj::exe::PeImageInfo& info,
                               const std::vector<std::uint8_t>& file,
                               const std::string& module,
                               const std::string& function,
                               std::uint32_t* slot,
                               bool* present,
                               std::string* error)
{
    if (re2dj::tools::windows_original_process_probe::FindIatSlotByName(
            info, file.data(), file.size(), module, function, slot, error))
    {
        *present = true;
        return true;
    }
    if (*error == "requested import is not present")
    {
        error->clear();
        *present = false;
        return true;
    }
    return false;
}

bool SetSoftwareBreakpoint(HANDLE process,
                           std::uintptr_t address,
                           std::uint8_t* original_byte,
                           std::string* error)
{
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(address),
                          original_byte,
                          sizeof(*original_byte),
                          &copied) == FALSE ||
        copied != sizeof(*original_byte))
    {
        *error = "cannot read bootstrap child entry byte";
        return false;
    }
    const std::uint8_t breakpoint = 0xcc;
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           &breakpoint,
                           sizeof(breakpoint),
                           &written) == FALSE ||
        written != sizeof(breakpoint) ||
        FlushInstructionCache(process, reinterpret_cast<const void*>(address), 1) == FALSE)
    {
        *error = "cannot set bootstrap child entry breakpoint";
        return false;
    }
    return true;
}

bool RestoreSoftwareBreakpoint(HANDLE process,
                               std::uintptr_t address,
                               std::uint8_t original_byte,
                               std::string* error)
{
    SIZE_T written = 0;
    if (WriteProcessMemory(process,
                           reinterpret_cast<void*>(address),
                           &original_byte,
                           sizeof(original_byte),
                           &written) == FALSE ||
        written != sizeof(original_byte) ||
        FlushInstructionCache(process, reinterpret_cast<const void*>(address), 1) == FALSE)
    {
        *error = "cannot restore bootstrap child entry byte";
        return false;
    }
    return true;
}

}  // namespace

bool PrepareBootstrapChildProcess(const DEBUG_EVENT& create_event,
                                  const std::filesystem::path& executable,
                                  const BootstrapChildHandoffOptions& options,
                                  BootstrapChildHandoffResult* result,
                                  std::string* error)
{
    if (result == nullptr || error == nullptr ||
        create_event.dwDebugEventCode != CREATE_PROCESS_DEBUG_EVENT)
    {
        if (error != nullptr)
        {
            *error = "invalid bootstrap child creation event";
        }
        return false;
    }
    result->process = create_event.u.CreateProcessInfo.hProcess;
    result->primary_thread = create_event.u.CreateProcessInfo.hThread;
    result->process_id = create_event.dwProcessId;
    result->primary_thread_id = create_event.dwThreadId;
    result->image_base = reinterpret_cast<std::uintptr_t>(
        create_event.u.CreateProcessInfo.lpBaseOfImage);
    if (!ReadWholeFile(executable, &result->image_file, error) ||
        !re2dj::exe::ReadPeImageInfo(executable, &result->image_info, error) ||
        !re2dj::exe::IsGuestExecutable(result->image_info))
    {
        return false;
    }
    const std::uintptr_t entry = result->image_base + result->image_info.entry_point_rva;
    std::uint8_t original_entry_byte = 0;
    if (!SetSoftwareBreakpoint(result->process, entry, &original_entry_byte, error) ||
        !ContinueDebugEvent(create_event.dwProcessId,
                            create_event.dwThreadId,
                            DBG_CONTINUE))
    {
        if (error->empty())
        {
            *error = "cannot continue bootstrap child creation event";
        }
        return false;
    }
    DWORD entry_process_id = 0;
    DWORD entry_thread_id = 0;
    for (std::uint32_t count = 0; count < 256; ++count)
    {
        DEBUG_EVENT event = {};
        if (WaitForDebugEvent(&event, 5000) == FALSE)
        {
            *error = "cannot wait for bootstrap child entry";
            return false;
        }
        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT &&
            event.u.LoadDll.hFile != nullptr)
        {
            CloseHandle(event.u.LoadDll.hFile);
        }
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
        {
            const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(
                event.u.Exception.ExceptionRecord.ExceptionAddress);
            if (address == entry)
            {
                entry_process_id = event.dwProcessId;
                entry_thread_id = event.dwThreadId;
                const bool runtime_loaded =
                    re2dj::platform::windows::LoadInjectedRuntime(
                        result->process,
                        result->primary_thread,
                        entry_process_id,
                        entry_thread_id,
                        options.runtime_path,
                        &result->runtime_base,
                        error);
                if (!runtime_loaded ||
                    !RestoreSoftwareBreakpoint(result->process,
                                                entry,
                                                original_entry_byte,
                                                error))
                {
                    return false;
                }
                break;
            }
        }
        if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
        {
            *error = "bootstrap child exited before entry";
            return false;
        }
        if (ContinueDebugEvent(event.dwProcessId,
                               event.dwThreadId,
                               event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT
                                   ? DBG_EXCEPTION_NOT_HANDLED
                                   : DBG_CONTINUE) == FALSE)
        {
            *error = "cannot continue bootstrap child debug event";
            return false;
        }
        if (count == 255)
        {
            *error = "bootstrap child did not reach entry";
            return false;
        }
    }

    const auto find_export = [&](const char* name, std::uint32_t* rva) {
        return re2dj::platform::windows::FindPe32ExportRva(
            options.runtime_path, name, rva, error);
    };
    const auto patch_iat = [&](const char* module,
                               const char* function,
                               const char* export_name) {
        std::uint32_t slot = 0;
        bool present = false;
        std::uint32_t thunk = 0;
        return find_export(export_name, &thunk) &&
               FindOptionalIatSlotByName(result->image_info,
                                          result->image_file,
                                          module,
                                          function,
                                          &slot,
                                          &present,
                                          error) &&
               (!present || WriteRemoteU32(result->process,
                                            result->image_base + slot,
                                            result->runtime_base + thunk,
                                            error));
    };

    bool prepared = true;
    std::uint32_t rva = 0;
    prepared = find_export("g_re2dj_vfs_hdd_root", &rva) &&
               WriteRemoteAnsi(result->process,
                               result->runtime_base + rva,
                               options.vfs_source_root.string(),
                               error);
    if (!options.guest_root.empty())
    {
        prepared = prepared && find_export("g_re2dj_vfs_guest_root", &rva) &&
                   WriteRemoteAnsi(result->process,
                                   result->runtime_base + rva,
                                   options.guest_root,
                                   error);
    }
    prepared = prepared && find_export("g_re2dj_vfs_overlay_root", &rva) &&
               WriteRemoteAnsi(result->process,
                               result->runtime_base + rva,
                               options.overlay_root.string(),
                               error);
    prepared = prepared && find_export("g_re2dj_vfs_chd_path", &rva) &&
               WriteRemoteAnsi(result->process,
                               result->runtime_base + rva,
                               options.chd_path.string(),
                               error);
    prepared = prepared && find_export("g_re2dj_vfs_trace_path", &rva) &&
               WriteRemoteAnsi(result->process,
                               result->runtime_base + rva,
                               options.vfs_trace_path.string(),
                               error);
    const char* const vfs_exports[] = {"_Re2djVfsCreateFileA@28",
                                       "_Re2djVfsReadFile@20",
                                       "_Re2djVfsWriteFile@20",
                                       "_Re2djVfsSetFilePointer@16",
                                       "_Re2djVfsGetFileSize@8",
                                       "_Re2djVfsCloseHandle@4",
                                       "_Re2djVfsGetFileType@4",
                                       "_Re2djVfsSetCurrentDirectoryA@4",
                                       "_Re2djVfsGetCurrentDirectoryA@8",
                                       "_Re2djVfsFindFirstFileA@8",
                                       "_Re2djVfsFindNextFileA@8",
                                       "_Re2djVfsFindClose@4"};
    const char* const vfs_imports[] = {"CreateFileA",
                                       "ReadFile",
                                       "WriteFile",
                                       "SetFilePointer",
                                       "GetFileSize",
                                       "CloseHandle",
                                       "GetFileType",
                                       "SetCurrentDirectoryA",
                                       "GetCurrentDirectoryA",
                                       "FindFirstFileA",
                                       "FindNextFileA",
                                       "FindClose"};
    for (std::size_t index = 0; prepared && index < std::size(vfs_exports); ++index)
    {
        std::uint32_t slot = 0;
        bool present = false;
        prepared = find_export(vfs_exports[index], &rva) &&
                   FindOptionalIatSlotByName(result->image_info,
                                              result->image_file,
                                              "KERNEL32.dll",
                                              vfs_imports[index],
                                              &slot,
                                              &present,
                                              error) &&
                   (!present || WriteRemoteU32(result->process,
                                                result->image_base + slot,
                                                result->runtime_base + rva,
                                                error));
    }
    prepared = prepared && find_export("_Re2djVfsLoadImageA@24", &rva) &&
               patch_iat("USER32.dll", "LoadImageA", "_Re2djVfsLoadImageA@24");

    if (options.device_mock_lptdi || options.dynamic_vfs_resolver)
    {
        std::vector<std::uint32_t> slots;
        bool present = false;
        std::uint32_t thunk = 0;
        prepared = prepared && find_export("_Re2djHleGetProcAddress@8", &thunk) &&
                   FindOptionalIatSlotsByName(result->image_info,
                                              result->image_file,
                                              "KERNEL32.dll",
                                              "GetProcAddress",
                                              &slots,
                                              &present,
                                              error);
        for (const std::uint32_t slot : slots)
        {
            prepared = prepared && WriteRemoteU32(result->process,
                                                  result->image_base + slot,
                                                  result->runtime_base + thunk,
                                                  error);
        }
        if (options.dynamic_vfs_resolver)
        {
            prepared = prepared && find_export("g_re2dj_vfs_dynamic_resolver", &rva) &&
                       WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
        }
        if (options.device_mock_lptdi)
        {
            prepared = prepared && find_export("g_re2dj_device_mock", &rva) &&
                       WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
            prepared = prepared && find_export("g_re2dj_device_mock_path_prefix", &rva) &&
                       WriteRemoteAnsi(result->process,
                                       result->runtime_base + rva,
                                       options.device_path_prefix,
                                       error);
            prepared = prepared && patch_iat("KERNEL32.dll",
                                             "DeviceIoControl",
                                             "_Re2djDeviceIoControlMock@32");
            if (options.device_mock_wts_console_session)
            {
                prepared = prepared && find_export("g_re2dj_wts_console_session_mock", &rva) &&
                           WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
            }
        }
    }
    if (options.hardlock_handshake_enabled)
    {
        prepared = prepared && find_export("g_re2dj_hardlock_response_450", &rva) &&
                   WriteRemoteBytes(result->process,
                                    result->runtime_base + rva,
                                    options.hardlock_handshake.data(),
                                    options.hardlock_handshake.size(),
                                    error);
        prepared = prepared && find_export("g_re2dj_hardlock_response_450_enabled", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
    }
    if (options.hardlock_tail_enabled)
    {
        prepared = prepared && find_export("g_re2dj_hardlock_44c_tail_word", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, options.hardlock_tail, error);
        prepared = prepared && find_export("g_re2dj_hardlock_44c_tail_enabled", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
    }
    if (options.hardlock_seeds.has_value())
    {
        prepared = prepared && find_export("g_re2dj_hardlock_seed_module_address", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, options.hardlock_seeds->module_address, error);
        prepared = prepared && find_export("g_re2dj_hardlock_seed1", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, options.hardlock_seeds->seed1, error);
        prepared = prepared && find_export("g_re2dj_hardlock_seed2", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, options.hardlock_seeds->seed2, error);
        prepared = prepared && find_export("g_re2dj_hardlock_seed3", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, options.hardlock_seeds->seed3, error);
        prepared = prepared && find_export("g_re2dj_hardlock_seeds_enabled", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
    }
    if (options.hardlock_device)
    {
        prepared = prepared && find_export("g_re2dj_hardlock_device_enabled", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
    }
    if (options.hardlock_transform_input_trace)
    {
        prepared = prepared &&
                   find_export("g_re2dj_hardlock_transform_input_trace", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
    }
    if (!options.hardlock_transform_input_dump_path.empty())
    {
        prepared = prepared &&
                   find_export("g_re2dj_hardlock_transform_input_dump", &rva) &&
                   WriteRemoteAnsi(result->process,
                                   result->runtime_base + rva,
                                   options.hardlock_transform_input_dump_path.string(),
                                   error);
    }
    if (!options.hardlock_transform_map.empty())
    {
        std::vector<std::uint8_t> block_rows;
        std::vector<std::uint8_t> payload_records;
        re2dj::hle::hardlock::PackHardlockTransformResponseMap(
            options.hardlock_transform_map, &block_rows, &payload_records);
        const auto write_rows = [&](const char* rows_export,
                                    const char* count_export,
                                    const std::vector<std::uint8_t>& packed,
                                    std::size_t count) -> bool {
            if (count == 0)
            {
                return true;
            }
            return find_export(rows_export, &rva) &&
                   WriteRemoteBytes(result->process,
                                    result->runtime_base + rva,
                                    packed.data(),
                                    packed.size(),
                                    error) &&
                   find_export(count_export, &rva) &&
                   WriteRemoteU32(result->process,
                                  result->runtime_base + rva,
                                  static_cast<std::uint32_t>(count),
                                  error);
        };
        prepared = prepared &&
                   write_rows("g_re2dj_hardlock_transform_responses",
                              "g_re2dj_hardlock_transform_response_count",
                              block_rows,
                              options.hardlock_transform_map.blocks.size()) &&
                   write_rows("g_re2dj_hardlock_payload_responses",
                              "g_re2dj_hardlock_payload_response_count",
                              payload_records,
                              options.hardlock_transform_map.payloads.size());
    }
    if (options.message_box)
    {
        prepared = prepared && find_export("g_re2dj_message_box_result", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, IDOK, error) &&
                   find_export("g_re2dj_hle_message_box", &rva) &&
                   WriteRemoteU32(result->process, result->runtime_base + rva, 1, error);
    }
    if (options.hle_d3d3)
    {
        prepared = prepared && find_export("g_re2dj_graphics_trace_path", &rva) &&
                   WriteRemoteAnsi(result->process,
                                   result->runtime_base + rva,
                                   options.graphics_trace_path.string(),
                                   error) &&
                   find_export("g_re2dj_fullscreen", &rva) &&
                   WriteRemoteU32(result->process,
                                  result->runtime_base + rva,
                                  options.fullscreen ? TRUE : FALSE,
                                  error) &&
                   patch_iat("DDRAW.dll",
                             "DirectDrawCreate",
                             "_Re2djHleDirectDrawCreate@12") &&
                   patch_iat("DDRAW.dll",
                             "DirectDrawCreateEx",
                             "_Re2djHleDirectDrawCreateEx@16");
    }
    if (prepared)
    {
        struct DisplayEntryPoint
        {
            const char* import_name;
            const char* export_name;
        };
        constexpr DisplayEntryPoint kDisplayEntryPoints[] = {
            {"ChangeDisplaySettingsExA", "_Re2djHleChangeDisplaySettingsExA@20"},
            {"ChangeDisplaySettingsA", "_Re2djHleChangeDisplaySettingsA@8"},
        };
        for (const DisplayEntryPoint& display_entry : kDisplayEntryPoints)
        {
            std::uint32_t slot = 0;
            bool present = false;
            prepared = prepared &&
                       find_export(display_entry.export_name, &rva) &&
                       FindOptionalIatSlotByName(result->image_info,
                                                  result->image_file,
                                                  "USER32.dll",
                                                  display_entry.import_name,
                                                  &slot,
                                                  &present,
                                                  error) &&
                       (!present || WriteRemoteU32(result->process,
                                                    result->image_base + slot,
                                                    result->runtime_base + rva,
                                                    error));
        }
    }
    if (options.hle_directsound)
    {
        std::uint32_t slot = 0;
        prepared = prepared && find_export("_Re2djHleDirectSoundCreate@12", &rva) &&
                   re2dj::tools::windows_original_process_probe::FindIatSlotByOrdinal(
                       result->image_info,
                       result->image_file.data(),
                       result->image_file.size(),
                       "DSOUND.dll",
                       1,
                       &slot,
                       error) &&
                   WriteRemoteU32(result->process,
                                  result->image_base + slot,
                                  result->runtime_base + rva,
                                  error);
    }
    if (prepared && options.hle_d3d3)
    {
        prepared = patch_iat("DINPUT.dll",
                             "DirectInputCreateA",
                             "_Re2djHleDirectInputCreateA@16");
    }
    if (!prepared)
    {
        if (error->empty())
        {
            *error = "bootstrap child runtime preparation failed";
        }
        return false;
    }
    result->primary_thread_id = entry_thread_id;
    return true;
}

}  // namespace re2dj::tools::windows_x86_launcher_probe
