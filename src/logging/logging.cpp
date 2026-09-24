#include "re2dj/logging/logging.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>

#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace re2dj::logging
{
namespace
{

constexpr char kLogPattern[] = "[%X.%e] [%8l] [%n] %v";

std::mutex g_logger_mutex;
std::shared_ptr<spdlog::logger> g_logger;

std::filesystem::path MakeDefaultLogPath()
{
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::uint64_t millisecond =
        static_cast<std::uint64_t>(milliseconds.count()) % 1000U;
    return std::filesystem::path("logs") /
           fmt::format("re2dj-{:%Y%m%d-%H%M%S}-{:03}.log",
                       fmt::localtime(time),
                       millisecond);
}

std::shared_ptr<spdlog::logger> MakeBootstrapLogger()
{
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("re2dj-bootstrap", sink);
    logger->set_pattern(kLogPattern);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);
    return logger;
}

}  // namespace

bool Initialize(const LoggerOptions& options,
                std::filesystem::path* actual_file_path,
                std::string* error)
{
    if (actual_file_path == nullptr || error == nullptr || options.name.empty())
    {
        if (error != nullptr)
        {
            *error = "logger name and output parameters are required";
        }
        return false;
    }

    const std::filesystem::path file_path =
        options.file_path.empty() ? MakeDefaultLogPath() : options.file_path;
    std::error_code filesystem_error;
    const std::filesystem::path parent = file_path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, filesystem_error);
    }
    if (filesystem_error)
    {
        *error = "cannot create log directory: " + filesystem_error.message();
        return false;
    }

    try
    {
        auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.string(), true);
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>(
            options.name, sinks.begin(), sinks.end());
        logger->set_pattern(kLogPattern);
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);

        std::lock_guard<std::mutex> lock(g_logger_mutex);
        if (g_logger != nullptr)
        {
            g_logger->flush();
        }
        g_logger = std::move(logger);
    }
    catch (const std::exception& exception)
    {
        *error = "cannot initialize spdlog sinks: " + std::string(exception.what());
        return false;
    }

    *actual_file_path = file_path;
    error->clear();
    return true;
}

std::shared_ptr<spdlog::logger> GetLogger()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    if (g_logger == nullptr)
    {
        g_logger = MakeBootstrapLogger();
    }
    return g_logger;
}

void Fatal(std::string_view classification, std::string_view message)
{
    const std::shared_ptr<spdlog::logger> logger = GetLogger();
    logger->critical("FATAL {} {}", classification, message);
    logger->flush();
}

void Shutdown()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    if (g_logger != nullptr)
    {
        g_logger->flush();
        g_logger.reset();
    }
}

}  // namespace re2dj::logging
