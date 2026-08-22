#ifndef RE2DJ_PLATFORM_WINDOWS_RUNTIME_EXPORT_LOCATOR_H_
#define RE2DJ_PLATFORM_WINDOWS_RUNTIME_EXPORT_LOCATOR_H_

#include <cstdint>
#include <filesystem>
#include <string>

namespace re2dj::platform::windows
{

bool FindPe32ExportRva(const std::filesystem::path& path,
                       const char* name,
                       std::uint32_t* rva,
                       std::string* error);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_RUNTIME_EXPORT_LOCATOR_H_
