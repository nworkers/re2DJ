#define NOMINMAX
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "graphics_trace_log.h"

// The launcher resolves this export in the injected runtime and writes the
// diagnostic log's sibling ".ddraw.log" path into it before the guest reaches
// its graphics initialization. It stays an exported symbol with this exact
// name because the launcher looks it up by name.
extern "C" __declspec(dllexport) char g_re2dj_graphics_trace_path[MAX_PATH] = {};

namespace re2dj::platform::windows
{
namespace
{

// One append handle shared by every caller. Opening per line would serialize
// on the file system for traces that run per frame.
HANDLE g_trace_file = INVALID_HANDLE_VALUE;
SRWLOCK g_trace_lock = SRWLOCK_INIT;

void AppendTraceFile(const char* message, std::size_t length)
{
    if (g_re2dj_graphics_trace_path[0] == '\0' || length == 0)
    {
        return;
    }
    AcquireSRWLockExclusive(&g_trace_lock);
    if (g_trace_file == INVALID_HANDLE_VALUE)
    {
        g_trace_file = CreateFileA(g_re2dj_graphics_trace_path,
                                   FILE_APPEND_DATA,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr,
                                   OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
    }
    if (g_trace_file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(g_trace_file, message, static_cast<DWORD>(length), &written, nullptr);
        constexpr char kNewline[] = "\r\n";
        WriteFile(g_trace_file,
                  kNewline,
                  static_cast<DWORD>(sizeof(kNewline) - 1),
                  &written,
                  nullptr);
    }
    ReleaseSRWLockExclusive(&g_trace_lock);
}

}  // namespace

void WriteGraphicsTraceLine(const char* message)
{
    if (message == nullptr)
    {
        return;
    }
    OutputDebugStringA(message);
    AppendTraceFile(message, std::strlen(message));
}

void WriteGraphicsTraceFormat(const char* format, ...)
{
    if (format == nullptr)
    {
        return;
    }
    char message[1024] = {};
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (length <= 0)
    {
        return;
    }
    WriteGraphicsTraceLine(message);
}

void ReportUnimplementedGraphicsCall(const char* interface_name,
                                     GraphicsCallLedger* ledger)
{
    if (interface_name == nullptr || ledger == nullptr || ledger->method == nullptr)
    {
        return;
    }
    // The ledger lives in a function-local static shared by every thread that
    // reaches the slot, so the budget is decremented atomically. Going negative
    // is harmless; the comparison is what stops the writes.
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&ledger->remaining)) < 0)
    {
        return;
    }
    WriteGraphicsTraceFormat(
        "re2dj:hle:%s::%s:not-implemented", interface_name, ledger->method);
}

}  // namespace re2dj::platform::windows
