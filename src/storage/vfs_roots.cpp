#include "re2dj/storage/vfs_roots.h"

#include <utility>

namespace re2dj::storage
{

bool ResolveVfsPath(const VfsRoots& roots,
                   const GuestPath& requested,
                   bool write,
                   std::filesystem::path* host_path,
                   std::string* error)
{
    if (roots.hdd == nullptr || host_path == nullptr || error == nullptr ||
        roots.current_directory.kind != GuestPathKind::kDriveAbsolute)
    {
        if (error != nullptr) *error = "invalid VFS roots";
        return false;
    }
    GuestPath combined;
    if (!CombineGuestPath(roots.current_directory, requested, &combined))
    {
        *error = "guest path escapes or uses an unsupported drive";
        return false;
    }
    const bool support = combined.drive_letter == 'C' &&
                         !combined.components.empty() &&
                         EqualsIgnoreAsciiCase(combined.components.front(), "windows");
    const bool guest_hdd = combined.drive_letter == 'D' &&
                           combined.components.size() >= 1 &&
                           EqualsIgnoreAsciiCase(combined.components.front(), "ez2dj");
    if (!support && !guest_hdd)
    {
        *error = "guest path is outside mounted C:\\windows or D:\\ez2dj roots";
        return false;
    }
    GuestPath relative_path = combined;
    relative_path.components.erase(relative_path.components.begin());
    const std::string mounted_relative = GuestPathToRelativeString(relative_path);
    if (!write)
    {
        const std::filesystem::path overlay_path =
            roots.overlay_root / std::filesystem::path(mounted_relative);
        std::error_code overlay_code;
        if (std::filesystem::is_regular_file(overlay_path, overlay_code) && !overlay_code)
        {
            *host_path = overlay_path;
            return true;
        }
    }
    const std::filesystem::path& base = support ? roots.support_root :
                                               (write ? roots.overlay_root : roots.hdd->root());
    if (!support && !write)
    {
        if (!roots.hdd->Resolve(mounted_relative, host_path))
        {
            *error = "guest HDD file was not found";
            return false;
        }
        return true;
    }
    *host_path = base / std::filesystem::path(mounted_relative);
    return true;
}

}  // namespace re2dj::storage
