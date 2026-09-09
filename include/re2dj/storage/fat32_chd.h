#ifndef RE2DJ_STORAGE_FAT32_CHD_H_
#define RE2DJ_STORAGE_FAT32_CHD_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "re2dj/storage/mame_chd.h"

namespace re2dj::storage
{

struct Fat32VolumeInfo
{
    // False when the image carries no partition table and LBA 0 is itself the
    // FAT32 boot sector. partition_index is then meaningless, so it cannot be
    // used to tell that case from the first partition.
    bool partitioned = true;
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
    // DOS-format date and time words exactly as the directory entry stores
    // them. They stay unconverted here because this is platform-neutral core:
    // turning them into a host file time is the host layer's job. Last access
    // has a date but no time, and zero means the entry carries no such stamp.
    std::uint16_t creation_time = 0;
    std::uint16_t creation_date = 0;
    std::uint16_t last_access_date = 0;
    std::uint16_t write_time = 0;
    std::uint16_t write_date = 0;
};

// Reads a candidate FAT32 boot sector and accepts it only when every declared
// value is self-consistent and fits inside the span it is given. This is what
// decides whether a sector is a FAT32 boot sector at all, so a caller can offer
// sector 0 of an unpartitioned image and a partition's first sector to the same
// test. `boot` must be at least 512 bytes.
bool ParseFat32BootSector(const std::uint8_t* boot,
                          std::size_t boot_size,
                          std::uint64_t volume_lba,
                          std::uint64_t volume_sectors,
                          Fat32VolumeInfo* info);

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

    // Guest file APIs can arrive on several threads, and the caches below are
    // shared mutable state, so the public read API is serialized. Every helper
    // named *Locked requires `lock_` to be held.
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

    bool FindLocked(std::string_view relative_path, Fat32Entry* out, std::string* error) const;
    void StoreLookupLocked(const std::string& key, std::optional<Fat32Entry> entry) const;
    bool ReadFileRangeLocked(std::string_view relative_path,
                             std::uint64_t offset,
                             void* destination,
                             std::size_t length,
                             std::string* error) const;
    // Returns the cached entry list for one directory, reading and parsing the
    // directory cluster chain only on the first request.
    bool DirectoryEntriesLocked(std::uint32_t first_cluster,
                                const std::vector<Fat32Entry>** entries,
                                std::string* error) const;
    // Resolves the cluster holding `index` within a chain, extending the cached
    // chain only as far as that index.
    bool ClusterAtIndexLocked(std::uint32_t first_cluster,
                              std::uint64_t index,
                              std::uint32_t* cluster,
                              std::string* error) const;

    std::unique_ptr<MameChdImage> image_;
    Fat32VolumeInfo info_;

    // The volume is opened read-only and the source image is never modified,
    // so a cached lookup stays valid for the life of the volume and none of
    // these caches needs an invalidation path. Each is capped so that a guest
    // probing many distinct paths cannot grow them without bound.
    mutable std::mutex lock_;
    mutable std::unordered_map<std::uint32_t, std::vector<Fat32Entry>> directory_cache_;
    mutable std::unordered_map<std::string, std::optional<Fat32Entry>> path_cache_;
    mutable std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> chain_cache_;
};

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_FAT32_CHD_H_
