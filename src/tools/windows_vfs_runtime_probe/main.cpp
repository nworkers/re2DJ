#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

extern "C" __declspec(dllimport) char g_re2dj_vfs_hdd_root[MAX_PATH];
extern "C" __declspec(dllimport) char g_re2dj_vfs_overlay_root[MAX_PATH];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_mock;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_ioctl_mode;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_response_410_size;
extern "C" __declspec(dllimport) unsigned char g_re2dj_device_response_410[8];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_response_414_size;
extern "C" __declspec(dllimport) unsigned char g_re2dj_device_response_414[104];
extern "C" __declspec(dllimport) unsigned char g_re2dj_device_target_state[8];
extern "C" __declspec(dllimport) HANDLE WINAPI Re2djVfsCreateFileA(
    LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD flags, HANDLE template_handle);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsReadFile(
    HANDLE handle, LPVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsWriteFile(
    HANDLE handle, LPCVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsCloseHandle(HANDLE handle);
extern "C" __declspec(dllimport) DWORD WINAPI Re2djVfsSetFilePointer(
    HANDLE handle, LONG distance, PLONG distance_high, DWORD method);
extern "C" __declspec(dllimport) DWORD WINAPI Re2djVfsGetFileSize(HANDLE handle, LPDWORD high);
extern "C" __declspec(dllimport) DWORD WINAPI Re2djVfsGetFileType(HANDLE handle);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djDeviceIoControlMock(
    HANDLE handle, DWORD control_code, LPVOID input, DWORD input_size, LPVOID output,
    DWORD output_size, LPDWORD bytes_returned, LPOVERLAPPED overlapped);

namespace
{

bool Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s (error %lu)\\n", message, static_cast<unsigned long>(GetLastError()));
    }
    return condition;
}

}  // namespace

int main()
{
    char temporary_directory[MAX_PATH] = {};
    char temporary[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, temporary_directory) == 0 ||
        GetTempFileNameA(temporary_directory, "r2v", 0, temporary) == 0)
    {
        return 1;
    }
    DeleteFileA(temporary);
    const std::filesystem::path root = temporary;
    const std::filesystem::path hdd = root / "hdd";
    const std::filesystem::path overlay = root / "overlay";
    std::filesystem::create_directories(hdd / "DATA");
    {
        std::ofstream original(hdd / "DATA" / "ORIGINAL.TXT", std::ios::binary);
        original << "original";
    }
    if (!Check(strcpy_s(g_re2dj_vfs_hdd_root, hdd.string().c_str()) == 0, "cannot configure HDD root") ||
        !Check(strcpy_s(g_re2dj_vfs_overlay_root, overlay.string().c_str()) == 0,
               "cannot configure overlay root"))
    {
        std::filesystem::remove_all(root);
        return 1;
    }

    HANDLE handle = Re2djVfsCreateFileA("D:\\ez2dj\\DATA\\ORIGINAL.TXT",
                                         GENERIC_READ,
                                         FILE_SHARE_READ,
                                         nullptr,
                                         OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL,
                                         nullptr);
    char contents[16] = {};
    DWORD read = 0;
    bool passed = Check(handle != INVALID_HANDLE_VALUE, "cannot open original through VFS") &&
                  Check(Re2djVfsReadFile(handle, contents, sizeof(contents), &read, nullptr) != FALSE,
                        "cannot read original through VFS") &&
                  Check(std::string(contents, read) == "original", "VFS read returned wrong original data") &&
                  Check(Re2djVfsCloseHandle(handle) != FALSE, "cannot close original VFS handle");

    handle = Re2djVfsCreateFileA("D:\\ez2dj\\DATA\\ORIGINAL.TXT",
                                  GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
    const char replacement[] = "O";
    DWORD written = 0;
    passed = passed && Check(handle != INVALID_HANDLE_VALUE, "cannot open copy-on-write VFS handle");
    passed = passed && Check(Re2djVfsWriteFile(handle, replacement, 1, &written, nullptr) != FALSE && written == 1,
                              "cannot write overlay copy");
    passed = passed && Check(Re2djVfsCloseHandle(handle) != FALSE, "cannot close overlay VFS handle");

    std::ifstream original(hdd / "DATA" / "ORIGINAL.TXT", std::ios::binary);
    std::string original_text((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
    std::ifstream copied(overlay / "DATA" / "ORIGINAL.TXT", std::ios::binary);
    std::string copied_text((std::istreambuf_iterator<char>(copied)), std::istreambuf_iterator<char>());
    passed = passed && Check(original_text == "original", "original HDD file was modified") &&
             Check(copied_text == "Original", "overlay copy has wrong data");
    original.close();
    copied.close();

    handle = Re2djVfsCreateFileA("\\\\.\\LPTDI7",
                                  GENERIC_READ,
                                  FILE_SHARE_READ,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
    passed = passed && Check(handle == INVALID_HANDLE_VALUE,
                             "device mock opened while policy was disabled");

    g_re2dj_device_mock = 1;
    handle = Re2djVfsCreateFileA("\\\\.\\LpTdI7",
                                  GENERIC_READ,
                                  FILE_SHARE_READ,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
    passed = passed && Check(handle != INVALID_HANDLE_VALUE,
                             "device mock did not recognize LPTDI path");
    read = 1;
    passed = passed &&
             Check(Re2djVfsReadFile(handle, contents, sizeof(contents), &read, nullptr) != FALSE &&
                       read == 0,
                   "device mock read did not return EOF");
    written = 1;
    SetLastError(ERROR_SUCCESS);
    const BOOL write_result = Re2djVfsWriteFile(handle, replacement, 1, &written, nullptr);
    const DWORD write_error = GetLastError();
    passed = passed && Check(write_result == FALSE && written == 0 &&
                                 write_error == ERROR_ACCESS_DENIED,
                             "device mock write did not fail with access denied");
    SetLastError(ERROR_SUCCESS);
    const DWORD seek_result = Re2djVfsSetFilePointer(handle, 0, nullptr, FILE_BEGIN);
    const DWORD seek_error = GetLastError();
    passed = passed && Check(seek_result == INVALID_SET_FILE_POINTER &&
                                 seek_error == ERROR_INVALID_FUNCTION,
                             "device mock seek did not fail with invalid function");
    SetLastError(ERROR_SUCCESS);
    const DWORD size_result = Re2djVfsGetFileSize(handle, nullptr);
    const DWORD size_error = GetLastError();
    passed = passed && Check(size_result == INVALID_FILE_SIZE &&
                                 size_error == ERROR_INVALID_FUNCTION,
                             "device mock size did not fail with invalid function");
    passed = passed && Check(Re2djVfsGetFileType(handle) == FILE_TYPE_CHAR,
                             "device mock did not report character-device type");

    std::uint8_t ioctl_input[4] = {1, 2, 3, 4};
    std::uint8_t ioctl_output[8] = {5, 6, 7, 8, 9, 10, 11, 12};
    const std::uint8_t expected_output[8] = {5, 6, 7, 8, 9, 10, 11, 12};
    DWORD bytes_returned = 99;
    g_re2dj_device_ioctl_mode = 1;
    SetLastError(ERROR_INVALID_FUNCTION);
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c406410,
                                             ioctl_input,
                                             sizeof(ioctl_input),
                                             ioctl_output,
                                             sizeof(ioctl_output),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "device IOCTL zero-byte success mock failed") &&
             Check(bytes_returned == 0, "device IOCTL mock returned nonzero bytes") &&
             Check(std::memcmp(ioctl_output, expected_output, sizeof(ioctl_output)) == 0,
                   "device IOCTL mock changed output buffer") &&
             Check(GetLastError() == ERROR_SUCCESS,
                    "device IOCTL mock did not clear last error");

    bytes_returned = 99;
    g_re2dj_device_ioctl_mode = 2;
    SetLastError(ERROR_INVALID_FUNCTION);
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c406410,
                                             ioctl_input,
                                             sizeof(ioctl_input),
                                             ioctl_output,
                                             sizeof(ioctl_output),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "device IOCTL full-size success mock failed") &&
             Check(bytes_returned == sizeof(ioctl_output),
                   "device IOCTL mock returned the wrong full size") &&
             Check(std::memcmp(ioctl_output, expected_output, sizeof(ioctl_output)) == 0,
                   "device IOCTL full-size mock changed output buffer") &&
             Check(GetLastError() == ERROR_SUCCESS,
                   "device IOCTL full-size mock did not clear last error");

    const std::uint8_t profile_response[8] = {1, 0, 0, 0, 4, 3, 2, 1};
    std::memcpy(g_re2dj_device_response_410,
                profile_response,
                sizeof(profile_response));
    g_re2dj_device_response_410_size = sizeof(profile_response);
    g_re2dj_device_response_414_size = 0;
    std::memset(ioctl_output, 0, sizeof(ioctl_output));
    bytes_returned = 99;
    g_re2dj_device_ioctl_mode = 3;
    SetLastError(ERROR_INVALID_FUNCTION);
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c406410,
                                             ioctl_input,
                                             sizeof(ioctl_input),
                                             ioctl_output,
                                             sizeof(ioctl_output),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "device IOCTL response-profile mock failed") &&
             Check(bytes_returned == sizeof(profile_response),
                   "device IOCTL response profile returned the wrong size") &&
             Check(std::memcmp(ioctl_output,
                               profile_response,
                               sizeof(profile_response)) == 0,
                   "device IOCTL response profile copied the wrong bytes") &&
             Check(GetLastError() == ERROR_SUCCESS,
                   "device IOCTL response profile did not clear last error");

    bytes_returned = 99;
    SetLastError(ERROR_SUCCESS);
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c406414,
                                             ioctl_input,
                                             sizeof(ioctl_input),
                                             ioctl_output,
                                             sizeof(ioctl_output),
                                             &bytes_returned,
                                             nullptr) == FALSE,
                   "missing device IOCTL profile entry did not fail") &&
             Check(bytes_returned == 0,
                   "missing device IOCTL profile entry returned bytes") &&
             Check(GetLastError() == ERROR_INVALID_FUNCTION,
                   "missing device IOCTL profile entry returned wrong error");

    const std::uint8_t target_state[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::memcpy(g_re2dj_device_target_state, target_state, sizeof(target_state));
    std::memset(ioctl_output, 0xff, sizeof(ioctl_output));
    bytes_returned = 99;
    g_re2dj_device_ioctl_mode = 4;
    SetLastError(ERROR_INVALID_FUNCTION);
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c406410,
                                             ioctl_input,
                                             sizeof(ioctl_input),
                                             ioctl_output,
                                             sizeof(ioctl_output),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "adaptive device first IOCTL failed") &&
             Check(bytes_returned == sizeof(ioctl_output),
                   "adaptive device first IOCTL returned the wrong size");
    for (const std::uint8_t value : ioctl_output)
    {
        passed = passed && Check(value == 0, "adaptive device first IOCTL was not zero");
    }

    std::uint8_t adaptive_input[24] = {0xf1, 0x31, 0xea, 0x75};
    std::uint8_t adaptive_output[104] = {};
    std::memset(adaptive_output, 0xff, sizeof(adaptive_output));
    const std::uint8_t expected_payload[8] = {
        0xe7, 0xc6, 0x6e, 0x40, 0x9a, 0xd1, 0x94, 0xb3,
    };
    bytes_returned = 99;
    SetLastError(ERROR_INVALID_FUNCTION);
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c406414,
                                             adaptive_input,
                                             sizeof(adaptive_input),
                                             adaptive_output,
                                             sizeof(adaptive_output),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "adaptive device second IOCTL failed") &&
             Check(bytes_returned == sizeof(adaptive_output),
                   "adaptive device second IOCTL returned the wrong size") &&
             Check(std::memcmp(adaptive_output + 4,
                               expected_payload,
                               sizeof(expected_payload)) == 0,
                   "adaptive device second IOCTL encoded the wrong payload") &&
             Check(GetLastError() == ERROR_SUCCESS,
                   "adaptive device second IOCTL did not clear last error");
    for (std::size_t index = 0; index < sizeof(adaptive_output); ++index)
    {
        if (index >= 4 && index < 12)
        {
            continue;
        }
        passed = passed &&
                 Check(adaptive_output[index] == 0,
                       "adaptive device second IOCTL left nonzero padding");
    }
    passed = passed &&
             Check(Re2djVfsCloseHandle(handle) != FALSE, "cannot close device mock handle");

    std::filesystem::remove_all(root);
    return passed ? 0 : 1;
}
