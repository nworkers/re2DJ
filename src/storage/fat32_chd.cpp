#include "re2dj/storage/fat32_chd.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#include "re2dj/storage/fat32_directory_name.h"
#include "re2dj/storage/guest_path.h"

namespace re2dj::storage
{
namespace
{

constexpr std::uint32_t kFat32EndOfChain = 0x0ffffff8;
constexpr std::uint32_t kFat32BadCluster = 0x0ffffff7;
constexpr std::uint32_t kFat32ReservedStart = 0x0ffffff0;

std::uint16_t ReadU16(const std::uint8_t* bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t ReadU32(const std::uint8_t* bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool IsFat32PartitionType(std::uint8_t type)
{
    return type == 0x0b || type == 0x0c || type == 0x1b || type == 0x1c;
}

std::string TrimPadded(const std::uint8_t* bytes, std::size_t length)
{
    std::size_t end = length;
    while (end != 0 && (bytes[end - 1] == ' ' || bytes[end - 1] == '\0'))
    {
        --end;
    }
    return std::string(reinterpret_cast<const char*>(bytes), end);
}

std::vector<std::string> SplitPath(std::string_view path)
{
    std::vector<std::string> components;
    std::string current;
    for (const char value : path)
    {
        if (value == '/' || value == '\\')
        {
            if (!current.empty())
            {
                components.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(value);
    }
    if (!current.empty())
    {
        components.push_back(std::move(current));
    }
    return components;
}

bool IsSafeRelativePath(std::string_view path)
{
    for (const std::string& component : SplitPath(path))
    {
        if (component == "." || component == "..")
        {
            return false;
        }
    }
    return path.empty() || (path.front() != '/' && path.front() != '\\');
}

// FAT name matching is case-insensitive and accepts either separator, so the
// lookup key folds ASCII case and normalizes separators. Two spellings of the
// same path then share one cache entry.
std::string MakeLookupKey(std::string_view path)
{
    std::string key;
    key.reserve(path.size());
    for (const char value : path)
    {
        if (value == '\\')
        {
            key.push_back('/');
            continue;
        }
        key.push_back(value >= 'A' && value <= 'Z'
                          ? static_cast<char>(value - 'A' + 'a')
                          : value);
    }
    return key;
}

// Cache bounds. A guest that probes many distinct paths must not be able to
// grow these without limit, and clearing wholesale is acceptable because every
// entry can be rebuilt from the read-only image.
constexpr std::size_t kDirectoryCacheEntries = 512;
constexpr std::size_t kPathCacheEntries = 4096;
constexpr std::size_t kChainCacheEntries = 256;

}  // namespace

bool ParseFat32BootSector(const std::uint8_t* boot,
                          std::size_t boot_size,
                          std::uint64_t volume_lba,
                          std::uint64_t volume_sectors,
                          Fat32VolumeInfo* info)
{
    if (boot == nullptr || info == nullptr || boot_size < 512 || boot[510] != 0x55 ||
        boot[511] != 0xaa)
    {
        return false;
    }
    const std::uint32_t bytes_per_sector = ReadU16(boot, 11);
    const std::uint32_t sectors_per_cluster = boot[13];
    const std::uint32_t reserved_sectors = ReadU16(boot, 14);
    const std::uint32_t fat_count = boot[16];
    const std::uint32_t total_sectors16 = ReadU16(boot, 19);
    const std::uint32_t total_sectors32 = ReadU32(boot, 32);
    const std::uint32_t sectors_per_fat = ReadU32(boot, 36);
    const std::uint32_t root_cluster = ReadU32(boot, 44);
    const std::uint64_t total_sectors =
        total_sectors32 != 0 ? total_sectors32 : total_sectors16;
    if (bytes_per_sector != 512 || sectors_per_cluster == 0 ||
        (sectors_per_cluster & (sectors_per_cluster - 1)) != 0 ||
        sectors_per_cluster > 128 || reserved_sectors == 0 || fat_count == 0 ||
        sectors_per_fat == 0 || total_sectors == 0 || total_sectors > volume_sectors ||
        root_cluster < 2)
    {
        return false;
    }
    const std::uint64_t fat_sectors = static_cast<std::uint64_t>(fat_count) * sectors_per_fat;
    if (reserved_sectors + fat_sectors >= total_sectors)
    {
        return false;
    }
    const std::uint64_t data_sectors = total_sectors - reserved_sectors - fat_sectors;
    const std::uint64_t cluster_count = data_sectors / sectors_per_cluster;
    if (cluster_count == 0 || cluster_count > (std::numeric_limits<std::uint32_t>::max)() - 1)
    {
        return false;
    }
    const std::uint32_t maximum_cluster = static_cast<std::uint32_t>(cluster_count + 1);
    const std::uint64_t fat_entries =
        static_cast<std::uint64_t>(sectors_per_fat) * bytes_per_sector / 4;
    if (maximum_cluster >= fat_entries || root_cluster > maximum_cluster)
    {
        return false;
    }

    info->partition_lba = volume_lba;
    info->partition_sectors = volume_sectors;
    info->bytes_per_sector = bytes_per_sector;
    info->sectors_per_cluster = sectors_per_cluster;
    info->reserved_sectors = reserved_sectors;
    info->fat_count = fat_count;
    info->sectors_per_fat = sectors_per_fat;
    info->root_cluster = root_cluster;
    info->data_lba = volume_lba + reserved_sectors + fat_sectors;
    info->cluster_count = static_cast<std::uint32_t>(cluster_count);
    info->maximum_cluster = maximum_cluster;
    info->volume_label = TrimPadded(boot + 71, 11);
    info->filesystem_type = TrimPadded(boot + 82, 8);
    return true;
}

bool Fat32Volume::Open(const std::filesystem::path& chd_path,
                       std::unique_ptr<Fat32Volume>* out,
                       std::string* error)
{
    const auto fail = [error](std::string message)
    {
        if (error != nullptr)
        {
            *error = std::move(message);
        }
        return false;
    };
    if (out == nullptr || chd_path.empty())
    {
        return fail("CHD path or output is empty");
    }

    std::unique_ptr<MameChdImage> image;
    if (!MameChdImage::Open(chd_path, &image, error))
    {
        return false;
    }
    if (image->info().unit_bytes != 512 || image->info().logical_bytes / 512 == 0)
    {
        return fail("FAT32 volume requires a 512-byte CHD logical sector");
    }

    const std::uint64_t image_sectors = image->info().logical_bytes / 512;

    std::vector<std::uint8_t> first;
    if (!image->ReadSector(0, &first, error) || first.size() < 512 || first[510] != 0x55 ||
        first[511] != 0xaa)
    {
        return fail("CHD does not contain a valid boot signature");
    }

    const auto build_volume = [](const std::vector<std::uint8_t>& boot,
                                 std::uint64_t volume_lba,
                                 std::uint64_t volume_sectors,
                                 Fat32VolumeInfo* info) {
        return ParseFat32BootSector(boot.data(), boot.size(), volume_lba, volume_sectors, info);
    };

    Fat32VolumeInfo info;
    bool accepted = false;
    for (std::uint32_t index = 0; index < 4 && !accepted; ++index)
    {
        const std::size_t offset = 446 + index * 16;
        if (!IsFat32PartitionType(first[offset + 4]))
        {
            continue;
        }
        const std::uint64_t partition_lba = ReadU32(first.data(), offset + 8);
        const std::uint64_t partition_sectors = ReadU32(first.data(), offset + 12);
        if (partition_sectors == 0 || partition_lba >= image_sectors ||
            partition_sectors > image_sectors - partition_lba)
        {
            continue;
        }
        std::vector<std::uint8_t> boot;
        if (!image->ReadSector(partition_lba, &boot, error))
        {
            return false;
        }
        if (build_volume(boot, partition_lba, partition_sectors, &info))
        {
            info.partitioned = true;
            info.partition_index = index;
            accepted = true;
        }
    }
    if (!accepted)
    {
        // No usable partition entry. The image may be a whole-disk volume, in
        // which case the sector already read is the boot sector itself; the
        // ez2dj5th CHD is formatted that way.
        if (build_volume(first, 0, image_sectors, &info))
        {
            info.partitioned = false;
            info.partition_index = 0;
            accepted = true;
        }
    }
    if (!accepted)
    {
        return fail(
            "CHD holds no FAT32 volume: no partition entry describes one, and "
            "sector 0 is not a FAT32 boot sector either");
    }

    *out = std::unique_ptr<Fat32Volume>(new Fat32Volume(std::move(image), std::move(info)));
    return true;
}

bool Fat32Volume::ReadSector(std::uint64_t lba,
                             std::vector<std::uint8_t>* sector,
                             std::string* error) const
{
    if (lba >= image_->info().logical_bytes / 512)
    {
        if (error != nullptr)
        {
            *error = "FAT32 sector is outside the CHD image";
        }
        return false;
    }
    return image_->ReadSector(lba, sector, error);
}

bool Fat32Volume::ReadCluster(std::uint32_t cluster,
                              std::vector<std::uint8_t>* bytes,
                              std::string* error) const
{
    if (cluster < 2 || cluster > info_.maximum_cluster || bytes == nullptr)
    {
        if (error != nullptr)
        {
            *error = "FAT32 cluster is outside the data region";
        }
        return false;
    }
    const std::uint64_t lba =
        info_.data_lba + static_cast<std::uint64_t>(cluster - 2) * info_.sectors_per_cluster;
    bytes->assign(static_cast<std::size_t>(info_.sectors_per_cluster) * 512, 0);
    return image_->Read(lba * 512, bytes->data(), bytes->size(), error);
}

bool Fat32Volume::ReadFatEntry(std::uint32_t cluster,
                               std::uint32_t* value,
                               std::string* error) const
{
    if (value == nullptr || cluster < 2 || cluster > info_.maximum_cluster)
    {
        if (error != nullptr)
        {
            *error = "FAT32 FAT index is outside the data region";
        }
        return false;
    }
    const std::uint64_t byte_offset = static_cast<std::uint64_t>(cluster) * 4;
    // The FAT entry is read directly rather than through a whole sector: the
    // image layer already serves the containing hunk from its cache, so the
    // intermediate sector buffer would only add an allocation per chain hop.
    const std::uint64_t address =
        (info_.partition_lba + info_.reserved_sectors) * 512 + byte_offset;
    std::array<std::uint8_t, 4> entry = {};
    if (!image_->Read(address, entry.data(), entry.size(), error))
    {
        return false;
    }
    *value = ReadU32(entry.data(), 0) & 0x0fffffff;
    return true;
}

bool Fat32Volume::ReadDirectoryClusterChain(std::uint32_t first_cluster,
                                            std::vector<Fat32Entry>* entries,
                                            std::string* error) const
{
    if (entries == nullptr || first_cluster < 2 || first_cluster > info_.maximum_cluster)
    {
        if (error != nullptr)
        {
            *error = "FAT32 directory cluster is invalid";
        }
        return false;
    }
    entries->clear();
    FatLongNameAssembler pending;
    std::uint32_t cluster = first_cluster;
    for (std::uint32_t count = 0; count <= info_.cluster_count; ++count)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadCluster(cluster, &bytes, error))
        {
            return false;
        }
        for (std::size_t offset = 0; offset + 32 <= bytes.size(); offset += 32)
        {
            const std::uint8_t* entry = bytes.data() + offset;
            if (entry[0] == 0x00)
            {
                return true;
            }
            if (entry[0] == 0xe5)
            {
                pending.Clear();
                continue;
            }
            if (entry[11] == kFatLongNameAttribute)
            {
                pending.Add(entry);
                continue;
            }
            const std::uint8_t attributes = entry[11];
            if ((attributes & 0x08) != 0)
            {
                pending.Clear();
                continue;
            }
            Fat32Entry result;
            result.name = DecodeFatShortName(entry);
            std::string long_name;
            if (pending.Decode(entry, &long_name))
            {
                result.name = std::move(long_name);
            }
            pending.Clear();
            result.directory = (attributes & 0x10) != 0;
            result.attributes = attributes;
            result.first_cluster = (static_cast<std::uint32_t>(ReadU16(entry, 20)) << 16) |
                                   ReadU16(entry, 26);
            result.size = ReadU32(entry, 28);
            result.creation_time = ReadU16(entry, 14);
            result.creation_date = ReadU16(entry, 16);
            result.last_access_date = ReadU16(entry, 18);
            result.write_time = ReadU16(entry, 22);
            result.write_date = ReadU16(entry, 24);
            if (result.name == "." || result.name == ".." || result.name.empty())
            {
                continue;
            }
            if (result.directory && (result.first_cluster < 2 ||
                                     result.first_cluster > info_.maximum_cluster))
            {
                if (error != nullptr)
                {
                    *error = "FAT32 directory entry has an invalid cluster";
                }
                return false;
            }
            entries->push_back(std::move(result));
        }

        std::uint32_t next = 0;
        if (!ReadFatEntry(cluster, &next, error))
        {
            return false;
        }
        if (next >= kFat32EndOfChain)
        {
            return true;
        }
        if (next == kFat32BadCluster || next >= kFat32ReservedStart || next < 2 ||
            next > info_.maximum_cluster)
        {
            if (error != nullptr)
            {
                *error = "FAT32 directory chain contains an invalid next cluster";
            }
            return false;
        }
        cluster = next;
    }
    if (error != nullptr)
    {
        *error = "FAT32 directory chain exceeded the cluster limit";
    }
    return false;
}

void Fat32Volume::StoreLookupLocked(const std::string& key,
                                    std::optional<Fat32Entry> entry) const
{
    if (path_cache_.size() >= kPathCacheEntries)
    {
        path_cache_.clear();
    }
    path_cache_[key] = std::move(entry);
}

bool Fat32Volume::DirectoryEntriesLocked(std::uint32_t first_cluster,
                                         const std::vector<Fat32Entry>** entries,
                                         std::string* error) const
{
    const auto cached = directory_cache_.find(first_cluster);
    if (cached != directory_cache_.end())
    {
        *entries = &cached->second;
        return true;
    }
    std::vector<Fat32Entry> parsed;
    if (!ReadDirectoryClusterChain(first_cluster, &parsed, error))
    {
        return false;
    }
    if (directory_cache_.size() >= kDirectoryCacheEntries)
    {
        directory_cache_.clear();
    }
    *entries = &directory_cache_.emplace(first_cluster, std::move(parsed)).first->second;
    return true;
}

bool Fat32Volume::ReadDirectory(std::string_view relative_path,
                                std::vector<Fat32Entry>* entries,
                                std::string* error) const
{
    if (entries == nullptr)
    {
        if (error != nullptr)
        {
            *error = "FAT32 directory output is null";
        }
        return false;
    }
    const std::lock_guard<std::mutex> guard(lock_);
    Fat32Entry directory;
    if (!FindLocked(relative_path, &directory, error))
    {
        return false;
    }
    if (!directory.directory)
    {
        if (error != nullptr)
        {
            *error = "FAT32 path is not a directory";
        }
        return false;
    }
    const std::vector<Fat32Entry>* cached = nullptr;
    if (!DirectoryEntriesLocked(directory.first_cluster, &cached, error))
    {
        return false;
    }
    *entries = *cached;
    return true;
}

bool Fat32Volume::Find(std::string_view relative_path,
                       Fat32Entry* out,
                       std::string* error) const
{
    const std::lock_guard<std::mutex> guard(lock_);
    return FindLocked(relative_path, out, error);
}

bool Fat32Volume::FindLocked(std::string_view relative_path,
                             Fat32Entry* out,
                             std::string* error) const
{
    if (out == nullptr || !IsSafeRelativePath(relative_path))
    {
        if (error != nullptr)
        {
            *error = "FAT32 path is not a safe relative path";
        }
        return false;
    }
    // FAT name matching is case-insensitive, so the cache key folds case and
    // normalizes the separator. Two spellings of one path share an entry.
    const std::string key = MakeLookupKey(relative_path);
    const auto cached = path_cache_.find(key);
    if (cached != path_cache_.end())
    {
        if (!cached->second.has_value())
        {
            if (error != nullptr)
            {
                *error = "FAT32 path was not found: " + std::string(relative_path);
            }
            return false;
        }
        *out = *cached->second;
        return true;
    }
    Fat32Entry current;
    current.name = "/";
    current.directory = true;
    current.first_cluster = info_.root_cluster;
    const std::vector<std::string> components = SplitPath(relative_path);
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        const std::string& component = components[index];
        const std::vector<Fat32Entry>* entries = nullptr;
        if (!DirectoryEntriesLocked(current.first_cluster, &entries, error))
        {
            return false;
        }
        const auto found = std::find_if(
            entries->begin(), entries->end(), [&component](const Fat32Entry& entry)
            { return EqualsIgnoreAsciiCase(entry.name, component); });
        if (found == entries->end())
        {
            // A guest that probes for optional files repeats the same failing
            // lookup, so the negative result is remembered too.
            StoreLookupLocked(key, std::nullopt);
            if (error != nullptr)
            {
                *error = "FAT32 path was not found: " + std::string(relative_path);
            }
            return false;
        }
        current = *found;
        if (!current.directory && index + 1 != components.size())
        {
            if (error != nullptr)
            {
                *error = "FAT32 path traverses through a file";
            }
            return false;
        }
    }
    StoreLookupLocked(key, current);
    *out = std::move(current);
    return true;
}

bool Fat32Volume::ClusterAtIndexLocked(std::uint32_t first_cluster,
                                       std::uint64_t index,
                                       std::uint32_t* cluster,
                                       std::string* error) const
{
    if (cluster == nullptr || first_cluster < 2 || first_cluster > info_.maximum_cluster)
    {
        if (error != nullptr)
        {
            *error = "FAT32 file has an invalid first cluster";
        }
        return false;
    }
    auto cached = chain_cache_.find(first_cluster);
    if (cached == chain_cache_.end())
    {
        if (chain_cache_.size() >= kChainCacheEntries)
        {
            chain_cache_.clear();
        }
        cached = chain_cache_.emplace(first_cluster, std::vector<std::uint32_t>{first_cluster})
                     .first;
    }
    std::vector<std::uint32_t>& chain = cached->second;
    // The chain is extended only as far as the requested index, so opening a
    // large file and reading its first bytes never walks the whole chain.
    while (chain.size() <= index)
    {
        if (chain.size() > info_.cluster_count)
        {
            if (error != nullptr)
            {
                *error = "FAT32 file chain exceeded the cluster limit";
            }
            return false;
        }
        std::uint32_t next = 0;
        if (!ReadFatEntry(chain.back(), &next, error))
        {
            return false;
        }
        if (next < 2 || next > info_.maximum_cluster || next >= kFat32EndOfChain)
        {
            if (error != nullptr)
            {
                *error = "FAT32 file chain ended before the requested offset";
            }
            return false;
        }
        chain.push_back(next);
    }
    *cluster = chain[static_cast<std::size_t>(index)];
    return true;
}

bool Fat32Volume::ReadFileRange(std::string_view relative_path,
                                std::uint64_t offset,
                                void* destination,
                                std::size_t length,
                                std::string* error) const
{
    const std::lock_guard<std::mutex> guard(lock_);
    return ReadFileRangeLocked(relative_path, offset, destination, length, error);
}

bool Fat32Volume::ReadFileRangeLocked(std::string_view relative_path,
                                      std::uint64_t offset,
                                      void* destination,
                                      std::size_t length,
                                      std::string* error) const
{
    Fat32Entry file;
    if (!FindLocked(relative_path, &file, error))
    {
        return false;
    }
    if (file.directory || offset > file.size || length > file.size - offset)
    {
        if (error != nullptr)
        {
            *error = "FAT32 file read range is outside the file";
        }
        return false;
    }
    if (length == 0)
    {
        return true;
    }
    if (destination == nullptr)
    {
        if (error != nullptr)
        {
            *error = "FAT32 file read destination is null";
        }
        return false;
    }
    const std::uint64_t cluster_bytes =
        static_cast<std::uint64_t>(info_.sectors_per_cluster) * info_.bytes_per_sector;
    if (cluster_bytes == 0)
    {
        if (error != nullptr)
        {
            *error = "FAT32 cluster size is zero";
        }
        return false;
    }
    if (file.first_cluster < 2 || file.first_cluster > info_.maximum_cluster)
    {
        if (error != nullptr)
        {
            *error = "FAT32 file has an invalid first cluster";
        }
        return false;
    }

    auto* output = static_cast<std::uint8_t*>(destination);
    std::size_t remaining = length;
    std::uint64_t index = offset / cluster_bytes;
    std::size_t within = static_cast<std::size_t>(offset % cluster_bytes);
    while (remaining != 0)
    {
        std::uint32_t cluster = 0;
        if (!ClusterAtIndexLocked(file.first_cluster, index, &cluster, error))
        {
            return false;
        }
        // The cluster payload is read straight into the caller's buffer. The
        // image layer serves it from its hunk cache, so an intermediate
        // cluster buffer would only add a copy and an allocation.
        const std::uint64_t address =
            (info_.data_lba + static_cast<std::uint64_t>(cluster - 2) * info_.sectors_per_cluster) *
                512 +
            within;
        const std::size_t count =
            std::min(remaining, static_cast<std::size_t>(cluster_bytes) - within);
        if (!image_->Read(address, output, count, error))
        {
            return false;
        }
        output += count;
        remaining -= count;
        within = 0;
        ++index;
    }
    return true;
}

bool Fat32Volume::ReadFile(std::string_view relative_path,
                           std::vector<std::uint8_t>* bytes,
                           std::string* error) const
{
    const std::lock_guard<std::mutex> guard(lock_);
    Fat32Entry file;
    if (bytes == nullptr || !FindLocked(relative_path, &file, error))
    {
        return false;
    }
    if (file.directory)
    {
        if (error != nullptr)
        {
            *error = "FAT32 path is a directory";
        }
        return false;
    }
    // A FAT32 size is 32 bits, so it always fits the host's size_t; a runtime
    // comparison is always false, which clang rejects as a warning.
    static_assert(sizeof(file.size) <= sizeof(std::size_t), "a FAT32 file size must fit size_t");
    bytes->assign(file.size, 0);
    return ReadFileRangeLocked(relative_path, 0, bytes->data(), bytes->size(), error);
}

bool Fat32Volume::MaterializeFile(std::string_view relative_path,
                                  const std::filesystem::path& output,
                                  std::string* error) const
{
    const std::lock_guard<std::mutex> guard(lock_);
    Fat32Entry file;
    if (!FindLocked(relative_path, &file, error))
    {
        return false;
    }
    if (file.directory || output.empty())
    {
        if (error != nullptr)
        {
            *error = "cannot materialize a FAT32 directory or empty output path";
        }
        return false;
    }
    std::error_code code;
    if (!output.parent_path().empty())
    {
        std::filesystem::create_directories(output.parent_path(), code);
        if (code)
        {
            if (error != nullptr)
            {
                *error = "cannot create CHD staging directory: " + code.message();
            }
            return false;
        }
    }
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        if (error != nullptr)
        {
            *error = "cannot create CHD staging file: " + output.string();
        }
        return false;
    }
    constexpr std::size_t kChunkBytes = 1024 * 1024;
    const std::size_t chunk_size = static_cast<std::size_t>(
        std::min<std::uint64_t>(kChunkBytes, file.size));
    std::vector<std::uint8_t> buffer(chunk_size);
    std::uint64_t offset = 0;
    while (offset < file.size)
    {
        const std::size_t count = static_cast<std::size_t>(
            std::min<std::uint64_t>(buffer.size(), file.size - offset));
        if (!ReadFileRangeLocked(relative_path, offset, buffer.data(), count, error))
        {
            return false;
        }
        stream.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(count));
        if (!stream)
        {
            if (error != nullptr)
            {
                *error = "cannot write CHD staging file: " + output.string();
            }
            return false;
        }
        offset += count;
    }
    return true;
}

}  // namespace re2dj::storage
