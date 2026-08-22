#ifndef RE2DJ_STORAGE_VFS_ROOTS_H_
#define RE2DJ_STORAGE_VFS_ROOTS_H_

#include <filesystem>
#include <string>

#include "re2dj/hdd/hdd_root.h"
#include "re2dj/storage/guest_path.h"

namespace re2dj::storage
{

struct VfsRoots
{
    const hdd::HddRoot* hdd = nullptr;
    std::filesystem::path overlay_root;
    std::filesystem::path support_root;
    GuestPath current_directory;
};

bool ResolveVfsPath(const VfsRoots& roots,
                   const GuestPath& requested,
                   bool write,
                   std::filesystem::path* host_path,
                   std::string* error);

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_VFS_ROOTS_H_
