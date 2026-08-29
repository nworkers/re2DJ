#define NOMINMAX
#define CINTERFACE
#define DIRECT3D_VERSION 0x0600
#define DIRECTSOUND_VERSION 0x0300
#include <windows.h>
#include <dwmapi.h>
#include <ddraw.h>
#include <d3d.h>
#include <dsound.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" __declspec(dllimport) char g_re2dj_vfs_hdd_root[MAX_PATH];
extern "C" __declspec(dllimport) char g_re2dj_vfs_overlay_root[MAX_PATH];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_mock;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_ioctl_mode;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_response_410_size;
extern "C" __declspec(dllimport) unsigned char g_re2dj_device_response_410[8];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_device_response_414_size;
extern "C" __declspec(dllimport) unsigned char g_re2dj_device_response_414[104];
extern "C" __declspec(dllimport) unsigned char g_re2dj_device_target_state[8];
extern "C" __declspec(dllimport) char g_re2dj_graphics_trace_path[MAX_PATH];
extern "C" __declspec(dllimport) HANDLE WINAPI Re2djVfsCreateFileA(
    LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD flags, HANDLE template_handle);
extern "C" __declspec(dllimport) HANDLE WINAPI Re2djVfsLoadImageA(
    HINSTANCE instance, LPCSTR name, UINT type, int desired_width,
    int desired_height, UINT flags);
extern "C" __declspec(dllimport) char g_re2dj_vfs_trace_path[MAX_PATH];
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
extern "C" __declspec(dllimport) LONG WINAPI Re2djHleChangeDisplaySettingsExA(
    LPCSTR device_name, DEVMODEA* dev_mode, HWND window, DWORD flags, LPVOID reserved);
extern "C" __declspec(dllimport) HRESULT WINAPI Re2djHleDirectDrawCreate(
    GUID* device_guid, LPDIRECTDRAW* direct_draw, IUnknown* outer);
extern "C" __declspec(dllimport) HRESULT WINAPI Re2djHleDirectSoundCreate(
    GUID* device_guid, LPDIRECTSOUND* direct_sound, IUnknown* outer);
extern "C" __declspec(dllimport) volatile float g_re2dj_audio_master_gain;
extern "C" __declspec(dllimport) char g_re2dj_audio_trace_path[MAX_PATH];
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_audio_image_base;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_demo_volume;
extern "C" __declspec(dllimport) volatile DWORD g_re2dj_fullscreen;
extern "C" __declspec(dllimport) BOOL WINAPI Re2djUpdateWindowTitle(HWND window, double fps);
extern "C" __declspec(dllimport) char g_re2dj_io_config_path[MAX_PATH];
extern "C" __declspec(dllimport) float WINAPI Re2djHleGetAudioMasterGain();
extern "C" __declspec(dllimport) UINT WINAPI Re2djHleGetPrivateProfileIntA(
    LPCSTR section, LPCSTR key, INT default_value, LPCSTR filename);

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

HRESULT CALLBACK CaptureZBufferFormat(DDPIXELFORMAT* format, void* context)
{
    bool* const captured = static_cast<bool*>(context);
    if (format != nullptr && format->dwSize == sizeof(DDPIXELFORMAT) &&
        (format->dwFlags & DDPF_ZBUFFER) != 0 && format->dwZBufferBitDepth == 16)
    {
        *captured = true;
    }
    return D3DENUMRET_CANCEL;
}

HRESULT CALLBACK CaptureTextureFormat(DDPIXELFORMAT* format, void* context)
{
    bool* const captured = static_cast<bool*>(context);
    if (format != nullptr && format->dwSize == sizeof(DDPIXELFORMAT) &&
        (format->dwFlags & DDPF_RGB) != 0 && format->dwRGBBitCount == 16 &&
        format->dwRBitMask == 0xf800 && format->dwGBitMask == 0x07e0 &&
        format->dwBBitMask == 0x001f)
    {
        *captured = true;
    }
    return D3DENUMRET_CANCEL;
}

LRESULT CALLBACK CaptionConsumingWindowProcedure(HWND window,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam)
{
    if (message == WM_SETTEXT || message == WM_GETTEXT || message == WM_GETTEXTLENGTH ||
        message == WM_NCCALCSIZE || message == WM_NCPAINT || message == WM_NCACTIVATE)
    {
        return 0;
    }
    if (message == WM_NCHITTEST)
    {
        return HTCLIENT;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

LRESULT CALLBACK CloseConsumingWindowProcedure(HWND window,
                                               UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam)
{
    if (message == WM_SYSCOMMAND && (wparam & 0xfff0U) == SC_CLOSE)
    {
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

int RunWindowCloseExitChild()
{
    constexpr char window_class_name[] = "re2dj-window-close-exit-child";
    const HINSTANCE module = GetModuleHandleA(nullptr);
    WNDCLASSA window_class = {};
    window_class.lpfnWndProc = CloseConsumingWindowProcedure;
    window_class.hInstance = module;
    window_class.lpszClassName = window_class_name;
    if (RegisterClassA(&window_class) == 0)
    {
        return 2;
    }
    const HWND window = CreateWindowExA(0,
                                        window_class_name,
                                        "close child",
                                        WS_POPUP,
                                        0,
                                        0,
                                        100,
                                        100,
                                        nullptr,
                                        nullptr,
                                        module,
                                        nullptr);
    LPDIRECTDRAW direct_draw = nullptr;
    LPDIRECTDRAW4 direct_draw4 = nullptr;
    if (window == nullptr ||
        Re2djHleDirectDrawCreate(nullptr, &direct_draw, nullptr) != DD_OK ||
        direct_draw == nullptr ||
        IDirectDraw_QueryInterface(direct_draw,
                                   IID_IDirectDraw4,
                                   reinterpret_cast<void**>(&direct_draw4)) != S_OK ||
        direct_draw4 == nullptr ||
        IDirectDraw4_SetCooperativeLevel(
            direct_draw4, window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) != DD_OK)
    {
        return 3;
    }
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR adapter = SetWindowLongPtrA(
        window,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&CloseConsumingWindowProcedure));
    if (adapter == 0 && GetLastError() != ERROR_SUCCESS)
    {
        return 4;
    }
    SendMessageA(window, WM_SYSCOMMAND, SC_CLOSE, 0);
    Sleep(2000);
    return 5;
}

bool RunWindowCloseExitProbe()
{
    constexpr char trace_environment[] = "RE2DJ_WINDOW_TRACE_PROBE";
    char executable[MAX_PATH] = {};
    char temporary_directory[MAX_PATH] = {};
    char trace_path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, executable, MAX_PATH) == 0 ||
        GetTempPathA(MAX_PATH, temporary_directory) == 0 ||
        GetTempFileNameA(temporary_directory, "r2w", 0, trace_path) == 0 ||
        DeleteFileA(trace_path) == FALSE)
    {
        return false;
    }
    std::string command_line = std::string("\"") + executable +
                               "\" --window-close-exit-child";
    std::vector<char> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back('\0');
    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (SetEnvironmentVariableA(trace_environment, trace_path) == FALSE)
    {
        return false;
    }
    if (CreateProcessA(executable,
                       mutable_command.data(),
                       nullptr,
                       nullptr,
                       FALSE,
                       CREATE_NO_WINDOW,
                       nullptr,
                       nullptr,
                       &startup,
                       &process) == FALSE)
    {
        SetEnvironmentVariableA(trace_environment, nullptr);
        return false;
    }
    SetEnvironmentVariableA(trace_environment, nullptr);
    CloseHandle(process.hThread);
    const DWORD wait = WaitForSingleObject(process.hProcess, 5000);
    DWORD exit_code = 0xffffffff;
    const bool exited = wait == WAIT_OBJECT_0 &&
                        GetExitCodeProcess(process.hProcess, &exit_code) != FALSE &&
                        exit_code == 0;
    if (wait == WAIT_TIMEOUT)
    {
        TerminateProcess(process.hProcess, 5);
        WaitForSingleObject(process.hProcess, 5000);
    }
    CloseHandle(process.hProcess);
    std::ifstream trace_file(trace_path, std::ios::binary);
    const std::string trace((std::istreambuf_iterator<char>(trace_file)),
                            std::istreambuf_iterator<char>());
    DeleteFileA(trace_path);
    return exited && trace.find("event=target") != std::string::npos &&
           trace.find("event=sample") != std::string::npos &&
           trace.find("event=watcher-exit") != std::string::npos;
}

}  // namespace

int main()
{
    char window_trace_path[MAX_PATH] = {};
    const DWORD window_trace_length = GetEnvironmentVariableA(
        "RE2DJ_WINDOW_TRACE_PROBE", window_trace_path, MAX_PATH);
    if (window_trace_length > 0 && window_trace_length < MAX_PATH)
    {
        strcpy_s(g_re2dj_graphics_trace_path, window_trace_path);
    }
    if (std::strstr(GetCommandLineA(), "--window-close-exit-child") != nullptr)
    {
        return RunWindowCloseExitChild();
    }
    if (!Check(RunWindowCloseExitProbe(), "window close did not exit the child process"))
    {
        return 1;
    }
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
    const std::filesystem::path profile = root / "ez2dj.ini";
    {
        std::ofstream ini(profile, std::ios::binary);
        ini << "[GAMEASSIGNMENTS]\nDemoVolume=0\n[OTHER]\nValue=7\n";
    }
    {
        BITMAPFILEHEADER file_header = {};
        file_header.bfType = 0x4d42;
        file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file_header.bfSize = file_header.bfOffBits + 4;
        BITMAPINFOHEADER info_header = {};
        info_header.biSize = sizeof(BITMAPINFOHEADER);
        info_header.biWidth = 1;
        info_header.biHeight = 1;
        info_header.biPlanes = 1;
        info_header.biBitCount = 24;
        info_header.biCompression = BI_RGB;
        info_header.biSizeImage = 4;
        const unsigned char pixel[4] = {0x33, 0x22, 0x11, 0x00};
        std::filesystem::create_directories(hdd / "System" / "CompanyLogo");
        std::ofstream bitmap(hdd / "System" / "CompanyLogo" / "LOGO.BMP",
                             std::ios::binary);
        bitmap.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
        bitmap.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
        bitmap.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    }
    {
        std::ofstream script(hdd / "System" / "CompanyLogo" / "logo.str", std::ios::binary);
        script << "logostr";
    }
    const std::filesystem::path trace = root / "vfs.log";
    const std::filesystem::path audio_trace = root / "audio.log";
    if (!Check(strcpy_s(g_re2dj_vfs_hdd_root, hdd.string().c_str()) == 0, "cannot configure HDD root") ||
        !Check(strcpy_s(g_re2dj_vfs_overlay_root, overlay.string().c_str()) == 0,
               "cannot configure overlay root") ||
        !Check(strcpy_s(g_re2dj_vfs_trace_path, trace.string().c_str()) == 0,
               "cannot configure VFS trace path") ||
        !Check(strcpy_s(g_re2dj_audio_trace_path, audio_trace.string().c_str()) == 0,
               "cannot configure audio trace path") ||
        !Check(strcpy_s(g_re2dj_io_config_path, "synthetic-io.ini") == 0 &&
                   std::strcmp(g_re2dj_io_config_path, "synthetic-io.ini") == 0,
               "cannot configure I/O mapping path"))
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
    g_re2dj_demo_volume = 3;
    bool passed =
        Check(Re2djHleGetPrivateProfileIntA("GAMEASSIGNMENTS",
                                            "DemoVolume",
                                            1,
                                            profile.string().c_str()) == 3,
              "demo volume profile override failed") &&
        Check(Re2djHleGetPrivateProfileIntA("OTHER",
                                            "Value",
                                            1,
                                            profile.string().c_str()) == 7,
              "unrelated profile read did not pass through") &&
        Check(handle != INVALID_HANDLE_VALUE, "cannot open original through VFS") &&
        Check(Re2djVfsReadFile(handle, contents, sizeof(contents), &read, nullptr) != FALSE,
              "cannot read original through VFS") &&
        Check(std::string(contents, read) == "original", "VFS read returned wrong original data") &&
        Check(Re2djVfsCloseHandle(handle) != FALSE, "cannot close original VFS handle");

    HBITMAP bitmap = static_cast<HBITMAP>(Re2djVfsLoadImageA(
        nullptr,
        "System\\CompanyLogo\\LOGO.BMP",
        IMAGE_BITMAP,
        0,
        0,
        LR_LOADFROMFILE));
    BITMAP bitmap_info = {};
    passed = passed &&
             Check(bitmap != nullptr, "cannot load relative bitmap through VFS") &&
             Check(GetObjectA(bitmap, sizeof(bitmap_info), &bitmap_info) == sizeof(bitmap_info),
                   "cannot inspect VFS-loaded bitmap") &&
             Check(bitmap_info.bmWidth == 1 && bitmap_info.bmHeight == 1,
                   "VFS-loaded bitmap has wrong dimensions");
    if (bitmap != nullptr)
    {
        DeleteObject(bitmap);
    }

    HANDLE script = Re2djVfsCreateFileA("System\\CompanyLogo\\logo.str",
                                        GENERIC_READ,
                                        FILE_SHARE_READ,
                                        nullptr,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr);
    char script_contents[16] = {};
    DWORD script_read = 0;
    passed = passed &&
             Check(script != INVALID_HANDLE_VALUE, "cannot open relative script through VFS") &&
             Check(Re2djVfsReadFile(script, script_contents, sizeof(script_contents), &script_read,
                                    nullptr) != FALSE,
                   "cannot read script through VFS") &&
             Check(std::string(script_contents, script_read) == "logostr",
                   "VFS read returned wrong script data") &&
             Check(Re2djVfsCloseHandle(script) != FALSE, "cannot close script VFS handle");

    // The original opens scene scripts with FILE_FLAG_NO_BUFFERING and then
    // reads the whole file, whose size is not a sector multiple. The VFS has to
    // drop that flag or the NT kernel rejects the read.
    HANDLE unbuffered = Re2djVfsCreateFileA("System\\CompanyLogo\\logo.str",
                                            GENERIC_READ,
                                            0,
                                            nullptr,
                                            OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING,
                                            nullptr);
    char unbuffered_contents[16] = {};
    DWORD unbuffered_read = 0;
    passed = passed &&
             Check(unbuffered != INVALID_HANDLE_VALUE,
                   "cannot open script with FILE_FLAG_NO_BUFFERING through VFS") &&
             Check(Re2djVfsReadFile(unbuffered, unbuffered_contents, sizeof(unbuffered_contents),
                                    &unbuffered_read, nullptr) != FALSE,
                   "unbuffered script read failed through VFS") &&
             Check(std::string(unbuffered_contents, unbuffered_read) == "logostr",
                   "unbuffered script read returned wrong data") &&
             Check(Re2djVfsCloseHandle(unbuffered) != FALSE,
                   "cannot close unbuffered script VFS handle");

    // A rooted guest path has no mapping, and the bounded trace this request
    // also produces must not overwrite the error the guest reads back.
    SetLastError(ERROR_SUCCESS);
    const HANDLE rejected = Re2djVfsCreateFileA("\\System\\CompanyLogo\\logo.str",
                                                GENERIC_READ,
                                                FILE_SHARE_READ,
                                                nullptr,
                                                OPEN_EXISTING,
                                                FILE_ATTRIBUTE_NORMAL,
                                                nullptr);
    const DWORD rejected_error = GetLastError();
    passed = passed &&
             Check(rejected == INVALID_HANDLE_VALUE, "unmapped script path was not rejected") &&
             Check(rejected_error == ERROR_INVALID_NAME,
                   "rejected script path reported the wrong Win32 error");

    std::string trace_contents;
    {
        std::ifstream trace_file(trace, std::ios::binary);
        trace_contents.assign(std::istreambuf_iterator<char>(trace_file),
                              std::istreambuf_iterator<char>());
    }
    passed = passed &&
             Check(trace_contents.find("api=CreateFileA") != std::string::npos,
                   "VFS trace is missing a CreateFileA marker") &&
             Check(trace_contents.find("api=LoadImageA") != std::string::npos,
                   "VFS trace is missing a LoadImageA marker") &&
             Check(trace_contents.find("logo.str") != std::string::npos,
                   "VFS trace is missing a script marker");

    DEVMODEA guest_mode = {};
    guest_mode.dmSize = sizeof(guest_mode);
    guest_mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    guest_mode.dmPelsWidth = 640;
    guest_mode.dmPelsHeight = 480;
    guest_mode.dmBitsPerPel = 16;
    SetLastError(ERROR_INVALID_FUNCTION);
    passed = passed &&
             Check(Re2djHleChangeDisplaySettingsExA(nullptr,
                                                     &guest_mode,
                                                     nullptr,
                                                     CDS_UPDATEREGISTRY,
                                                     nullptr) == DISP_CHANGE_SUCCESSFUL,
                   "logical display mode was not accepted") &&
             Check(GetLastError() == ERROR_SUCCESS,
                   "logical display mode did not clear last error");

    DEVMODEA unsupported_mode = guest_mode;
    unsupported_mode.dmPelsWidth = 1;
    unsupported_mode.dmPelsHeight = 1;
    unsupported_mode.dmBitsPerPel = 1;
    passed = passed &&
             Check(Re2djHleChangeDisplaySettingsExA("re2dj-invalid-display",
                                                     &unsupported_mode,
                                                     nullptr,
                                                     CDS_TEST,
                                                     nullptr) != DISP_CHANGE_SUCCESSFUL,
                   "nonmatching display request did not use host fallback");

    LPDIRECTDRAW direct_draw = nullptr;
    LPDIRECTDRAW4 direct_draw4 = nullptr;
    LPDIRECT3D3 direct3d = nullptr;
    IUnknown* draw_identity = nullptr;
    IUnknown* d3d_identity = nullptr;
    constexpr char graphics_window_class[] = "re2dj-runtime-probe-window";
    const HINSTANCE module = GetModuleHandleA(nullptr);
    WNDCLASSA window_class = {};
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = CaptionConsumingWindowProcedure;
    window_class.hInstance = module;
    window_class.lpszClassName = graphics_window_class;
    const ATOM window_class_atom = RegisterClassA(&window_class);
    HWND graphics_window = nullptr;
    if (window_class_atom != 0)
    {
        graphics_window = CreateWindowExA(0,
                                          graphics_window_class,
                                          "before",
                                          WS_POPUP,
                                          0,
                                          0,
                                          100,
                                          100,
                                          nullptr,
                                          nullptr,
                                          module,
                                          nullptr);
    }
    passed = passed &&
             Check(window_class_atom != 0 && graphics_window != nullptr,
                   "cannot create graphics policy probe window") &&
             Check(Re2djHleDirectDrawCreate(nullptr, &direct_draw, nullptr) == DD_OK &&
                       direct_draw != nullptr,
                   "cannot create DirectDraw HLE facade") &&
             Check(IDirectDraw_QueryInterface(direct_draw,
                                              IID_IDirectDraw4,
                                              reinterpret_cast<void**>(&direct_draw4)) == S_OK &&
                       direct_draw4 != nullptr,
                   "DirectDraw4 query failed") &&
             Check(IDirectDraw_QueryInterface(direct_draw,
                                              IID_IDirect3D3,
                                              reinterpret_cast<void**>(&direct3d)) == S_OK &&
                       direct3d != nullptr,
                   "Direct3D3 query failed") &&
             Check(IDirectDraw4_QueryInterface(direct_draw4,
                                               IID_IUnknown,
                                               reinterpret_cast<void**>(&draw_identity)) == S_OK &&
                       draw_identity != nullptr,
                   "DirectDraw COM identity query failed") &&
             Check(IDirect3D3_QueryInterface(direct3d,
                                             IID_IUnknown,
                                             reinterpret_cast<void**>(&d3d_identity)) == S_OK &&
                       d3d_identity == draw_identity,
                   "Direct3D COM identity is inconsistent");
    if (d3d_identity != nullptr)
    {
        IDirectDraw4_Release(reinterpret_cast<LPDIRECTDRAW4>(d3d_identity));
    }
    if (draw_identity != nullptr)
    {
        IDirectDraw4_Release(reinterpret_cast<LPDIRECTDRAW4>(draw_identity));
    }
    if (direct_draw != nullptr)
    {
        IDirectDraw_Release(direct_draw);
        direct_draw = nullptr;
    }

    D3DFINDDEVICESEARCH search = {};
    search.dwSize = sizeof(search);
    search.dwFlags = D3DFDS_HARDWARE;
    search.bHardware = TRUE;
    D3DFINDDEVICERESULT result = {};
    result.dwSize = sizeof(result);
    bool zbuffer_captured = false;
    LPDIRECTDRAWSURFACE4 primary = nullptr;
    LPDIRECTDRAWSURFACE4 back_buffer = nullptr;
    LPDIRECT3DDEVICE3 device = nullptr;
    LPDIRECT3DVIEWPORT3 viewport = nullptr;
    bool texture_format_captured = false;
    if (direct_draw4 != nullptr && direct3d != nullptr)
    {
        g_re2dj_fullscreen = FALSE;
        passed = passed &&
                 Check(IDirectDraw4_SetCooperativeLevel(
                           direct_draw4, graphics_window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) == DD_OK,
                       "logical DirectDraw cooperative level failed") &&
                 Check(IDirectDraw4_SetDisplayMode(direct_draw4, 640, 480, 16, 0, 0) == DD_OK,
                       "logical DirectDraw display mode failed") &&
                 Check(IDirect3D3_FindDevice(direct3d, &search, &result) == DD_OK &&
                           IsEqualGUID(result.guid, IID_IDirect3DHALDevice),
                       "virtual hardware FindDevice failed") &&
                 Check(IDirect3D3_EnumZBufferFormats(direct3d,
                                                    result.guid,
                                                    CaptureZBufferFormat,
                                                    &zbuffer_captured) == DD_OK &&
                           zbuffer_captured,
                       "virtual Z-buffer enumeration failed");

        char window_title[192] = {};
        RECT client_rectangle = {};
        RECT window_rectangle = {};
        BOOL non_client_rendering_enabled = TRUE;
        const LONG_PTR windowed_style = GetWindowLongPtrA(graphics_window, GWL_STYLE);
        GetWindowRect(graphics_window, &window_rectangle);
        const int caption_test_x =
            window_rectangle.left + (window_rectangle.right - window_rectangle.left) / 2;
        const int caption_test_y = window_rectangle.top + GetSystemMetrics(SM_CYSIZEFRAME) +
                                   GetSystemMetrics(SM_CYCAPTION) / 2;
        const int window_title_length =
            GetWindowTextA(graphics_window, window_title, sizeof(window_title));
        const bool window_title_valid =
            window_title_length != 0 &&
            std::strncmp(window_title, "re2DJ v", std::strlen("re2DJ v")) == 0 &&
            std::strstr(window_title, RE2DJ_VERSION) != nullptr &&
            std::strstr(window_title, " - Build ") != nullptr &&
            std::strstr(window_title, " - SDL3 OpenGL - FPS : 0.0") != nullptr;
        if (!window_title_valid)
        {
            std::fprintf(stderr,
                         "observed window title length=%d text=[%s]\n",
                         window_title_length,
                         window_title);
        }
        passed = passed &&
                 Check(window_title_valid, "window title policy failed") &&
                 Check((windowed_style & WS_CAPTION) == WS_CAPTION &&
                           (windowed_style & WS_SYSMENU) != 0 &&
                           (windowed_style & WS_THICKFRAME) != 0 &&
                           (windowed_style & WS_MAXIMIZEBOX) != 0,
                       "windowed style policy failed") &&
                 Check(SUCCEEDED(DwmGetWindowAttribute(
                           graphics_window,
                           DWMWA_NCRENDERING_ENABLED,
                           &non_client_rendering_enabled,
                           sizeof(non_client_rendering_enabled))) &&
                           non_client_rendering_enabled == FALSE,
                       "windowed DWM non-client policy failed") &&
                 Check(SendMessageA(graphics_window,
                                    WM_NCHITTEST,
                                    0,
                                    MAKELPARAM(caption_test_x, caption_test_y)) == HTCAPTION,
                       "window caption hit-test policy failed") &&
                 Check(SendMessageA(graphics_window, WM_GETICON, ICON_SMALL, 0) != 0,
                       "window caption icon policy failed") &&
                 Check(SendMessageA(graphics_window,
                                    WM_SETTEXT,
                                    0,
                                    reinterpret_cast<LPARAM>("")) != FALSE &&
                           SendMessageA(graphics_window, WM_SETICON, ICON_SMALL, 0) != FALSE &&
                           GetWindowTextA(
                               graphics_window, window_title, sizeof(window_title)) != 0 &&
                           std::strstr(window_title,
                                       " - SDL3 OpenGL - FPS : 0.0") != nullptr &&
                           SendMessageA(graphics_window, WM_GETICON, ICON_SMALL, 0) != 0,
                       "window caption overwrite guard failed") &&
                 Check(GetClientRect(graphics_window, &client_rectangle) != FALSE &&
                           client_rectangle.right - client_rectangle.left == 1280 &&
                           client_rectangle.bottom - client_rectangle.top == 960,
                       "windowed client size policy failed") &&
                 Check(Re2djUpdateWindowTitle(graphics_window, 59.94) != FALSE &&
                           GetWindowTextA(
                               graphics_window, window_title, sizeof(window_title)) != 0 &&
                           std::strstr(window_title, " - FPS : 59.9") != nullptr,
                       "window FPS title policy failed");

        g_re2dj_fullscreen = TRUE;
        passed = passed &&
                 Check(IDirectDraw4_SetCooperativeLevel(
                           direct_draw4, graphics_window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) == DD_OK,
                       "fullscreen DirectDraw cooperative level failed");
        RECT fullscreen_rectangle = {};
        MONITORINFO monitor_info = {};
        monitor_info.cbSize = sizeof(monitor_info);
        const HMONITOR graphics_monitor =
            MonitorFromWindow(graphics_window, MONITOR_DEFAULTTONEAREST);
        passed = passed &&
                 Check((GetWindowLongPtrA(graphics_window, GWL_STYLE) & WS_POPUP) != 0 &&
                           (GetWindowLongPtrA(graphics_window, GWL_STYLE) & WS_CAPTION) == 0,
                       "fullscreen style policy failed") &&
                 Check(SUCCEEDED(DwmGetWindowAttribute(
                           graphics_window,
                           DWMWA_NCRENDERING_ENABLED,
                           &non_client_rendering_enabled,
                           sizeof(non_client_rendering_enabled))) &&
                           non_client_rendering_enabled != FALSE,
                       "fullscreen DWM non-client policy failed") &&
                 Check(GetWindowRect(graphics_window, &fullscreen_rectangle) != FALSE &&
                           graphics_monitor != nullptr &&
                           GetMonitorInfoA(graphics_monitor, &monitor_info) != FALSE &&
                           EqualRect(&fullscreen_rectangle, &monitor_info.rcMonitor) != FALSE,
                       "fullscreen monitor bounds policy failed");
        g_re2dj_fullscreen = FALSE;
        passed = passed &&
                 Check(IDirectDraw4_SetCooperativeLevel(
                           direct_draw4, graphics_window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) == DD_OK,
                       "windowed DirectDraw policy restore failed");

        DDSURFACEDESC2 descriptor = {};
        descriptor.dwSize = sizeof(descriptor);
        descriptor.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        descriptor.dwBackBufferCount = 1;
        descriptor.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX |
                                    DDSCAPS_FLIP | DDSCAPS_VIDEOMEMORY;
        passed = passed &&
                 Check(IDirectDraw4_CreateSurface(direct_draw4,
                                                  &descriptor,
                                                  &primary,
                                                  nullptr) == DD_OK &&
                           primary != nullptr,
                       "logical primary surface creation failed");
        DDSCAPS2 back_caps = {};
        back_caps.dwCaps = DDSCAPS_BACKBUFFER;
        if (primary != nullptr)
        {
            passed = passed &&
                     Check(IDirectDrawSurface4_GetAttachedSurface(
                               primary, &back_caps, &back_buffer) == DD_OK &&
                               back_buffer != nullptr,
                           "logical back buffer lookup failed");
        }
        if (back_buffer != nullptr)
        {
            passed = passed &&
                     Check(IDirect3D3_CreateDevice(direct3d,
                                                   IID_IDirect3DHALDevice,
                                                   back_buffer,
                                                   &device,
                                                   nullptr) == DD_OK &&
                               device != nullptr,
                           "logical Direct3D3 device creation failed");
        }
        passed = passed &&
                 Check(IDirect3D3_CreateViewport(direct3d, &viewport, nullptr) == DD_OK &&
                           viewport != nullptr,
                       "logical Direct3D3 viewport creation failed");
        if (device != nullptr && viewport != nullptr)
        {
            D3DDEVICEDESC hardware_caps = {};
            D3DDEVICEDESC software_caps = {};
            hardware_caps.dwSize = sizeof(hardware_caps);
            software_caps.dwSize = sizeof(software_caps);
            D3DVIEWPORT2 viewport_state = {};
            viewport_state.dwSize = sizeof(viewport_state);
            viewport_state.dwWidth = 640;
            viewport_state.dwHeight = 480;
            viewport_state.dvClipX = -1.0f;
            viewport_state.dvClipY = 1.0f;
            viewport_state.dvClipWidth = 2.0f;
            viewport_state.dvClipHeight = 2.0f;
            viewport_state.dvMinZ = 0.0f;
            viewport_state.dvMaxZ = 1.0f;
            passed = passed &&
                     Check(IDirect3DDevice3_GetCaps(device,
                                                   &hardware_caps,
                                                   &software_caps) == DD_OK &&
                               (hardware_caps.dwDeviceRenderBitDepth & DDBD_16) != 0,
                           "virtual device capabilities failed") &&
                     Check(IDirect3DDevice3_EnumTextureFormats(device,
                                                              CaptureTextureFormat,
                                                              &texture_format_captured) == DD_OK &&
                               texture_format_captured,
                           "virtual texture format enumeration failed") &&
                     Check(IDirect3DDevice3_AddViewport(device, viewport) == DD_OK,
                           "device viewport attach failed") &&
                     Check(IDirect3DViewport3_SetViewport2(viewport, &viewport_state) == DD_OK,
                           "viewport state setup failed") &&
                     Check(IDirect3DDevice3_SetCurrentViewport(device, viewport) == DD_OK,
                           "current viewport setup failed") &&
                     Check(IDirect3DDevice3_SetTexture(device, 0, nullptr) == DD_OK,
                           "null texture reset failed");

            D3DVERTEX vertex_vertices[3] = {};
            vertex_vertices[0].x = -0.5f;
            vertex_vertices[0].y = -0.5f;
            vertex_vertices[1].x = 0.0f;
            vertex_vertices[1].y = 0.5f;
            vertex_vertices[2].x = 0.5f;
            vertex_vertices[2].y = -0.5f;
            for (D3DVERTEX& vertex : vertex_vertices)
            {
                vertex.z = 0.5f;
                vertex.nz = 1.0f;
            }
            D3DLVERTEX lit_vertices[3] = {};
            lit_vertices[0].x = -0.5f;
            lit_vertices[0].y = -0.5f;
            lit_vertices[1].x = 0.0f;
            lit_vertices[1].y = 0.5f;
            lit_vertices[2].x = 0.5f;
            lit_vertices[2].y = -0.5f;
            for (D3DLVERTEX& vertex : lit_vertices)
            {
                vertex.z = 0.5f;
                vertex.color = 0xffffffff;
            }
            passed = passed &&
                     Check(sizeof(D3DVERTEX) == 32 && sizeof(D3DLVERTEX) == 32 &&
                               IDirect3DDevice3_DrawPrimitive(device,
                                                              D3DPT_TRIANGLESTRIP,
                                                              D3DFVF_VERTEX,
                                                              vertex_vertices,
                                                              3,
                                                              0) == DD_OK &&
                               IDirect3DDevice3_DrawPrimitive(device,
                                                              D3DPT_TRIANGLESTRIP,
                                                              D3DFVF_LVERTEX,
                                                              lit_vertices,
                                                              3,
                                                              0) == DD_OK,
                           "untransformed Direct3D draw probe failed");
        }

        LPDIRECTDRAWSURFACE4 texture_source = nullptr;
        LPDIRECTDRAWSURFACE4 texture_destination = nullptr;
        LPDIRECTDRAWSURFACE4 texture_mismatch = nullptr;
        LPDIRECTDRAWSURFACE4 texture_copy_target = nullptr;
        LPDIRECT3DTEXTURE2 source_texture = nullptr;
        LPDIRECT3DTEXTURE2 destination_texture = nullptr;
        DDSURFACEDESC2 texture_descriptor = {};
        texture_descriptor.dwSize = sizeof(texture_descriptor);
        texture_descriptor.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
                                     DDSD_PIXELFORMAT;
        texture_descriptor.dwWidth = 3;
        texture_descriptor.dwHeight = 2;
        texture_descriptor.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
        texture_descriptor.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        texture_descriptor.ddpfPixelFormat.dwFlags = DDPF_RGB;
        texture_descriptor.ddpfPixelFormat.dwRGBBitCount = 16;
        texture_descriptor.ddpfPixelFormat.dwRBitMask = 0xf800;
        texture_descriptor.ddpfPixelFormat.dwGBitMask = 0x07e0;
        texture_descriptor.ddpfPixelFormat.dwBBitMask = 0x001f;
        passed = passed &&
                 Check(IDirectDraw4_CreateSurface(direct_draw4,
                                                  &texture_descriptor,
                                                  &texture_source,
                                                  nullptr) == DD_OK &&
                           IDirectDraw4_CreateSurface(direct_draw4,
                                                      &texture_descriptor,
                                                      &texture_destination,
                                                      nullptr) == DD_OK,
                       "texture Load probe surface creation failed");
        texture_descriptor.dwWidth = 2;
        passed = passed &&
                 Check(IDirectDraw4_CreateSurface(direct_draw4,
                                                  &texture_descriptor,
                                                  &texture_mismatch,
                                                  nullptr) == DD_OK,
                       "texture Load mismatch surface creation failed");
        texture_descriptor.dwWidth = 3;
        texture_descriptor.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        passed = passed &&
                 Check(IDirectDraw4_CreateSurface(direct_draw4,
                                                  &texture_descriptor,
                                                  &texture_copy_target,
                                                  nullptr) == DD_OK,
                       "texture Load color-key target creation failed");
        if (texture_source != nullptr && texture_destination != nullptr &&
            texture_mismatch != nullptr && texture_copy_target != nullptr)
        {
            DDBLTFX fill = {};
            fill.dwSize = sizeof(fill);
            fill.dwFillColor = 0xf800;
            DDCOLORKEY red_key = {0xf800, 0xf800};
            passed = passed &&
                     Check(IDirectDrawSurface4_Blt(texture_source,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   DDBLT_COLORFILL,
                                                   &fill) == DD_OK &&
                               IDirectDrawSurface4_SetColorKey(
                                   texture_source, DDCKEY_SRCBLT, &red_key) == DD_OK &&
                               IDirectDrawSurface4_QueryInterface(
                                   texture_source,
                                   IID_IDirect3DTexture2,
                                   reinterpret_cast<void**>(&source_texture)) == S_OK &&
                               IDirectDrawSurface4_QueryInterface(
                                   texture_destination,
                                   IID_IDirect3DTexture2,
                                   reinterpret_cast<void**>(&destination_texture)) == S_OK,
                           "texture Load probe setup failed");
            if (source_texture != nullptr && destination_texture != nullptr)
            {
                passed = passed &&
                         Check(IDirect3DTexture2_Load(destination_texture, nullptr) ==
                                   DDERR_INVALIDPARAMS &&
                                   IDirect3DTexture2_Load(destination_texture,
                                                          destination_texture) == DD_OK,
                               "texture Load null/self contract failed");
                LPDIRECT3DTEXTURE2 mismatch_texture = nullptr;
                passed = passed &&
                         Check(IDirectDrawSurface4_QueryInterface(
                                   texture_mismatch,
                                   IID_IDirect3DTexture2,
                                   reinterpret_cast<void**>(&mismatch_texture)) == S_OK &&
                                   mismatch_texture != nullptr &&
                                   IDirect3DTexture2_Load(destination_texture,
                                                          mismatch_texture) ==
                                       D3DERR_TEXTURE_LOAD_FAILED,
                               "texture Load size mismatch contract failed");
                if (mismatch_texture != nullptr)
                {
                    IDirect3DTexture2_Release(mismatch_texture);
                }
                passed = passed &&
                         Check(IDirect3DTexture2_Load(destination_texture, source_texture) ==
                                   DD_OK,
                               "texture Load RGB565 copy failed");
                HDC destination_dc = nullptr;
                if (IDirectDrawSurface4_GetDC(texture_destination, &destination_dc) == DD_OK)
                {
                    passed = passed &&
                             Check(GetPixel(destination_dc, 1, 1) == RGB(255, 0, 0),
                                   "texture Load copied incorrect RGB565 pixels");
                    IDirectDrawSurface4_ReleaseDC(texture_destination, destination_dc);
                }
                else
                {
                    passed = false;
                }

                fill.dwFillColor = 0x07e0;
                passed = passed &&
                         Check(IDirectDrawSurface4_Blt(texture_copy_target,
                                                       nullptr,
                                                       nullptr,
                                                       nullptr,
                                                       DDBLT_COLORFILL,
                                                       &fill) == DD_OK &&
                                   IDirectDrawSurface4_BltFast(texture_copy_target,
                                                               0,
                                                               0,
                                                               texture_destination,
                                                               nullptr,
                                                               DDBLTFAST_SRCCOLORKEY) == DD_OK,
                               "texture Load source color-key propagation failed");
                HDC target_dc = nullptr;
                if (IDirectDrawSurface4_GetDC(texture_copy_target, &target_dc) == DD_OK)
                {
                    passed = passed &&
                             Check(GetPixel(target_dc, 1, 1) == RGB(0, 255, 0),
                                   "texture Load did not preserve the source color key");
                    IDirectDrawSurface4_ReleaseDC(texture_copy_target, target_dc);
                }
                else
                {
                    passed = false;
                }
            }
        }
        if (destination_texture != nullptr)
        {
            IDirect3DTexture2_Release(destination_texture);
        }
        if (source_texture != nullptr)
        {
            IDirect3DTexture2_Release(source_texture);
        }
        if (texture_copy_target != nullptr)
        {
            IDirectDrawSurface4_Release(texture_copy_target);
        }
        if (texture_mismatch != nullptr)
        {
            IDirectDrawSurface4_Release(texture_mismatch);
        }
        if (texture_destination != nullptr)
        {
            IDirectDrawSurface4_Release(texture_destination);
        }
        if (texture_source != nullptr)
        {
            IDirectDrawSurface4_Release(texture_source);
        }

        IDirect3DVertexBuffer* vertex_buffer = nullptr;
        D3DVERTEXBUFFERDESC vertex_descriptor = {};
        vertex_descriptor.dwSize = sizeof(vertex_descriptor);
        vertex_descriptor.dwCaps = D3DVBCAPS_SYSTEMMEMORY;
        vertex_descriptor.dwFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1;
        vertex_descriptor.dwNumVertices = 4;
        passed = passed &&
                 Check(IDirect3D3_CreateVertexBuffer(direct3d,
                                                     &vertex_descriptor,
                                                     &vertex_buffer,
                                                     0,
                                                     nullptr) == DD_OK &&
                           vertex_buffer != nullptr,
                       "logical Direct3D3 vertex buffer creation failed");
        if (vertex_buffer != nullptr)
        {
            void* vertices = nullptr;
            passed = passed &&
                     Check(IDirect3DVertexBuffer_Lock(vertex_buffer,
                                                      0,
                                                      &vertices,
                                                      nullptr) == DD_OK &&
                               vertices != nullptr,
                           "vertex buffer nullable-size lock failed");
            if (vertices != nullptr)
            {
                std::memset(vertices, 0, 4 * 32);
            }
            passed = passed &&
                     Check(IDirect3DVertexBuffer_Unlock(vertex_buffer) == DD_OK,
                           "vertex buffer unlock failed");
            D3DVERTEXBUFFERDESC stored_descriptor = {};
            stored_descriptor.dwSize = sizeof(stored_descriptor);
            passed = passed &&
                     Check(IDirect3DVertexBuffer_GetVertexBufferDesc(vertex_buffer,
                                                                     &stored_descriptor) == DD_OK &&
                               stored_descriptor.dwFVF == vertex_descriptor.dwFVF &&
                               stored_descriptor.dwNumVertices == vertex_descriptor.dwNumVertices,
                           "vertex buffer descriptor query failed");
            IDirect3DVertexBuffer_Release(vertex_buffer);
        }
    }

    if (device != nullptr)
    {
        IDirect3DDevice3_Release(device);
    }
    if (viewport != nullptr)
    {
        IDirect3DViewport3_Release(viewport);
    }
    if (back_buffer != nullptr)
    {
        IDirectDrawSurface4_Release(back_buffer);
    }
    if (primary != nullptr)
    {
        IDirectDrawSurface4_Release(primary);
    }
    if (direct3d != nullptr)
    {
        IDirect3D3_Release(direct3d);
    }
    if (direct_draw4 != nullptr)
    {
        passed = passed &&
                 Check(IDirectDraw4_RestoreDisplayMode(direct_draw4) == DD_OK,
                       "logical display mode restore failed");
        IDirectDraw4_Release(direct_draw4);
    }
    if (graphics_window != nullptr)
    {
        DestroyWindow(graphics_window);
    }
    if (window_class_atom != 0)
    {
        UnregisterClassA(graphics_window_class, module);
    }

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

    g_re2dj_audio_master_gain = 2.0f;
    g_re2dj_audio_image_base = static_cast<DWORD>(
        reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr)));
    LPDIRECTSOUND direct_sound = nullptr;
    passed = passed && Check(Re2djHleDirectSoundCreate(nullptr, &direct_sound, nullptr) == DS_OK,
                             "DirectSound facade creation failed") &&
             Check(Re2djHleGetAudioMasterGain() == 2.0f,
                   "DirectSound master gain was not applied") &&
             Check(IDirectSound_SetCooperativeLevel(direct_sound, GetDesktopWindow(), DSSCL_NORMAL) == DS_OK,
                   "DirectSound cooperative level failed");
    WAVEFORMATEX wave = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};
    DSBUFFERDESC sound_desc = {};
    sound_desc.dwSize = sizeof(DSBUFFERDESC);
    sound_desc.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN;
    sound_desc.dwBufferBytes = 16;
    sound_desc.lpwfxFormat = &wave;
    LPDIRECTSOUNDBUFFER sound_buffer = nullptr;
    passed = passed && Check(IDirectSound_CreateSoundBuffer(direct_sound, &sound_desc, &sound_buffer, nullptr) == DS_OK,
                             "DirectSound secondary buffer creation failed");
    void* first = nullptr;
    DWORD first_bytes = 0;
    void* second = nullptr;
    DWORD second_bytes = 0;
    passed = passed && Check(IDirectSoundBuffer_Lock(sound_buffer, 0, 0, &first, &first_bytes,
                                                     &second, &second_bytes, DSBLOCK_ENTIREBUFFER) == DS_OK,
                             "DirectSound buffer lock failed");
    const std::int16_t pcm[] = {0, 8192, -8192, 16384, -16384, 32767, -32768, 0};
    if (first != nullptr) std::memcpy(first, pcm, (std::min)(sizeof(pcm), static_cast<std::size_t>(first_bytes)));
    if (second != nullptr) std::memset(second, 0, second_bytes);
    passed = passed && Check(IDirectSoundBuffer_Unlock(sound_buffer, first, first_bytes, second, second_bytes) == DS_OK,
                             "DirectSound buffer unlock failed") &&
             Check(IDirectSoundBuffer_SetCurrentPosition(sound_buffer, 0) == DS_OK,
                   "DirectSound buffer position failed") &&
             Check(IDirectSoundBuffer_Play(sound_buffer, 0, 0, 0) == DS_OK,
                   "DirectSound buffer play failed") &&
             Check(IDirectSoundBuffer_Stop(sound_buffer) == DS_OK,
                   "DirectSound buffer stop failed");
    LPDIRECTSOUNDBUFFER duplicate_buffer = nullptr;
    passed = passed &&
             Check(IDirectSoundBuffer_SetVolume(sound_buffer, -1200) == DS_OK,
                   "DirectSound source volume setup failed") &&
             Check(IDirectSound_DuplicateSoundBuffer(direct_sound,
                                                      sound_buffer,
                                                      &duplicate_buffer) == DS_OK &&
                       duplicate_buffer != nullptr,
                   "DirectSound buffer duplication failed");
    if (duplicate_buffer != nullptr)
    {
        LONG source_volume = 0;
        LONG duplicate_volume = 0;
        passed = passed &&
                 Check(IDirectSoundBuffer_GetVolume(duplicate_buffer, &duplicate_volume) == DS_OK &&
                           duplicate_volume == -1200,
                       "DirectSound duplicate did not inherit controls") &&
                 Check(IDirectSoundBuffer_SetVolume(duplicate_buffer, -2400) == DS_OK,
                       "DirectSound duplicate volume update failed") &&
                 Check(IDirectSoundBuffer_GetVolume(sound_buffer, &source_volume) == DS_OK &&
                           source_volume == -1200,
                       "DirectSound duplicate changed source controls") &&
                 Check(IDirectSoundBuffer_Play(duplicate_buffer, 0, 0, 0) == DS_OK,
                       "DirectSound duplicate play failed") &&
                 Check(IDirectSoundBuffer_Stop(duplicate_buffer) == DS_OK,
                       "DirectSound duplicate stop failed");
        IDirectSoundBuffer_Release(duplicate_buffer);
    }

    DSBUFFERDESC streaming_desc = sound_desc;
    streaming_desc.dwFlags = DSBCAPS_STATIC | DSBCAPS_LOCHARDWARE |
                             DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN |
                             DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2;
    streaming_desc.dwBufferBytes = 32;
    LPDIRECTSOUNDBUFFER streaming_buffer = nullptr;
    passed = passed &&
             Check(IDirectSound_CreateSoundBuffer(direct_sound,
                                                   &streaming_desc,
                                                   &streaming_buffer,
                                                   nullptr) == DS_OK,
                   "DirectSound streaming buffer creation failed");
    if (streaming_buffer != nullptr)
    {
        void* stream_first = nullptr;
        DWORD stream_first_bytes = 0;
        void* stream_second = nullptr;
        DWORD stream_second_bytes = 0;
        passed = passed &&
                 Check(IDirectSoundBuffer_Lock(streaming_buffer,
                                               0,
                                               0,
                                               &stream_first,
                                               &stream_first_bytes,
                                               &stream_second,
                                               &stream_second_bytes,
                                               DSBLOCK_ENTIREBUFFER) == DS_OK,
                       "DirectSound streaming initial lock failed");
        if (stream_first != nullptr) std::memset(stream_first, 0x11, stream_first_bytes);
        if (stream_second != nullptr) std::memset(stream_second, 0x11, stream_second_bytes);
        passed = passed &&
                 Check(IDirectSoundBuffer_Unlock(streaming_buffer,
                                                 stream_first,
                                                 stream_first_bytes,
                                                 stream_second,
                                                 stream_second_bytes) == DS_OK,
                       "DirectSound streaming initial unlock failed") &&
                 Check(IDirectSoundBuffer_SetCurrentPosition(streaming_buffer, 12) == DS_OK,
                       "DirectSound streaming initial position failed") &&
                 Check(IDirectSoundBuffer_Play(streaming_buffer, 0, 0, DSBPLAY_LOOPING) == DS_OK,
                       "DirectSound streaming play failed");

        stream_first = nullptr;
        stream_first_bytes = 0;
        stream_second = nullptr;
        stream_second_bytes = 0;
        passed = passed &&
                 Check(IDirectSoundBuffer_Lock(streaming_buffer,
                                               24,
                                               16,
                                               &stream_first,
                                               &stream_first_bytes,
                                               &stream_second,
                                               &stream_second_bytes,
                                               0) == DS_OK,
                       "DirectSound streaming wrap lock failed") &&
                 Check(stream_first_bytes == 8 && stream_second_bytes == 8,
                       "DirectSound streaming wrap regions are incorrect");
        if (stream_first != nullptr) std::memset(stream_first, 0x22, stream_first_bytes);
        if (stream_second != nullptr) std::memset(stream_second, 0x33, stream_second_bytes);
        passed = passed &&
                 Check(IDirectSoundBuffer_Unlock(streaming_buffer,
                                                 stream_first,
                                                 stream_first_bytes,
                                                 stream_second,
                                                 stream_second_bytes) == DS_OK,
                       "DirectSound streaming wrap unlock failed");
        DWORD streaming_cursor = 0;
        passed = passed &&
                 Check(IDirectSoundBuffer_GetCurrentPosition(streaming_buffer,
                                                             &streaming_cursor,
                                                             nullptr) == DS_OK &&
                           streaming_cursor < streaming_desc.dwBufferBytes,
                       "DirectSound streaming cursor is outside the ring") &&
                 Check(IDirectSoundBuffer_Stop(streaming_buffer) == DS_OK,
                       "DirectSound streaming stop failed") &&
                 Check(IDirectSoundBuffer_Play(streaming_buffer, 0, 0, DSBPLAY_LOOPING) == DS_OK,
                       "DirectSound streaming restart failed") &&
                 Check(IDirectSoundBuffer_Stop(streaming_buffer) == DS_OK,
                       "DirectSound streaming second stop failed");
        IDirectSoundBuffer_Release(streaming_buffer);
    }
    if (sound_buffer != nullptr) IDirectSoundBuffer_Release(sound_buffer);
    if (direct_sound != nullptr) IDirectSound_Release(direct_sound);

    std::ifstream audio_trace_stream(audio_trace, std::ios::binary);
    const std::string audio_trace_text((std::istreambuf_iterator<char>(audio_trace_stream)),
                                       std::istreambuf_iterator<char>());
    passed = passed &&
             Check(audio_trace_text.find("ini:demo-volume configured=3") != std::string::npos,
                   "audio trace omitted demo volume override") &&
             Check(audio_trace_text.find("directsound:create-device master-linear=2.000000000") != std::string::npos,
                   "audio trace omitted master gain") &&
             Check(audio_trace_text.find("directsound:set-volume") != std::string::npos &&
                       audio_trace_text.find("applied=-1200") != std::string::npos &&
                       audio_trace_text.find("caller-rva=0x") != std::string::npos &&
                       audio_trace_text.find("track-gain=") != std::string::npos,
                   "audio trace omitted buffer volume") &&
             Check(audio_trace_text.find("directsound:first-play") != std::string::npos &&
                       audio_trace_text.find("peak=1.000000000") != std::string::npos &&
                       audio_trace_text.find("rms=") != std::string::npos,
                   "audio trace omitted PCM statistics") &&
             Check(audio_trace_text.find("directsound:streaming-start") != std::string::npos &&
                       audio_trace_text.find("streaming=1") != std::string::npos,
                   "audio trace omitted streaming start") &&
             Check(audio_trace_text.find("lock-offset=24 first=8 second=8") != std::string::npos &&
                       audio_trace_text.find("dirty-offset=24 dirty-bytes=16") != std::string::npos &&
                       audio_trace_text.find("backend-refresh=1") != std::string::npos,
                   "audio trace omitted streaming wrap refresh");
    audio_trace_stream.close();

    std::filesystem::remove_all(root);
    return passed ? 0 : 1;
}
