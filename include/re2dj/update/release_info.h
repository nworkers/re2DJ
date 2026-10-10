#ifndef RE2DJ_UPDATE_RELEASE_INFO_H_
#define RE2DJ_UPDATE_RELEASE_INFO_H_

#include "re2dj/update/semantic_version.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace re2dj::update
{

// #17 (rePIU #48). What the launcher needs from GitHub's latest-release response
// (`GET /repos/{owner}/{repo}/releases/latest`).
struct ReleaseAsset
{
    std::string name;
    std::string download_url;
    std::uint64_t size = 0;
    // Lower-case hex from `digest: "sha256:<hex>"`; empty when GitHub gave no
    // SHA-256, which makes the asset unusable for an install.
    std::string sha256;
};

struct ReleaseInfo
{
    std::string tag;
    SemanticVersion version;
    std::string page_url;
    std::vector<ReleaseAsset> assets;
};

// Parses the response. False, with `error` saying why, for malformed JSON or a
// tag that is not `v<major.minor.patch>`; an asset missing a field is dropped
// rather than failing the release.
bool ParseLatestRelease(std::string_view json, ReleaseInfo* release,
                        std::string* error);

// The archive this build installs from: `re2DJ-v<ver>-windows-x86.zip` for Win/x86,
// `-linux-x64.tar.gz` for Linux/x64, `-linux-x86.tar.gz` for Linux/x86, from
// the labels RE2DJ_HOST_OS_LABEL and RE2DJ_HOST_ARCH_LABEL give (version.h).
// Nothing for any other build.
[[nodiscard]] std::optional<std::string> ReleaseAssetName(
    const SemanticVersion& version, std::string_view platform,
    std::string_view architecture);

// The asset of that name, compared without case, or null.
[[nodiscard]] const ReleaseAsset* FindReleaseAsset(const ReleaseInfo& release,
                                                   std::string_view name);

}  // namespace re2dj::update

#endif  // RE2DJ_UPDATE_RELEASE_INFO_H_
