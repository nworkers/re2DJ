#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "message_box_boundary.h"

namespace re2dj::platform::windows
{
namespace
{

MessageBoxBoundarySink g_sink = nullptr;
int g_result = IDOK;
volatile LONG g_installed = 0;
volatile LONG g_capture_count = 0;

// A resource-id argument arrives as a small integer rather than a pointer.
bool IsStringPointer(const void* value)
{
    return reinterpret_cast<std::uintptr_t>(value) > 0xffffu;
}

void ReportCapture(const char* which, const char* text, const char* caption, UINT type)
{
    if (g_sink == nullptr)
    {
        return;
    }
    char message[1024] = {};
    const int length = std::snprintf(message,
                                     sizeof(message),
                                     "re2dj:hle:message-box:%s:index=%ld:type=0x%08lx:"
                                     "result=%d:caption=%s:text=%s\n",
                                     which,
                                     static_cast<long>(InterlockedIncrement(&g_capture_count)),
                                     static_cast<unsigned long>(type),
                                     g_result,
                                     caption,
                                     text);
    if (length <= 0)
    {
        return;
    }
    OutputDebugStringA(message);
    g_sink(message);
}

int WINAPI RecordMessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type)
{
    (void)owner;
    ReportCapture("ansi",
                  IsStringPointer(text) ? text : "<resource-id>",
                  IsStringPointer(caption) ? caption : "<resource-id>",
                  type);
    return g_result;
}

// Wide text is narrowed for the log only; nothing is handed back to the guest.
void NarrowForLog(LPCWSTR source, char* destination, int destination_size)
{
    destination[0] = '\0';
    if (!IsStringPointer(source))
    {
        std::snprintf(destination, static_cast<std::size_t>(destination_size), "<resource-id>");
        return;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, source, -1, destination, destination_size, nullptr, nullptr) == 0)
    {
        std::snprintf(destination, static_cast<std::size_t>(destination_size), "<unconvertible>");
    }
}

int WINAPI RecordMessageBoxW(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type)
{
    (void)owner;
    char narrow_text[512] = {};
    char narrow_caption[256] = {};
    NarrowForLog(text, narrow_text, static_cast<int>(sizeof(narrow_text)));
    NarrowForLog(caption, narrow_caption, static_cast<int>(sizeof(narrow_caption)));
    ReportCapture("wide", narrow_text, narrow_caption, type);
    return g_result;
}

// Overwrites the first five bytes of `target` with a relative jump to
// `replacement`. The recorder never calls the original, so no trampoline for
// the displaced prologue is needed.
bool RedirectEntryPoint(void* target, void* replacement)
{
    if (target == nullptr || replacement == nullptr)
    {
        return false;
    }
    constexpr SIZE_T kPatchSize = 5;
    DWORD previous_protection = 0;
    if (VirtualProtect(target, kPatchSize, PAGE_EXECUTE_READWRITE, &previous_protection) == FALSE)
    {
        return false;
    }
    auto* const bytes = static_cast<unsigned char*>(target);
    const std::int32_t relative = static_cast<std::int32_t>(
        reinterpret_cast<std::intptr_t>(replacement) -
        (reinterpret_cast<std::intptr_t>(target) + static_cast<std::intptr_t>(kPatchSize)));
    bytes[0] = 0xe9;
    std::memcpy(bytes + 1, &relative, sizeof(relative));
    DWORD restored_protection = 0;
    VirtualProtect(target, kPatchSize, previous_protection, &restored_protection);
    FlushInstructionCache(GetCurrentProcess(), target, kPatchSize);
    return true;
}

}  // namespace

bool InstallMessageBoxBoundary(MessageBoxBoundarySink sink, int result)
{
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0)
    {
        return true;
    }
    g_sink = sink;
    g_result = result;

    const HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32 == nullptr)
    {
        InterlockedExchange(&g_installed, 0);
        return false;
    }
    const FARPROC ansi_entry = GetProcAddress(user32, "MessageBoxA");
    const FARPROC wide_entry = GetProcAddress(user32, "MessageBoxW");
    const bool ansi_redirected =
        RedirectEntryPoint(reinterpret_cast<void*>(ansi_entry),
                           reinterpret_cast<void*>(&RecordMessageBoxA));
    const bool wide_redirected =
        RedirectEntryPoint(reinterpret_cast<void*>(wide_entry),
                           reinterpret_cast<void*>(&RecordMessageBoxW));
    if (!ansi_redirected && !wide_redirected)
    {
        InterlockedExchange(&g_installed, 0);
        return false;
    }
    if (sink != nullptr)
    {
        char message[192] = {};
        std::snprintf(message,
                      sizeof(message),
                      "re2dj:hle:message-box:installed:ansi=%d:wide=%d:result=%d\n",
                      ansi_redirected ? 1 : 0,
                      wide_redirected ? 1 : 0,
                      result);
        OutputDebugStringA(message);
        sink(message);
    }
    return true;
}

}  // namespace re2dj::platform::windows
