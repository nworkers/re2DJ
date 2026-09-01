#ifndef RE2DJ_STORAGE_FAT32_CHD_H_
#define RE2DJ_STORAGE_FAT32_CHD_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/storage/mame_chd.h"

namespace re2dj::storage
{

struct Fat32VolumeInfo
{
    std::uint32_t partition_index = 0;
    std::uint64_t partition_lba = 0;
    std::uint64_t partition_sectors = 0;
    std::uint32_t bytes_per_sector = 0;
    std::uint32_t sectors_per_cluster = 0;
    std::uint32_t reserved_sectors = 0;
    std::uint32_t fat_count = 0;
    std::uint32_t sectors_per_fat = 0;
    std::uint32_t root_cluster = 0;
    std::uint64_t data_lba = 0;
    std::uint32_t cluster_count = 0;
    std::uint32_t maximum_cluster = 0;
    std::string volume_label;
    std::string filesystem_type;
};

struct Fat32Entry
{
    std::string name;
    bool directory = false;
    std::uint8_t attributes = 0;
    std::uint32_t first_cluster = 0;
    std::uint32_t size = 0;
};

// Read-only FAT32 filesystem view backed by a MAME CHD logical block device.
// The volume never writes to the source image.
class Fat32Volume
{
public:
    Fat32Volume() = delete;
    ~Fat32Volume() = default;

    Fat32Volume(const Fat32Volume&) = delete;
    Fat32Volume& operator=(const Fat32Volume&) = delete;

    static bool Open(const std::filesystem::path& chd_path,
                     std::unique_ptr<Fat32Volume>* out,
                     std::string* error);

    const Fat32VolumeInfo& info() const
    {
        return info_;
    }

    const MameChdImage& image() const
    {
        return *image_;
    }

    // Finds a relative path using case-insensitive FAT name matching.
    // Separators may be '/' or '\\'. Empty path names the root directory.
    bool Find(std::string_view relative_path, Fat32Entry* out, std::string* error) const;

    bool ReadDirectory(std::string_view relative_path,
                       std::vector<Fat32Entry>* entries,
                       std::string* error) const;

    bool ReadFileRange(std::string_view relative_path,
                       std::uint64_t offset,
                       void* destination,
                       std::size_t length,
                       std::string* error) const;

    bool ReadFile(std::string_view relative_path,
                  std::vector<std::uint8_t>* bytes,
                  std::string* error) const;

    // Materializes one file into a caller-owned temporary path. This is used
    // only for host APIs such as CreateProcessW that require a native path.
    bool MaterializeFile(std::string_view relative_path,
                         const std::filesystem::path& output,
                         std::string* error) const;

private:
    Fat32Volume(std::unique_ptr<MameChdImage> image, Fat32VolumeInfo info)
        : image_(std::move(image)), info_(std::move(info))
    {
    }

    bool ReadSector(std::uint64_t lba,
                    std::vector<std::uint8_t>* sector,
                    std::string* error) const;
    bool ReadCluster(std::uint32_t cluster,
                     std::vector<std::uint8_t>* bytes,
                     std::string* error) const;
    bool ReadFatEntry(std::uint32_t cluster,
                      std::uint32_t* value,
                      std::string* error) const;
    bool ReadDirectoryClusterChain(std::uint32_t first_cluster,
                                   std::vector<Fat32Entry>* entries,
                                   std::string* error) const;

    std::unique_ptr<MameChdImage> image_;
    Fat32VolumeInfo info_;
};

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_FAT32_CHD_H_
