#include "re2dj/storage/vfs_roots.h"

#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace re2dj::storage
{
namespace
{

bool ResolveOverlayFile(const std::filesystem::path& root,
                        const std::vector<std::string>& components,
                        std::filesystem::path* resolved)
{
    if (resolved == nullptr)
    {
        return false;
    }

    std::filesystem::path current = root;
    for (const std::string& component : components)
    {
        std::error_code code;
        std::filesystem::directory_iterator iterator(current, code);
        if (code)
        {
            return false;
        }

        std::filesystem::path exact;
        std::filesystem::path folded;
        const std::filesystem::directory_iterator end;
        for (; iterator != end; iterator.increment(code))
        {
            if (code)
            {
                return false;
            }
            const std::string name = iterator->path().filename().string();
            if (name == component)
            {
                exact = iterator->path();
                break;
            }
            if (folded.empty() && EqualsIgnoreAsciiCase(name, component))
            {
                folded = iterator->path();
            }
        }

        current = !exact.empty() ? std::move(exact) : std::move(folded);
        if (current.empty())
        {
            return false;
        }
    }

    std::error_code code;
    if (!std::filesystem::is_regular_file(current, code) || code)
    {
        return false;
    }
    *resolved = std::move(current);
    return true;
}

}  // namespace

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
        std::filesystem::path overlay_path;
        if (ResolveOverlayFile(roots.overlay_root, relative_path.components, &overlay_path))
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
