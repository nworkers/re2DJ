#ifndef RE2DJ_CONFIG_HARDLOCK_SECRET_CONFIG_H_
#define RE2DJ_CONFIG_HARDLOCK_SECRET_CONFIG_H_

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace re2dj::config
{

struct HardlockSecretMaterial
{
    std::uint16_t module_address = 0;
    std::array<std::uint16_t, 3> seeds = {};
};

std::filesystem::path DefaultHardlockSecretConfigPath();

bool IsHardlockSecretPathAllowed(const std::filesystem::path& path);

bool LoadHardlockSecretConfig(const std::filesystem::path& path,
                              std::string_view profile_id,
                              HardlockSecretMaterial* material,
                              std::string* error);

}  // namespace re2dj::config

#endif  // RE2DJ_CONFIG_HARDLOCK_SECRET_CONFIG_H_
