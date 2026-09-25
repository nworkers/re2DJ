#include "re2dj/hle/guest_files.h"

#include <algorithm>
#include <system_error>
#include <utility>

#include "re2dj/hle/win32_errors.h"

namespace re2dj::hle
{
namespace
{

std::filesystem::path HostPath(const std::filesystem::path& root, std::string_view relative)
{
    std::filesystem::path path = root;
    std::size_t start = 0;
    while (start <= relative.size())
    {
        const std::size_t slash = relative.find('/', start);
        const std::size_t end = slash == std::string_view::npos ? relative.size() : slash;
        if (end > start)
        {
            path /= std::string(relative.substr(start, end - start));
        }
        if (slash == std::string_view::npos)
        {
            break;
        }
        start = slash + 1;
    }
    return path;
}

// The CHD's FAT32 volume as a file source.
class Fat32FileSource final : public GuestFileSource
{
public:
    explicit Fat32FileSource(std::unique_ptr<storage::Fat32Volume> volume) : volume_(std::move(volume)) {}

    bool Find(std::string_view relative_path, bool* directory, std::uint64_t* size) const override
    {
        storage::Fat32Entry entry;
        std::string error;
        if (!volume_->Find(relative_path, &entry, &error))
        {
            return false;
        }
        *directory = entry.directory;
        *size = entry.size;
        return true;
    }
    bool ReadRange(std::string_view relative_path,
                   std::uint64_t offset,
                   void* destination,
                   std::size_t length) const override
    {
        std::string error;
        return volume_->ReadFileRange(relative_path, offset, destination, length, &error);
    }
    bool Materialize(std::string_view relative_path, const std::filesystem::path& output) const override
    {
        std::string error;
        return volume_->MaterializeFile(relative_path, output, &error);
    }

private:
    std::unique_ptr<storage::Fat32Volume> volume_;
};

}  // namespace

GuestFiles::~GuestFiles()
{
    for (auto& [handle, file] : files_)
    {
        if (file.host != nullptr)
        {
            std::fclose(file.host);
        }
    }
}

bool GuestFiles::Configure(GuestFileConfig config, std::string* error)
{
    if (config.chd_image.empty())
    {
        config_ = std::move(config);
        source_.reset();
        return true;
    }
    std::unique_ptr<storage::Fat32Volume> volume;
    if (!storage::Fat32Volume::Open(config.chd_image, &volume, error))
    {
        return false;
    }
    return Configure(std::move(config), std::make_unique<Fat32FileSource>(std::move(volume)), error);
}

bool GuestFiles::Configure(GuestFileConfig config,
                           std::unique_ptr<GuestFileSource> source,
                           std::string* error)
{
    config_ = std::move(config);
    source_.reset();
    if (!storage::ParseGuestPath(config_.guest_root, &root_) ||
        root_.kind != storage::GuestPathKind::kDriveAbsolute || !storage::NormalizeGuestPath(&root_))
    {
        if (error != nullptr) *error = "guest root is not a drive-absolute path: " + config_.guest_root;
        return false;
    }
    source_ = std::move(source);
    return true;
}

bool GuestFiles::RelativeToRoot(std::string_view guest_path, std::string* relative) const
{
    storage::GuestPath parsed;
    storage::GuestPath combined;
    if (!storage::ParseGuestPath(guest_path, &parsed) ||
        !storage::CombineGuestPath(root_, parsed, &combined) ||
        combined.drive_letter != root_.drive_letter ||
        combined.components.size() <= root_.components.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < root_.components.size(); ++index)
    {
        if (!storage::EqualsIgnoreAsciiCase(combined.components[index], root_.components[index]))
        {
            return false;
        }
    }
    combined.components.erase(combined.components.begin(),
                              combined.components.begin() +
                                  static_cast<std::ptrdiff_t>(root_.components.size()));
    *relative = storage::GuestPathToRelativeString(combined);
    return true;
}

std::uint32_t GuestFiles::OpenHost(const std::filesystem::path& path,
                                   bool write,
                                   bool truncate,
                                   File* file) const
{
    const char* mode = !write ? "rb" : truncate ? "w+b" : "r+b";
    std::FILE* stream = std::fopen(path.string().c_str(), mode);
    if (stream == nullptr)
    {
        return kWin32ErrorAccessDenied;
    }
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    file->host = stream;
    file->size = code ? 0 : static_cast<std::uint64_t>(size);
    return kWin32ErrorSuccess;
}

GuestFiles::OpenResult GuestFiles::Open(std::string_view guest_path,
                                        bool read,
                                        bool write,
                                        std::uint32_t disposition)
{
    OpenResult result;
    std::string relative;
    if (source_ == nullptr || !RelativeToRoot(guest_path, &relative))
    {
        result.outside_root = true;
        return result;
    }
    const std::string chd_relative = config_.chd_root.empty() ? relative : config_.chd_root + "/" + relative;
    bool chd_directory = false;
    std::uint64_t chd_size = 0;
    const bool in_chd = source_->Find(chd_relative, &chd_directory, &chd_size);
    const std::filesystem::path overlay = HostPath(config_.overlay_root, relative);
    std::error_code code;
    const bool in_overlay = std::filesystem::is_regular_file(overlay, code);
    if ((in_chd && chd_directory) || std::filesystem::is_directory(overlay, code))
    {
        result.error = kWin32ErrorAccessDenied;
        return result;
    }
    const bool exists = in_overlay || in_chd;
    if (disposition < kCreateNew || disposition > kTruncateExisting)
    {
        result.error = kWin32ErrorInvalidParameter;
        return result;
    }
    if (disposition == kCreateNew && exists)
    {
        result.error = kWin32ErrorFileExists;
        return result;
    }
    if ((disposition == kOpenExisting || disposition == kTruncateExisting) && !exists)
    {
        result.error = kWin32ErrorFileNotFound;
        return result;
    }

    File file;
    file.read = read;
    file.write = write;
    const bool truncate = disposition == kCreateAlways || disposition == kTruncateExisting;
    if (!write && !truncate)
    {
        if (in_overlay)
        {
            result.error = OpenHost(overlay, false, false, &file);
        }
        else if (in_chd)
        {
            file.chd_relative = chd_relative;
            file.size = chd_size;
        }
        else
        {
            // OPEN_ALWAYS or CREATE_NEW without write access still creates.
            std::filesystem::create_directories(overlay.parent_path(), code);
            result.error = OpenHost(overlay, true, true, &file);
        }
    }
    else
    {
        std::filesystem::create_directories(overlay.parent_path(), code);
        // Copy on write: the overlay starts as the CHD's copy.
        if (!in_overlay && in_chd && !truncate && !source_->Materialize(chd_relative, overlay))
        {
            result.error = kWin32ErrorAccessDenied;
            return result;
        }
        result.error = OpenHost(overlay, true, truncate || !exists, &file);
    }
    if (result.error != kWin32ErrorSuccess)
    {
        return result;
    }
    result.handle = handles_ != nullptr ? handles_->Allocate() : own_handles_.Allocate();
    files_.emplace(result.handle, std::move(file));
    // CREATE_ALWAYS and OPEN_ALWAYS on an existing file succeed with
    // ERROR_ALREADY_EXISTS as the last error.
    result.error = exists && (disposition == kCreateAlways || disposition == kOpenAlways)
                       ? kWin32ErrorAlreadyExists
                       : kWin32ErrorSuccess;
    return result;
}

bool GuestFiles::IsOpen(std::uint32_t handle) const
{
    return files_.count(handle) == 1;
}

std::uint32_t GuestFiles::Read(std::uint32_t handle,
                               std::uint32_t size,
                               std::vector<std::uint8_t>* bytes)
{
    const auto found = files_.find(handle);
    if (found == files_.end())
    {
        return kWin32ErrorInvalidHandle;
    }
    File& file = found->second;
    if (!file.read)
    {
        return kWin32ErrorAccessDenied;
    }
    const std::uint64_t available = file.position >= file.size ? 0 : file.size - file.position;
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(size, available));
    bytes->assign(count, 0);
    if (count != 0)
    {
        const bool read = file.host != nullptr
                              ? std::fseek(file.host, static_cast<long>(file.position), SEEK_SET) == 0 &&
                                    std::fread(bytes->data(), 1, count, file.host) == count
                              : source_->ReadRange(file.chd_relative, file.position, bytes->data(), count);
        if (!read)
        {
            bytes->clear();
            return kWin32ErrorReadFault;
        }
    }
    file.position += count;
    return kWin32ErrorSuccess;
}

std::uint32_t GuestFiles::Write(std::uint32_t handle, std::span<const std::uint8_t> bytes)
{
    const auto found = files_.find(handle);
    if (found == files_.end())
    {
        return kWin32ErrorInvalidHandle;
    }
    File& file = found->second;
    if (!file.write || file.host == nullptr)
    {
        return kWin32ErrorAccessDenied;
    }
    if (!bytes.empty() &&
        (std::fseek(file.host, static_cast<long>(file.position), SEEK_SET) != 0 ||
         std::fwrite(bytes.data(), 1, bytes.size(), file.host) != bytes.size() ||
         std::fflush(file.host) != 0))
    {
        return kWin32ErrorWriteFault;
    }
    file.position += bytes.size();
    file.size = std::max(file.size, file.position);
    return kWin32ErrorSuccess;
}

std::uint32_t GuestFiles::Seek(std::uint32_t handle,
                               std::int64_t distance,
                               std::uint32_t method,
                               std::uint64_t* position)
{
    const auto found = files_.find(handle);
    if (found == files_.end())
    {
        return kWin32ErrorInvalidHandle;
    }
    File& file = found->second;
    std::int64_t base = 0;
    switch (method)
    {
    case 0:
        break;
    case 1:
        base = static_cast<std::int64_t>(file.position);
        break;
    case 2:
        base = static_cast<std::int64_t>(file.size);
        break;
    default:
        return kWin32ErrorInvalidParameter;
    }
    if (base + distance < 0)
    {
        return kWin32ErrorNegativeSeek;
    }
    file.position = static_cast<std::uint64_t>(base + distance);
    *position = file.position;
    return kWin32ErrorSuccess;
}

std::uint32_t GuestFiles::Size(std::uint32_t handle, std::uint64_t* size) const
{
    const auto found = files_.find(handle);
    if (found == files_.end())
    {
        return kWin32ErrorInvalidHandle;
    }
    *size = found->second.size;
    return kWin32ErrorSuccess;
}

bool GuestFiles::Close(std::uint32_t handle)
{
    const auto found = files_.find(handle);
    if (found == files_.end())
    {
        return false;
    }
    if (found->second.host != nullptr)
    {
        std::fclose(found->second.host);
    }
    files_.erase(found);
    return true;
}

}  // namespace re2dj::hle
