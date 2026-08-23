#ifndef RE2DJ_DEVICE_LPTDI_RESPONSE_PROFILE_H_
#define RE2DJ_DEVICE_LPTDI_RESPONSE_PROFILE_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace re2dj::device
{

constexpr std::uint32_t kLptdiIoctlCode410 = 0x9c406410;
constexpr std::uint32_t kLptdiIoctlCode414 = 0x9c406414;
constexpr std::size_t kLptdiIoctlResponseSize410 = 8;
constexpr std::size_t kLptdiIoctlResponseSize414 = 104;

struct LptdiResponseEntry
{
    std::uint32_t control_code = 0;
    std::vector<std::uint8_t> bytes;
};

struct LptdiResponseProfile
{
    std::vector<LptdiResponseEntry> entries;
};

bool ReadLptdiResponseProfile(const std::filesystem::path& path,
                              LptdiResponseProfile* profile,
                              std::string* error);

const LptdiResponseEntry* FindLptdiResponse(const LptdiResponseProfile& profile,
                                            std::uint32_t control_code);

}  // namespace re2dj::device

#endif  // RE2DJ_DEVICE_LPTDI_RESPONSE_PROFILE_H_
