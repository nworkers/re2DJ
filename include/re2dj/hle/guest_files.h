#ifndef RE2DJ_HLE_GUEST_FILES_H_
#define RE2DJ_HLE_GUEST_FILES_H_

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/guest_handles.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/storage/guest_path.h"

namespace re2dj::hle
{

// Where guest files come from: the CHD image, read-only, with writes going to
// a host overlay directory, as on the Windows path.
struct GuestFileConfig
{
    // Empty when the run has no CHD, which provides no files.
    std::filesystem::path chd_image;
    // The product's directory inside the image, for example "EZ2DJ".
    std::string chd_root;
    // The guest's own name for that directory, for example "D:\ez2dj"; it is
    // also the guest's current directory.
    std::string guest_root;
    // Host directory writes go to, mirroring the directory under chd_root.
    std::filesystem::path overlay_root;
};

// The read-only image guest files come from, below chd_root. Production uses
// the CHD's FAT32 volume; tests supply their own.
class GuestFileSource
{
public:
    virtual ~GuestFileSource() = default;
    // Case-insensitive lookup of a '/'-separated path.
    virtual bool Find(std::string_view relative_path, bool* directory, std::uint64_t* size) const = 0;
    virtual bool ReadRange(std::string_view relative_path,
                           std::uint64_t offset,
                           void* destination,
                           std::size_t length) const = 0;
    // Copies the whole file to a host path.
    virtual bool Materialize(std::string_view relative_path, const std::filesystem::path& output) const = 0;
};

// CreateFile dispositions (fileapi.h).
inline constexpr std::uint32_t kCreateNew = 1;
inline constexpr std::uint32_t kCreateAlways = 2;
inline constexpr std::uint32_t kOpenExisting = 3;
inline constexpr std::uint32_t kOpenAlways = 4;
inline constexpr std::uint32_t kTruncateExisting = 5;

// The guest's open files. Reads come from the overlay when it holds the file,
// otherwise from the CHD; opening for write first copies the CHD file into
// the overlay. Handles come from the guest's shared handle space. Results are
// Win32 error codes, 0 for success.
class GuestFiles
{
public:
    // What Open did: a handle, or 0 with a Win32 error; outside_root marks a
    // path this model does not serve, which the caller stops on.
    struct OpenResult
    {
        std::uint32_t handle = 0;
        std::uint32_t error = 0;
        bool outside_root = false;
    };

    GuestFiles() = default;
    ~GuestFiles();
    GuestFiles(const GuestFiles&) = delete;
    GuestFiles& operator=(const GuestFiles&) = delete;

    // Opens the CHD. With an empty chd_image the set provides no files.
    bool Configure(GuestFileConfig config, std::string* error);
    // Uses source in place of the CHD; config.chd_image is ignored.
    bool Configure(GuestFileConfig config, std::unique_ptr<GuestFileSource> source, std::string* error);
    bool configured() const { return source_ != nullptr; }
    void SetHandleAllocator(GuestHandleAllocator* allocator) { handles_ = allocator; }

    OpenResult Open(std::string_view guest_path, bool read, bool write, std::uint32_t disposition);
    bool IsOpen(std::uint32_t handle) const;
    std::uint32_t Read(std::uint32_t handle, std::uint32_t size, std::vector<std::uint8_t>* bytes);
    std::uint32_t Write(std::uint32_t handle, std::span<const std::uint8_t> bytes);
    // SetFilePointer's move: method 0 begin, 1 current, 2 end.
    std::uint32_t Seek(std::uint32_t handle, std::int64_t distance, std::uint32_t method,
                       std::uint64_t* position);
    std::uint32_t Size(std::uint32_t handle, std::uint64_t* size) const;
    bool Close(std::uint32_t handle);

private:
    struct File
    {
        // CHD files are read in place; overlay files are host FILE streams.
        std::string chd_relative;
        std::FILE* host = nullptr;
        std::uint64_t size = 0;
        std::uint64_t position = 0;
        bool read = false;
        bool write = false;
    };

    // The path below the guest root, '/'-separated, or false when the path
    // lies outside it.
    bool RelativeToRoot(std::string_view guest_path, std::string* relative) const;
    std::uint32_t OpenHost(const std::filesystem::path& path, bool write, bool truncate, File* file) const;

    GuestFileConfig config_;
    std::unique_ptr<GuestFileSource> source_;
    storage::GuestPath root_;
    GuestHandleAllocator own_handles_;
    GuestHandleAllocator* handles_ = nullptr;
    std::map<std::uint32_t, File> files_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_FILES_H_
