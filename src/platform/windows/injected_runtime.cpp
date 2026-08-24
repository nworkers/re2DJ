#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <intrin.h>

#include "re2dj/device/lptdi_challenge_response.h"

extern "C" __declspec(dllexport) volatile DWORD g_re2dj_probe_original_target = 0;
extern "C" __declspec(dllexport) char g_re2dj_hle_command_line[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_hle_windows_directory[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_hdd_root[MAX_PATH] = {};
extern "C" __declspec(dllexport) char g_re2dj_vfs_overlay_root[MAX_PATH] = {};
// Device-emulation policy: 0 keeps the natural open failure, 1 lets the
// emulated \\.\ devices open successfully.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_mock = 0;
// IOCTL policy: 0 disables synthetic IOCTLs, 1 returns zero bytes, 2 reports
// the full output size while preserving the buffer, 3 copies configured
// response-profile bytes, and 4 derives a response for a selected target state.
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_ioctl_mode = 0;
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_response_410_size = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_device_response_410[8] = {};
extern "C" __declspec(dllexport) volatile DWORD g_re2dj_device_response_414_size = 0;
extern "C" __declspec(dllexport) unsigned char g_re2dj_device_response_414[104] = {};
extern "C" __declspec(dllexport) unsigned char g_re2dj_device_target_state[8] = {};

namespace
{

constexpr char kProbeMessage[] = "re2dj:handoff:GetCommandLineA";
constexpr char kHleMessage[] = "re2dj:hle:GetCommandLineA";
constexpr char kWindowsDirectoryMessage[] = "re2dj:hle:GetWindowsDirectoryA";
constexpr char kCreateFileMessage[] = "re2dj:vfs:CreateFileA";
constexpr char kFileApiMessage[] = "re2dj:vfs:file-api";
constexpr char kDeviceIoControlMessage[] = "re2dj:device:DeviceIoControl";
constexpr char kDisplayModeMessage[] = "re2dj:hle:ChangeDisplaySettingsExA";
constexpr char kExitProcessMessage[] = "re2dj:probe:ExitProcess";

bool HasPrefixIgnoreCase(const char* text, const char* prefix)
{
    for (; *prefix != '\0'; ++text, ++prefix)
    {
        if (*text == '\0' || _strnicmp(text, prefix, 1) != 0)
        {
            return false;
        }
    }
    return *text == '\0' || *text == '\\' || *text == '/';
}

bool JoinRoot(const char* root, const char* suffix, char path[MAX_PATH])
{
    if (root == nullptr || root[0] == '\0' || suffix == nullptr)
    {
        return false;
    }
    if (strcpy_s(path, MAX_PATH, root) != 0)
    {
        return false;
    }
    const std::size_t length = std::strlen(path);
    if (*suffix != '\0' && length != 0 && path[length - 1] != '\\' && path[length - 1] != '/')
    {
        if (strcat_s(path, MAX_PATH, "\\") != 0)
        {
            return false;
        }
    }
    while (*suffix == '\\' || *suffix == '/')
    {
        ++suffix;
    }
    return strcat_s(path, MAX_PATH, suffix) == 0;
}

bool EnsureParentDirectories(const char* path)
{
    char parent[MAX_PATH] = {};
    if (strcpy_s(parent, path) != 0)
    {
        return false;
    }
    char* slash = std::strrchr(parent, '\\');
    if (slash == nullptr)
    {
        return true;
    }
    *slash = '\0';
    for (char* cursor = parent + 3; *cursor != '\0'; ++cursor)
    {
        if (*cursor != '\\' && *cursor != '/')
        {
            continue;
        }
        const char saved = *cursor;
        *cursor = '\0';
        const BOOL created = CreateDirectoryA(parent, nullptr);
        const DWORD error = created ? ERROR_SUCCESS : GetLastError();
        *cursor = saved;
        if (!created && error != ERROR_ALREADY_EXISTS)
        {
            return false;
        }
    }
    const BOOL created = CreateDirectoryA(parent, nullptr);
    return created != FALSE || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool IsRegularFile(const char* path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Synthetic handles for emulated \\.\ devices live in a reserved range so the
// file wrappers can recognize them without consulting the host handle table.
constexpr std::uintptr_t kDeviceMockHandleBase = 0xFEED0000;

bool IsDeviceMockHandle(HANDLE handle)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(handle);
    return value > kDeviceMockHandleBase && value <= kDeviceMockHandleBase + 0xff;
}

// Matches device paths such as \\.\LPTDI1 case-insensitively. A plain prefix
// test is used because the port digit varies at guest runtime.
bool HasDeviceMockPrefix(const char* name)
{
    constexpr char kPrefix[] = "\\\\.\\lptdi";
    if (g_re2dj_device_mock == 0 || name == nullptr)
    {
        return false;
    }
    return _strnicmp(name, kPrefix, sizeof(kPrefix) - 1) == 0;
}

bool MapVfsPath(const char* name, bool write, char path[MAX_PATH], char source[MAX_PATH])
{
    const char* hdd_suffix = nullptr;
    const char* support_suffix = nullptr;
    if (HasPrefixIgnoreCase(name, "D:\\ez2dj"))
    {
        hdd_suffix = name + 8;
    }
    else if (HasPrefixIgnoreCase(name, "C:\\windows"))
    {
        support_suffix = name + 10;
    }
    else if (name[0] != '\\' && name[0] != '/')
    {
        hdd_suffix = name;
    }
    else
    {
        return false;
    }

    const char* target_root = write ? g_re2dj_vfs_overlay_root
                                    : (hdd_suffix != nullptr ? g_re2dj_vfs_hdd_root
                                                              : g_re2dj_hle_windows_directory);
    if (target_root[0] == '\0')
    {
        return false;
    }
    if (write && support_suffix != nullptr)
    {
        char support_root[MAX_PATH] = {};
        if (!JoinRoot(target_root, "windows", support_root) ||
            !JoinRoot(support_root, support_suffix, path))
        {
            return false;
        }
    }
    else if (!JoinRoot(target_root, hdd_suffix != nullptr ? hdd_suffix : support_suffix, path))
    {
        return false;
    }

    if (!write)
    {
        char overlay[MAX_PATH] = {};
        char overlay_windows[MAX_PATH] = {};
        if (g_re2dj_vfs_overlay_root[0] != '\0' &&
            ((support_suffix == nullptr && JoinRoot(g_re2dj_vfs_overlay_root, hdd_suffix, overlay)) ||
             (support_suffix != nullptr &&
              JoinRoot(g_re2dj_vfs_overlay_root, "windows", overlay_windows) &&
              JoinRoot(overlay_windows, support_suffix, overlay))) &&
            IsRegularFile(overlay))
        {
            strcpy_s(path, MAX_PATH, overlay);
        }
        return true;
    }
    if (source != nullptr)
    {
        const char* source_root = hdd_suffix != nullptr ? g_re2dj_vfs_hdd_root
                                                         : g_re2dj_hle_windows_directory;
        if (!JoinRoot(source_root, hdd_suffix != nullptr ? hdd_suffix : support_suffix, source))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

extern "C" __declspec(dllexport) LONG WINAPI Re2djHleChangeDisplaySettingsExA(
    LPCSTR device_name,
    DEVMODEA* dev_mode,
    HWND window,
    DWORD flags,
    LPVOID reserved)
{
    constexpr DWORD kRequiredFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    if (device_name == nullptr && dev_mode != nullptr && window == nullptr &&
        flags == CDS_UPDATEREGISTRY && reserved == nullptr &&
        (dev_mode->dmFields & kRequiredFields) == kRequiredFields &&
        dev_mode->dmPelsWidth == 640 && dev_mode->dmPelsHeight == 480 &&
        dev_mode->dmBitsPerPel == 16)
    {
        OutputDebugStringA(kDisplayModeMessage);
        SetLastError(ERROR_SUCCESS);
        return DISP_CHANGE_SUCCESSFUL;
    }
    return ChangeDisplaySettingsExA(device_name, dev_mode, window, flags, reserved);
}

extern "C" __declspec(dllexport) HANDLE WINAPI Re2djVfsCreateFileA(
    LPCSTR name,
    DWORD access,
    DWORD share,
    LPSECURITY_ATTRIBUTES security,
    DWORD disposition,
    DWORD flags,
    HANDLE template_handle)
{
    OutputDebugStringA(kCreateFileMessage);
    if (name == nullptr)
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    if (HasDeviceMockPrefix(name))
    {
        SetLastError(ERROR_SUCCESS);
        return reinterpret_cast<HANDLE>(kDeviceMockHandleBase + 1);
    }
    char path[MAX_PATH] = {};
    char source[MAX_PATH] = {};
    const bool write = (access & (GENERIC_WRITE | FILE_APPEND_DATA | DELETE)) != 0;
    if (!MapVfsPath(name, write, path, source))
    {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    if (write)
    {
        const bool overlay_exists = IsRegularFile(path);
        const bool source_exists = IsRegularFile(source);
        if (disposition == CREATE_NEW && (overlay_exists || source_exists))
        {
            SetLastError(ERROR_FILE_EXISTS);
            return INVALID_HANDLE_VALUE;
        }
        if ((disposition == OPEN_EXISTING || disposition == TRUNCATE_EXISTING) &&
            !overlay_exists && !source_exists)
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        if (!overlay_exists && source_exists && disposition != CREATE_ALWAYS &&
            !EnsureParentDirectories(path))
        {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        if (!overlay_exists && source_exists && disposition != CREATE_ALWAYS &&
            CopyFileA(source, path, TRUE) == FALSE)
        {
            return INVALID_HANDLE_VALUE;
        }
        if (!EnsureParentDirectories(path))
        {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
    }
    return CreateFileA(path, access, share, security, disposition, flags, template_handle);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsReadFile(
    HANDLE handle, LPVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        if (transferred != nullptr)
        {
            *transferred = 0;
        }
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return ReadFile(handle, buffer, size, transferred, overlapped);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsWriteFile(
    HANDLE handle, LPCVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        if (transferred != nullptr)
        {
            *transferred = 0;
        }
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return WriteFile(handle, buffer, size, transferred, overlapped);
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsSetFilePointer(
    HANDLE handle, LONG distance, PLONG distance_high, DWORD method)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return INVALID_SET_FILE_POINTER;
    }
    return SetFilePointer(handle, distance, distance_high, method);
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetFileSize(
    HANDLE handle, LPDWORD high)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return INVALID_FILE_SIZE;
    }
    return GetFileSize(handle, high);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djVfsCloseHandle(HANDLE handle)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return CloseHandle(handle);
}

extern "C" __declspec(dllexport) DWORD WINAPI Re2djVfsGetFileType(HANDLE handle)
{
    OutputDebugStringA(kFileApiMessage);
    if (IsDeviceMockHandle(handle))
    {
        SetLastError(ERROR_SUCCESS);
        return FILE_TYPE_CHAR;
    }
    return GetFileType(handle);
}

extern "C" __declspec(dllexport) BOOL WINAPI Re2djDeviceIoControlMock(
    HANDLE handle,
    DWORD control_code,
    LPVOID input,
    DWORD input_size,
    LPVOID output,
    DWORD output_size,
    LPDWORD bytes_returned,
    LPOVERLAPPED overlapped)
{
    OutputDebugStringA(kDeviceIoControlMessage);
    if (IsDeviceMockHandle(handle))
    {
        if (g_re2dj_device_ioctl_mode == 4)
        {
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 0;
            }
            if (control_code == 0x9c406410)
            {
                if (output == nullptr || output_size < 8)
                {
                    SetLastError(ERROR_INSUFFICIENT_BUFFER);
                    return FALSE;
                }
                std::memset(output, 0, 8);
                if (bytes_returned != nullptr)
                {
                    *bytes_returned = 8;
                }
                SetLastError(ERROR_SUCCESS);
                return TRUE;
            }
            if (control_code != 0x9c406414)
            {
                SetLastError(ERROR_INVALID_FUNCTION);
                return FALSE;
            }
            if (input == nullptr || input_size < 4)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (output == nullptr || output_size < 104)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            const auto* const input_bytes = static_cast<const unsigned char*>(input);
            const std::uint32_t seed =
                static_cast<std::uint32_t>(input_bytes[0]) |
                (static_cast<std::uint32_t>(input_bytes[1]) << 8) |
                (static_cast<std::uint32_t>(input_bytes[2]) << 16) |
                (static_cast<std::uint32_t>(input_bytes[3]) << 24);
            re2dj::device::LptdiTargetState target_state = {};
            std::memcpy(target_state.data(),
                        g_re2dj_device_target_state,
                        target_state.size());
            const re2dj::device::LptdiTargetState response =
                re2dj::device::EncodeLptdiTargetState(seed, target_state);
            std::memset(output, 0, 104);
            std::memcpy(static_cast<unsigned char*>(output) + 4,
                        response.data(),
                        response.size());
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 104;
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        if (g_re2dj_device_ioctl_mode == 3)
        {
            const unsigned char* response = nullptr;
            DWORD response_size = 0;
            DWORD response_capacity = 0;
            if (control_code == 0x9c406410)
            {
                response = g_re2dj_device_response_410;
                response_size = g_re2dj_device_response_410_size;
                response_capacity = sizeof(g_re2dj_device_response_410);
            }
            else if (control_code == 0x9c406414)
            {
                response = g_re2dj_device_response_414;
                response_size = g_re2dj_device_response_414_size;
                response_capacity = sizeof(g_re2dj_device_response_414);
            }
            if (bytes_returned != nullptr)
            {
                *bytes_returned = 0;
            }
            if (response == nullptr || response_size == 0)
            {
                SetLastError(ERROR_INVALID_FUNCTION);
                return FALSE;
            }
            if (response_size > response_capacity)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }
            if (output == nullptr || output_size < response_size)
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            std::memcpy(output, response, response_size);
            if (bytes_returned != nullptr)
            {
                *bytes_returned = response_size;
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        if (bytes_returned != nullptr)
        {
            *bytes_returned = g_re2dj_device_ioctl_mode == 2 && output != nullptr
                                  ? output_size
                                  : 0;
        }
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return DeviceIoControl(handle,
                           control_code,
                           input,
                           input_size,
                           output,
                           output_size,
                           bytes_returned,
                           overlapped);
}

extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI Re2djProbeExitProcess(UINT code)
{
    char message[96] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:probe:ExitProcess:code=0x%08x:return=0x%08x",
                  static_cast<unsigned>(code),
                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(_ReturnAddress())));
    OutputDebugStringA(message);
    OutputDebugStringA(kExitProcessMessage);
    ExitProcess(code);
}

extern "C" __declspec(dllexport) void __declspec(naked) Re2djProbeGetCommandLineA()
{
    __asm
    {
        pushfd
        pushad
        push offset kProbeMessage
        call OutputDebugStringA
        add esp, 4
        popad
        popfd
        jmp dword ptr [g_re2dj_probe_original_target]
    }
}

extern "C" __declspec(dllexport) void __declspec(naked) Re2djHleGetCommandLineA()
{
    __asm
    {
        pushfd
        pushad
        push offset kHleMessage
        call OutputDebugStringA
        add esp, 4
        popad
        popfd
        mov eax, offset g_re2dj_hle_command_line
        ret
    }
}

extern "C" __declspec(dllexport) void __declspec(naked) Re2djHleGetWindowsDirectoryA()
{
    __asm
    {
        push offset kWindowsDirectoryMessage
        call OutputDebugStringA
        add esp, 4
        mov eax, dword ptr [esp+4]
        test eax, eax
        je no_copy
        push dword ptr [esp+8]
        push offset g_re2dj_hle_windows_directory
        push eax
        call lstrcpynA
    no_copy:
        push offset g_re2dj_hle_windows_directory
        call lstrlenA
        ret 8
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
