#include "re2dj/storage/vfs_file_table.h"

#include <fstream>
#include <limits>

namespace re2dj::storage
{

VfsFileTable::VfsFileTable(VfsRoots roots) : roots_(std::move(roots))
{
}

std::uint32_t VfsFileTable::Open(const std::string& guest_path,
                                 VfsOpenMode mode,
                                 std::string* error)
{
    GuestPath parsed;
    if (error == nullptr || !ParseGuestPath(guest_path, &parsed))
    {
        if (error != nullptr) *error = "invalid guest file path";
        return 0;
    }
    std::filesystem::path host_path;
    const bool write = mode != VfsOpenMode::kRead;
    if (!ResolveVfsPath(roots_, parsed, write, &host_path, error))
    {
        return 0;
    }
    std::error_code code;
    if (write)
    {
        std::filesystem::create_directories(host_path.parent_path(), code);
        if (code)
        {
            *error = "cannot create overlay parent directory";
            return 0;
        }
        if (mode == VfsOpenMode::kCreate)
        {
            std::ofstream create(host_path, std::ios::binary | std::ios::app);
            if (!create)
            {
                *error = "cannot create overlay file";
                return 0;
            }
        }
    }
    else if (!std::filesystem::is_regular_file(host_path, code) || code)
    {
        *error = "guest file does not exist";
        return 0;
    }
    const std::uint32_t handle = next_handle_++;
    entries_.emplace(handle, Entry{host_path, mode, 0});
    return handle;
}

bool VfsFileTable::Read(std::uint32_t handle, void* buffer, std::size_t size,
                        std::size_t* transferred, std::string* error)
{
    auto found = entries_.find(handle);
    if (found == entries_.end() || buffer == nullptr || transferred == nullptr || error == nullptr)
    {
        if (error != nullptr) *error = "invalid VFS file handle";
        return false;
    }
    std::ifstream stream(found->second.path, std::ios::binary);
    stream.seekg(static_cast<std::streamoff>(found->second.position));
    stream.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    *transferred = static_cast<std::size_t>(stream.gcount());
    found->second.position += *transferred;
    return true;
}

bool VfsFileTable::Write(std::uint32_t handle, const void* buffer, std::size_t size,
                         std::size_t* transferred, std::string* error)
{
    auto found = entries_.find(handle);
    if (found == entries_.end() || buffer == nullptr || transferred == nullptr || error == nullptr ||
        found->second.mode == VfsOpenMode::kRead)
    {
        if (error != nullptr) *error = "invalid or read-only VFS file handle";
        return false;
    }
    std::fstream stream(found->second.path, std::ios::binary | std::ios::in | std::ios::out);
    if (!stream)
    {
        *error = "cannot open overlay file for writing";
        return false;
    }
    stream.seekp(static_cast<std::streamoff>(found->second.position));
    stream.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
    if (!stream)
    {
        *error = "cannot write overlay file";
        return false;
    }
    *transferred = size;
    found->second.position += size;
    return true;
}

bool VfsFileTable::Seek(std::uint32_t handle, std::int64_t distance,
                        std::uint32_t method, std::uint64_t* position, std::string* error)
{
    auto found = entries_.find(handle);
    if (found == entries_.end() || position == nullptr || error == nullptr)
    {
        if (error != nullptr) *error = "invalid VFS file handle";
        return false;
    }
    std::uint64_t base = method == 1 ? found->second.position : 0;
    if (method == 2)
    {
        if (!Size(handle, &base, error)) return false;
    }
    const std::uint64_t magnitude = distance < 0
                                        ? static_cast<std::uint64_t>(-(distance + 1)) + 1
                                        : static_cast<std::uint64_t>(distance);
    if (distance < 0 && magnitude > base)
    {
        *error = "seek moved before file start";
        return false;
    }
    if (distance >= 0 && magnitude > (std::numeric_limits<std::uint64_t>::max)() - base)
    {
        *error = "seek moved beyond maximum file position";
        return false;
    }
    found->second.position = distance < 0 ? base - magnitude : base + magnitude;
    *position = found->second.position;
    return true;
}

bool VfsFileTable::Size(std::uint32_t handle, std::uint64_t* size, std::string* error) const
{
    const auto found = entries_.find(handle);
    if (found == entries_.end() || size == nullptr || error == nullptr)
    {
        if (error != nullptr) *error = "invalid VFS file handle";
        return false;
    }
    std::error_code code;
    *size = std::filesystem::file_size(found->second.path, code);
    if (code)
    {
        *error = "cannot query VFS file size";
        return false;
    }
    return true;
}

bool VfsFileTable::Close(std::uint32_t handle, std::string* error)
{
    if (error == nullptr || entries_.erase(handle) == 0)
    {
        if (error != nullptr) *error = "invalid VFS file handle";
        return false;
    }
    return true;
}

}  // namespace re2dj::storage
