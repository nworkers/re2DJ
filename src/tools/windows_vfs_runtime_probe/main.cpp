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
extern "C" __declspec(dllimport) HANDLE WINAPI Re2djVfsCreateFileA(
    LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD flags, HANDLE template_handle);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsReadFile(
    HANDLE handle, LPVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsWriteFile(
    HANDLE handle, LPCVOID buffer, DWORD size, LPDWORD transferred, LPOVERLAPPED overlapped);
extern "C" __declspec(dllimport) BOOL WINAPI Re2djVfsCloseHandle(HANDLE handle);

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
    std::filesystem::remove_all(root);
    return passed ? 0 : 1;
}
