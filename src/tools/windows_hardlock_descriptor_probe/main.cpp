#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

extern "C" __declspec(dllimport) char g_re2dj_vfs_trace_path[MAX_PATH];
extern "C" __declspec(dllimport) char g_re2dj_device_mock_path_prefix[MAX_PATH];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_vfs_dynamic_resolver;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_mock;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_ioctl_mode;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_wts_console_session_mock;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_response_450_enabled;
extern "C" __declspec(dllimport) unsigned char g_re2dj_hardlock_response_450[6];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_44c_tail_enabled;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_44c_tail_word;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_secret_enabled;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_module_address;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_seed1;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_seed2;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_hardlock_seed3;
extern "C" __declspec(dllimport) FARPROC WINAPI Re2djHleGetProcAddress(
    HMODULE module, LPCSTR name);
extern "C" __declspec(dllimport) HANDLE WINAPI Re2djVfsCreateFileA(
    LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD flags, HANDLE template_handle);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsCloseHandle(HANDLE handle);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djDeviceIoControlMock(
    HANDLE handle, DWORD control_code, LPVOID input, DWORD input_size,
    LPVOID output, DWORD output_size, LPDWORD bytes_returned,
    LPOVERLAPPED overlapped);

namespace
{

bool Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s (error %lu)\n",
                     message,
                     static_cast<unsigned long>(GetLastError()));
    }
    return condition;
}

}  // namespace

int main()
{
    char temporary_directory[MAX_PATH] = {};
    char trace_path[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, temporary_directory) == 0 ||
        GetTempFileNameA(temporary_directory, "r2h", 0, trace_path) == 0 ||
        DeleteFileA(trace_path) == FALSE ||
        strcpy_s(g_re2dj_vfs_trace_path, trace_path) != 0 ||
        strcpy_s(g_re2dj_device_mock_path_prefix, "\\\\.\\FEnteDev") != 0)
    {
        return 1;
    }

    g_re2dj_device_mock = 1;
    g_re2dj_vfs_dynamic_resolver = 1;
    g_re2dj_device_ioctl_mode = 1;
    HMODULE kernel32 = GetModuleHandleA("KERNEL32.dll");
    const FARPROC expected_get_tick_count = GetProcAddress(kernel32, "GetTickCount");
    const FARPROC dynamic_get_tick_count = Re2djHleGetProcAddress(
        kernel32, "GetTickCount");
    const FARPROC dynamic_create_file = Re2djHleGetProcAddress(
        kernel32, "CreateFileA");
    using WtsQuerySessionInformationAProc = BOOL(WINAPI*)(
        HANDLE, DWORD, DWORD, LPSTR*, DWORD*);
    using WtsFreeMemoryProc = void(WINAPI*)(void*);
    HMODULE wtsapi = LoadLibraryA("wtsapi32.dll");
    const auto dynamic_wts_query = reinterpret_cast<WtsQuerySessionInformationAProc>(
        Re2djHleGetProcAddress(wtsapi, "WTSQuerySessionInformationA"));
    const auto wts_free_memory = reinterpret_cast<WtsFreeMemoryProc>(
        GetProcAddress(wtsapi, "WTSFreeMemory"));
    LPSTR wts_buffer = nullptr;
    DWORD wts_bytes = 0;
    g_re2dj_wts_console_session_mock = 1;
    const BOOL wts_result = dynamic_wts_query == nullptr
                                ? FALSE
                                : dynamic_wts_query(nullptr,
                                                    0xffffffffu,
                                                    4,
                                                    &wts_buffer,
                                                    &wts_bytes);
    std::uint32_t wts_state = 0xffffffffu;
    if (wts_result != FALSE && wts_buffer != nullptr &&
        wts_bytes >= sizeof(wts_state))
    {
        std::memcpy(&wts_state, wts_buffer, sizeof(wts_state));
    }
    if (wts_buffer != nullptr && wts_free_memory != nullptr)
    {
        wts_free_memory(wts_buffer);
    }
    g_re2dj_wts_console_session_mock = 0;
    HANDLE handle = Re2djVfsCreateFileA("\\\\.\\FEnteDev",
                                        GENERIC_READ,
                                        FILE_SHARE_READ,
                                        nullptr,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr);
    std::uint8_t descriptor[256] = {};
    descriptor[0x00] = 0x03;
    descriptor[0x01] = 0x47;
    descriptor[0x08] = 0x34;
    descriptor[0x09] = 0x12;
    descriptor[0x16] = 0x01;
    descriptor[0x18] = 0;
    descriptor[0x1a] = 0x07;
    for (std::size_t index = 0; index < 8; ++index)
    {
        descriptor[0x24 + index] = static_cast<std::uint8_t>(0x10 + index);
        descriptor[0x2c + index] = static_cast<std::uint8_t>(0xa0 + index);
    }
    descriptor[0xfe] = 0x34;
    descriptor[0xff] = 0x12;

    std::uint8_t lptdi_input[4] = {};
    std::uint8_t lptdi_output[8] = {};
    std::uint8_t hardlock_450_packet[6] = {0x01, 0x00, 0x00, 0x00, 0x03, 0x00};
    const std::uint8_t hardlock_450_response[6] = {
        0x01, 0x00, 0xfa, 0xfa, 0x00, 0x10};
    std::memcpy(g_re2dj_hardlock_response_450,
                hardlock_450_response,
                sizeof(hardlock_450_response));
    g_re2dj_hardlock_response_450_enabled = 1;
    g_re2dj_hardlock_44c_tail_word = 1;
    g_re2dj_hardlock_44c_tail_enabled = 1;
    DWORD bytes_returned = 99;
    bool passed = Check(wtsapi != nullptr && dynamic_wts_query != nullptr &&
                            wts_free_memory != nullptr,
                        "cannot resolve WTS observation functions") &&
                  Check(wts_result != FALSE && wts_bytes == sizeof(wts_state) &&
                            wts_state == 0,
                        "WTS console-session mock did not return active state") &&
                  Check(dynamic_get_tick_count == expected_get_tick_count,
                        "dynamic resolver changed a forwarded Win32 address") &&
                  Check(dynamic_create_file ==
                            reinterpret_cast<FARPROC>(&Re2djVfsCreateFileA),
                        "dynamic resolver changed the HLE CreateFileA route") &&
                  Check(handle != INVALID_HANDLE_VALUE,
                        "cannot open synthetic Hardlock device") &&
                  Check(Re2djDeviceIoControlMock(handle,
                                                  0x9c406410,
                                                  lptdi_input,
                                                  sizeof(lptdi_input),
                                                  lptdi_output,
                                                  sizeof(lptdi_output),
                                                  &bytes_returned,
                                                  nullptr) != FALSE,
                        "ordinary LPTDI IOCTL failed") &&
                  Check(Re2djDeviceIoControlMock(handle,
                                                  0x9c402450,
                                                  hardlock_450_packet,
                                                  sizeof(hardlock_450_packet),
                                                  hardlock_450_packet,
                                                  sizeof(hardlock_450_packet),
                                                  &bytes_returned,
                                                  nullptr) != FALSE,
                        "Hardlock 0x450 IOCTL failed") &&
                  Check(bytes_returned == sizeof(hardlock_450_response) &&
                            std::memcmp(hardlock_450_packet,
                                        hardlock_450_response,
                                        sizeof(hardlock_450_response)) == 0,
                        "Hardlock 0x450 replay did not replace the response") &&
                  Check(Re2djDeviceIoControlMock(handle,
                                                  0x9c40244c,
                                                  descriptor,
                                                  sizeof(descriptor),
                                                  descriptor,
                                                  sizeof(descriptor),
                                                  &bytes_returned,
                                                  nullptr) != FALSE,
                        "Hardlock 0x44c IOCTL failed") &&
                  Check(bytes_returned == sizeof(descriptor) &&
                            descriptor[0xfe] == 1 && descriptor[0xff] == 0,
                        "Hardlock 0x44c replay did not patch the tail");

    descriptor[0x18] = 0x06;
    descriptor[0xfe] = 0x34;
    descriptor[0xff] = 0x12;
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c40244c,
                                             descriptor,
                                             sizeof(descriptor),
                                             descriptor,
                                             sizeof(descriptor),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "Hardlock Function 6 IOCTL failed") &&
             Check(bytes_returned == 0 && descriptor[0xfe] == 0x34 &&
                       descriptor[0xff] == 0x12,
                   "Function 0 tail patch leaked into Function 6");

    descriptor[0x18] = 0x0e;
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c402458,
                                             descriptor,
                                             sizeof(descriptor),
                                             descriptor,
                                             sizeof(descriptor),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "Hardlock descriptor IOCTL failed") &&
             Check(bytes_returned == 0,
                   "descriptor diagnostic changed returned bytes");

    g_re2dj_hardlock_module_address = 0x1357;
    g_re2dj_hardlock_seed1 = 0x2468;
    g_re2dj_hardlock_seed2 = 0x369a;
    g_re2dj_hardlock_seed3 = 0x48ac;
    g_re2dj_hardlock_secret_enabled = 1;
    descriptor[0x08] = 0xcd;
    descriptor[0x09] = 0xab;
    passed = passed &&
             Check(Re2djDeviceIoControlMock(handle,
                                             0x9c402458,
                                             descriptor,
                                             sizeof(descriptor),
                                             descriptor,
                                             sizeof(descriptor),
                                             &bytes_returned,
                                             nullptr) != FALSE,
                   "redacted Hardlock descriptor IOCTL failed") &&
             Check(Re2djVfsCloseHandle(handle) != FALSE,
                   "cannot close synthetic Hardlock device");
    g_re2dj_hardlock_secret_enabled = 0;

    std::ifstream trace(trace_path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(trace)),
                               std::istreambuf_iterator<char>());
    passed = passed &&
             Check(contents.find(
                       "hardlock-descriptor:code=0x9c40244c") !=
                       std::string::npos,
                   "trace is missing the Hardlock 0x44c descriptor marker") &&
             Check(contents.find(
                       "hardlock-descriptor:code=0x9c402458:version=0347:module_id=0x0000:module_address=0x1234:block_count=1:function=0x000e:status=7") !=
                       std::string::npos,
                   "trace is missing the Hardlock descriptor marker") &&
             Check(contents.find(
                       "id_ref=1011121314151617:id_verify=a0a1a2a3a4a5a6a7:"
                       "tail_word=0x1234") !=
                       std::string::npos,
                   "trace has incorrect Hardlock key material") &&
             Check(contents.find(
                       "hardlock-descriptor:code=0x9c402458:function=0x000e:status=7:secret_fields=redacted") !=
                       std::string::npos,
                   "trace is missing the redacted Hardlock descriptor marker") &&
             Check(contents.find("module_address=0xabcd") == std::string::npos,
                   "redacted trace exposed a configured descriptor field") &&
             Check(contents.find("hardlock-descriptor:code=0x9c406410") ==
                       std::string::npos,
                   "trace contains an unrelated LPTDI descriptor marker") &&
             Check(contents.find(
                       "dynamic-resolver:name=CreateFileA:route=hle") !=
                       std::string::npos,
                   "trace is missing the HLE resolver route marker") &&
             Check(contents.find(
                       "dynamic-resolver:name=GetTickCount:route=win32") !=
                       std::string::npos,
                   "trace is missing the Win32 resolver route marker") &&
             Check(contents.find(
                       "wts-query:session=4294967295:class=4:success=1:bytes=4:"
                       "scalar_size=4:scalar=0x00000000") !=
                       std::string::npos,
                   "trace is missing the overridden WTS query marker") &&
             Check(contents.find(
                       "hardlock-450-packet:index=1:in_place=1:"
                       "input=010000000300") != std::string::npos,
                   "trace is missing the Hardlock 0x450 packet marker") &&
             Check(contents.find("hardlock-450-packet:index=2") ==
                       std::string::npos,
                   "trace contains an unrelated Hardlock 0x450 packet marker");
    trace.close();
    DeleteFileA(trace_path);
    if (wtsapi != nullptr)
    {
        FreeLibrary(wtsapi);
    }
    return passed ? 0 : 1;
}
