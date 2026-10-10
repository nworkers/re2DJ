#ifndef RE2DJ_UPDATE_UPDATE_INSTALL_H_
#define RE2DJ_UPDATE_UPDATE_INSTALL_H_

#include "re2dj/update/semantic_version.h"

#include <filesystem>
#include <string>
#include <vector>

namespace re2dj::update
{

// #17 (rePIU #48). Whether `install_folder` is one this build may replace files in:
// a `VERSION` file there must name `build_version`. Every release archive puts
// one next to the executables; a build tree has none next to its outputs, so
// a developer's tree is never overwritten with release binaries. False with
// the reason otherwise, including when the folder cannot be written.
bool CanInstallInto(const std::filesystem::path& install_folder,
                    const SemanticVersion& build_version, std::string* reason);

// Moves each staged file (relative to `staging`) over its counterpart in
// `install_folder`. An existing file is first renamed to `<name>.re2dj-old`,
// which also works for the running executable: Linux allows renaming it, and
// Windows allows renaming but not overwriting it. Any failure moves every file
// back and returns false, leaving the install as it was. The renamed-aside
// files are listed in `<install_folder>/.re2dj-old-files` for the next start.
bool InstallStagedFiles(const std::filesystem::path& staging,
                        const std::vector<std::filesystem::path>& files,
                        const std::filesystem::path& install_folder,
                        std::string* error);

// Deletes the files a previous install renamed aside. One still in use -- on
// Windows the old launcher waits for the new one -- stays listed for the next
// start. Returns how many were removed.
std::size_t RemovePreviousInstallLeftovers(
    const std::filesystem::path& install_folder);

}  // namespace re2dj::update

#endif  // RE2DJ_UPDATE_UPDATE_INSTALL_H_
