#ifndef RE2DJ_STORAGE_VFS_FILE_TABLE_H_
#define RE2DJ_STORAGE_VFS_FILE_TABLE_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "re2dj/storage/vfs_roots.h"

namespace re2dj::storage
{

enum class VfsOpenMode
{
    kRead,
    kWrite,
    kReadWrite,
    kCreate,
};

class VfsFileTable
{
public:
    explicit VfsFileTable(VfsRoots roots);

    std::uint32_t Open(const std::string& guest_path,
                       VfsOpenMode mode,
                       std::string* error);
    bool Read(std::uint32_t handle, void* buffer, std::size_t size,
              std::size_t* transferred, std::string* error);
    bool Write(std::uint32_t handle, const void* buffer, std::size_t size,
               std::size_t* transferred, std::string* error);
    bool Seek(std::uint32_t handle, std::int64_t distance,
              std::uint32_t method, std::uint64_t* position, std::string* error);
    bool Size(std::uint32_t handle, std::uint64_t* size, std::string* error) const;
    bool Close(std::uint32_t handle, std::string* error);

private:
    struct Entry
    {
        std::filesystem::path path;
        VfsOpenMode mode = VfsOpenMode::kRead;
        std::uint64_t position = 0;
    };

    VfsRoots roots_;
    std::uint32_t next_handle_ = 4;
    std::unordered_map<std::uint32_t, Entry> entries_;
};

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_VFS_FILE_TABLE_H_
