#ifndef RE2DJ_LOGGING_LOGGING_H_
#define RE2DJ_LOGGING_LOGGING_H_

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace spdlog
{
class logger;
}

namespace re2dj::logging
{

struct LoggerOptions
{
    std::string name = "re2dj";
    std::filesystem::path file_path;
};

bool Initialize(const LoggerOptions& options,
                std::filesystem::path* actual_file_path,
                std::string* error);

std::shared_ptr<spdlog::logger> GetLogger();

// The guest API call log: every facade call with its inputs and responses,
// written only to the file next to the main log ("<name>.api.log"). Null
// before Initialize, so probes that never initialize record nothing.
std::shared_ptr<spdlog::logger> GetApiLogger();

void Fatal(std::string_view classification, std::string_view message);

void Shutdown();

}  // namespace re2dj::logging

#endif  // RE2DJ_LOGGING_LOGGING_H_
