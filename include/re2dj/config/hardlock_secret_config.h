#ifndef RE2DJ_CONFIG_HARDLOCK_SECRET_CONFIG_H_
#define RE2DJ_CONFIG_HARDLOCK_SECRET_CONFIG_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace re2dj::config
{

// Hardlock material the user supplies from outside the repository. re2DJ
// computes none of it: the responses are produced by a separate program and
// this layer only locates and reads the files.
struct HardlockSecretMaterial
{
    // Device replay values, kept as the same hex text the launcher options
    // accept so a single parser validates both sources. Empty means the key was
    // absent and the corresponding option stays unset.
    std::string handshake_response_hex;
    std::string descriptor_tail_hex;
};

std::filesystem::path DefaultHardlockSecretConfigPath();

// Convention path for a profile's externally computed response map. Nothing in
// this repository produces the file; a profile default reads it when present.
std::filesystem::path DefaultHardlockTransformMapPath(std::string_view profile_id);

bool IsHardlockSecretPathAllowed(const std::filesystem::path& path);

// Reads the profile's section when both the file and the section exist, with
// every key optional. A missing file or section is not a failure: `found`
// reports whether anything was read, and only a malformed file returns false.
bool LoadHardlockProfileMaterial(const std::filesystem::path& path,
                                 std::string_view profile_id,
                                 HardlockSecretMaterial* material,
                                 bool* found,
                                 std::string* error);

}  // namespace re2dj::config

#endif  // RE2DJ_CONFIG_HARDLOCK_SECRET_CONFIG_H_
