#include "runtime_log.h"

#include <array>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

extern "C" __declspec(dllexport) char g_re2dj_runtime_log_path[MAX_PATH] = {};

// Defined next to the code that owns each trace.
extern "C" char g_re2dj_vfs_trace_path[MAX_PATH];
extern "C" char g_re2dj_graphics_trace_path[MAX_PATH];
extern "C" char g_re2dj_audio_trace_path[MAX_PATH];

namespace re2dj::platform::windows
{
namespace
{

constexpr char kRecordPattern[] = "[%H:%M:%S.%e] [%t] %v";

// One channel's file logger, kept for the path it was opened with.
struct ChannelLog
{
    SRWLOCK lock = SRWLOCK_INIT;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    // The path that last failed to open, so it is not retried every line.
    std::string failed_path;
};

std::array<ChannelLog, 4> g_channels;

const char* ChannelPath(RuntimeLogChannel channel)
{
    switch (channel)
    {
    case RuntimeLogChannel::kRuntime:
        return g_re2dj_runtime_log_path;
    case RuntimeLogChannel::kVfs:
        return g_re2dj_vfs_trace_path;
    case RuntimeLogChannel::kGraphics:
        return g_re2dj_graphics_trace_path;
    case RuntimeLogChannel::kAudio:
        return g_re2dj_audio_trace_path;
    }
    return "";
}

const char* ChannelName(RuntimeLogChannel channel)
{
    switch (channel)
    {
    case RuntimeLogChannel::kRuntime:
        return "runtime";
    case RuntimeLogChannel::kVfs:
        return "vfs";
    case RuntimeLogChannel::kGraphics:
        return "graphics";
    case RuntimeLogChannel::kAudio:
        return "audio";
    }
    return "runtime";
}

// The logger for the channel's current path: opened on first use or after the
// path changes, closed when the path is cleared. Called under the lock.
spdlog::logger* CurrentLogger(RuntimeLogChannel channel, ChannelLog* log)
{
    const std::string_view path(ChannelPath(channel),
                                strnlen(ChannelPath(channel), MAX_PATH));
    if (path != log->path)
    {
        log->logger.reset();
        log->path.assign(path);
    }
    if (log->logger != nullptr || log->path.empty() || log->path == log->failed_path)
    {
        return log->logger.get();
    }
    try
    {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log->path, false);
        log->logger = std::make_shared<spdlog::logger>(ChannelName(channel), sink);
        log->logger->set_pattern(kRecordPattern);
        log->logger->set_level(spdlog::level::trace);
        log->logger->flush_on(spdlog::level::trace);
    }
    catch (const std::exception&)
    {
        log->logger.reset();
        log->failed_path = log->path;
    }
    return log->logger.get();
}

}  // namespace

void WriteRuntimeLog(RuntimeLogChannel channel, const char* message)
{
    if (message == nullptr)
    {
        return;
    }
    const DWORD last_error = GetLastError();
    if (channel == RuntimeLogChannel::kRuntime)
    {
        OutputDebugStringA(message);
    }
    std::size_t length = std::strlen(message);
    while (length != 0 && (message[length - 1] == '\n' || message[length - 1] == '\r'))
    {
        --length;
    }
    ChannelLog& log = g_channels[static_cast<std::size_t>(channel)];
    AcquireSRWLockExclusive(&log.lock);
    try
    {
        if (spdlog::logger* logger = CurrentLogger(channel, &log); logger != nullptr)
        {
            logger->info("{}", std::string_view(message, length));
        }
    }
    catch (const std::exception&)
    {
        // A record that cannot be written is dropped; the guest runs on.
    }
    ReleaseSRWLockExclusive(&log.lock);
    SetLastError(last_error);
}

}  // namespace re2dj::platform::windows

extern "C" __declspec(dllexport) void Re2djCloseRuntimeLogs()
{
    for (auto& log : re2dj::platform::windows::g_channels)
    {
        AcquireSRWLockExclusive(&log.lock);
        log.logger.reset();
        log.path.clear();
        ReleaseSRWLockExclusive(&log.lock);
    }
}
