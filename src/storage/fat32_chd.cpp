#include "re2dj/storage/fat32_chd.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

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

std::string Utf16ToUtf8(const std::vector<std::uint16_t>& code_units)
{
    std::string result;
    for (std::size_t index = 0; index < code_units.size(); ++index)
    {
        std::uint32_t code_point = code_units[index];
        if (code_point == 0x0000 || code_point == 0xffff)
        {
            break;
        }
        if (code_point >= 0xd800 && code_point <= 0xdbff && index + 1 < code_units.size())
        {
            const std::uint32_t low = code_units[index + 1];
            if (low >= 0xdc00 && low <= 0xdfff)
            {
                code_point = 0x10000 + ((code_point - 0xd800) << 10) + (low - 0xdc00);
                ++index;
            }
        }
        if (code_point < 0x80)
        {
            result.push_back(static_cast<char>(code_point));
        }
        else if (code_point < 0x800)
        {
            result.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else if (code_point < 0x10000)
        {
            result.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
        else
        {
            result.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
    }
    return result;
}

std::uint8_t ShortNameChecksum(const std::uint8_t* short_name)
{
    std::uint8_t checksum = 0;
    for (std::size_t index = 0; index < 11; ++index)
    {
        checksum = static_cast<std::uint8_t>(((checksum & 1) << 7) +
                                              (checksum >> 1) + short_name[index]);
    }
    return checksum;
}

std::string DecodeShortName(const std::uint8_t* entry)
{
    std::string base(reinterpret_cast<const char*>(entry), 8);
    std::string extension(reinterpret_cast<const char*>(entry + 8), 3);
    while (!base.empty() && base.back() == ' ')
    {
        base.pop_back();
    }
    while (!extension.empty() && extension.back() == ' ')
    {
        extension.pop_back();
    }
    if (!extension.empty())
    {
        base.push_back('.');
        base.append(extension);
    }
    return base;
}

void AppendLfnPart(const std::uint8_t* entry, std::vector<std::uint16_t>* part)
{
    constexpr std::array<std::pair<std::size_t, std::size_t>, 3> ranges = {
        {{1, 5}, {14, 6}, {28, 4}}};
    for (const auto [offset, count] : ranges)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            part->push_back(ReadU16(entry, offset + index * 2));
        }
    }
}

struct PendingLfn
{
    std::vector<std::pair<std::uint8_t, std::vector<std::uint16_t>>> parts;
    std::uint8_t checksum = 0;
    bool has_last = false;

    void Clear()
    {
        parts.clear();
        checksum = 0;
        has_last = false;
    }

    void Add(const std::uint8_t* entry)
    {
        const std::uint8_t order = entry[0];
        const std::uint8_t sequence = order & 0x1f;
        if (sequence == 0 || (order & 0x80) != 0 || (parts.empty() && (order & 0x40) == 0))
        {
            Clear();
        }
        if (parts.empty())
        {
            checksum = entry[13];
            has_last = (order & 0x40) != 0;
        }
        else if (entry[13] != checksum)
        {
            Clear();
            return;
        }
        std::vector<std::uint16_t> part;
        AppendLfnPart(entry, &part);
        parts.emplace_back(sequence, std::move(part));
    }

    bool Decode(const std::uint8_t* short_entry, std::string* name) const
    {
        if (!has_last || parts.empty() || ShortNameChecksum(short_entry) != checksum)
        {
            return false;
        }
        std::vector<std::uint16_t> units;
        for (std::uint8_t sequence = 1; sequence <= parts.size(); ++sequence)
        {
            const auto found = std::find_if(
                parts.begin(), parts.end(), [sequence](const auto& part)
                { return part.first == sequence; });
            if (found == parts.end())
            {
                return false;
            }
            units.insert(units.end(), found->second.begin(), found->second.end());
        }
        *name = Utf16ToUtf8(units);
        return !name->empty();
    }
};

}  // namespace

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

    std::vector<std::uint8_t> mbr;
    if (!image->ReadSector(0, &mbr, error) || mbr.size() < 512 || mbr[510] != 0x55 ||
        mbr[511] != 0xaa)
    {
        return fail("CHD does not contain a valid MBR signature");
    }

    std::uint32_t partition_index = 0;
    std::uint64_t partition_lba = 0;
    std::uint64_t partition_sectors = 0;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const std::size_t offset = 446 + index * 16;
        if (IsFat32PartitionType(mbr[offset + 4]))
        {
            partition_index = index;
            partition_lba = ReadU32(mbr.data(), offset + 8);
            partition_sectors = ReadU32(mbr.data(), offset + 12);
            break;
        }
    }
    if (partition_sectors == 0 || partition_lba >= image->info().logical_bytes / 512 ||
        partition_sectors > image->info().logical_bytes / 512 - partition_lba)
    {
        return fail("CHD MBR has no in-range FAT32 partition");
    }

    std::vector<std::uint8_t> boot;
    if (!image->ReadSector(partition_lba, &boot, error) || boot.size() < 512 ||
        boot[510] != 0x55 || boot[511] != 0xaa)
    {
        return fail("FAT32 partition boot sector is invalid");
    }
    const std::uint32_t bytes_per_sector = ReadU16(boot.data(), 11);
    const std::uint32_t sectors_per_cluster = boot[13];
    const std::uint32_t reserved_sectors = ReadU16(boot.data(), 14);
    const std::uint32_t fat_count = boot[16];
    const std::uint32_t total_sectors16 = ReadU16(boot.data(), 19);
    const std::uint32_t total_sectors32 = ReadU32(boot.data(), 32);
    const std::uint32_t sectors_per_fat = ReadU32(boot.data(), 36);
    const std::uint32_t root_cluster = ReadU32(boot.data(), 44);
    const std::uint64_t total_sectors = total_sectors32 != 0 ? total_sectors32 : total_sectors16;
    if (bytes_per_sector != 512 || sectors_per_cluster == 0 ||
        (sectors_per_cluster & (sectors_per_cluster - 1)) != 0 ||
        sectors_per_cluster > 128 || reserved_sectors == 0 || fat_count == 0 ||
        sectors_per_fat == 0 || total_sectors == 0 || total_sectors > partition_sectors ||
        root_cluster < 2)
    {
        return fail("unsupported or invalid FAT32 BPB");
    }
    const std::uint64_t fat_sectors = static_cast<std::uint64_t>(fat_count) * sectors_per_fat;
    if (reserved_sectors + fat_sectors >= total_sectors)
    {
        return fail("FAT32 data region is empty");
    }
    const std::uint64_t data_sectors = total_sectors - reserved_sectors - fat_sectors;
    const std::uint64_t cluster_count = data_sectors / sectors_per_cluster;
    if (cluster_count == 0 || cluster_count > (std::numeric_limits<std::uint32_t>::max)() - 1)
    {
        return fail("FAT32 cluster count is outside the supported range");
    }
    const std::uint32_t maximum_cluster = static_cast<std::uint32_t>(cluster_count + 1);
    const std::uint64_t fat_entries =
        static_cast<std::uint64_t>(sectors_per_fat) * bytes_per_sector / 4;
    if (maximum_cluster >= fat_entries || root_cluster > maximum_cluster)
    {
        return fail("FAT32 FAT does not cover the declared data region");
    }

    Fat32VolumeInfo info;
    info.partition_index = partition_index;
    info.partition_lba = partition_lba;
    info.partition_sectors = partition_sectors;
    info.bytes_per_sector = bytes_per_sector;
    info.sectors_per_cluster = sectors_per_cluster;
    info.reserved_sectors = reserved_sectors;
    info.fat_count = fat_count;
    info.sectors_per_fat = sectors_per_fat;
    info.root_cluster = root_cluster;
    info.data_lba = partition_lba + reserved_sectors + fat_sectors;
    info.cluster_count = static_cast<std::uint32_t>(cluster_count);
    info.maximum_cluster = maximum_cluster;
    info.volume_label = TrimPadded(boot.data() + 71, 11);
    info.filesystem_type = TrimPadded(boot.data() + 82, 8);

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
    const std::uint64_t lba = info_.partition_lba + info_.reserved_sectors + byte_offset / 512;
    std::vector<std::uint8_t> sector;
    if (!ReadSector(lba, &sector, error))
    {
        return false;
    }
    *value = ReadU32(sector.data(), static_cast<std::size_t>(byte_offset % 512)) & 0x0fffffff;
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
    PendingLfn pending;
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
            if (entry[11] == 0x0f)
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
            result.name = DecodeShortName(entry);
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

bool Fat32Volume::ReadDirectory(std::string_view relative_path,
                                std::vector<Fat32Entry>* entries,
                                std::string* error) const
{
    Fat32Entry directory;
    if (!Find(relative_path, &directory, error))
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
    return ReadDirectoryClusterChain(directory.first_cluster, entries, error);
}

bool Fat32Volume::Find(std::string_view relative_path,
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
    Fat32Entry current;
    current.name = "/";
    current.directory = true;
    current.first_cluster = info_.root_cluster;
    const std::vector<std::string> components = SplitPath(relative_path);
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        const std::string& component = components[index];
        std::vector<Fat32Entry> entries;
        if (!ReadDirectoryClusterChain(current.first_cluster, &entries, error))
        {
            return false;
        }
        const auto found = std::find_if(
            entries.begin(), entries.end(), [&component](const Fat32Entry& entry)
            { return EqualsIgnoreAsciiCase(entry.name, component); });
        if (found == entries.end())
        {
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
    *out = std::move(current);
    return true;
}

bool Fat32Volume::ReadFileRange(std::string_view relative_path,
                                std::uint64_t offset,
                                void* destination,
                                std::size_t length,
                                std::string* error) const
{
    Fat32Entry file;
    if (!Find(relative_path, &file, error))
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
    std::uint32_t cluster = file.first_cluster;
    if (file.size != 0 && (cluster < 2 || cluster > info_.maximum_cluster))
    {
        if (error != nullptr)
        {
            *error = "FAT32 file has an invalid first cluster";
        }
        return false;
    }
    std::uint64_t skip = offset / cluster_bytes;
    for (std::uint32_t count = 0; count < skip; ++count)
    {
        std::uint32_t next = 0;
        if (!ReadFatEntry(cluster, &next, error) || next < 2 || next > info_.maximum_cluster ||
            next >= kFat32EndOfChain)
        {
            if (error != nullptr && error->empty())
            {
                *error = "FAT32 file chain ended before the requested offset";
            }
            return false;
        }
        cluster = next;
    }

    auto* output = static_cast<std::uint8_t*>(destination);
    std::size_t remaining = length;
    std::size_t within = static_cast<std::size_t>(offset % cluster_bytes);
    for (std::uint32_t count = 0; remaining != 0 && count <= info_.cluster_count; ++count)
    {
        std::vector<std::uint8_t> cluster_bytes_buffer;
        if (!ReadCluster(cluster, &cluster_bytes_buffer, error))
        {
            return false;
        }
        const std::size_t available = cluster_bytes_buffer.size() - within;
        const std::size_t copy_count = std::min(remaining, available);
        std::memcpy(output, cluster_bytes_buffer.data() + within, copy_count);
        output += copy_count;
        remaining -= copy_count;
        within = 0;
        if (remaining == 0)
        {
            return true;
        }
        std::uint32_t next = 0;
        if (!ReadFatEntry(cluster, &next, error) || next < 2 || next > info_.maximum_cluster ||
            next >= kFat32EndOfChain)
        {
            if (error != nullptr && error->empty())
            {
                *error = "FAT32 file chain ended before the requested range";
            }
            return false;
        }
        cluster = next;
    }
    if (error != nullptr)
    {
        *error = "FAT32 file chain exceeded the cluster limit";
    }
    return false;
}

bool Fat32Volume::ReadFile(std::string_view relative_path,
                           std::vector<std::uint8_t>* bytes,
                           std::string* error) const
{
    Fat32Entry file;
    if (bytes == nullptr || !Find(relative_path, &file, error))
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
    if (file.size > (std::numeric_limits<std::size_t>::max)())
    {
        if (error != nullptr)
        {
            *error = "FAT32 file is too large for the host address space";
        }
        return false;
    }
    bytes->assign(file.size, 0);
    return ReadFileRange(relative_path, 0, bytes->data(), bytes->size(), error);
}

bool Fat32Volume::MaterializeFile(std::string_view relative_path,
                                  const std::filesystem::path& output,
                                  std::string* error) const
{
    Fat32Entry file;
    if (!Find(relative_path, &file, error))
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
        if (!ReadFileRange(relative_path, offset, buffer.data(), count, error))
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
