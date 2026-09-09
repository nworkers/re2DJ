#include "re2dj/storage/fat32_chd.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "test_support.h"

namespace
{

void PutU16(std::array<std::uint8_t, 512>& sector, std::size_t offset, std::uint16_t value)
{
    sector[offset] = static_cast<std::uint8_t>(value & 0xff);
    sector[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void PutU32(std::array<std::uint8_t, 512>& sector, std::size_t offset, std::uint32_t value)
{
    sector[offset] = static_cast<std::uint8_t>(value & 0xff);
    sector[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    sector[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
    sector[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
}

// A FAT32 boot sector shaped like the ones the real images carry: 512-byte
// sectors, 32 sectors per cluster, two FATs and the root at cluster 2.
std::array<std::uint8_t, 512> MakeBootSector(std::uint32_t total_sectors)
{
    std::array<std::uint8_t, 512> sector = {};
    sector[0] = 0xeb;
    sector[1] = 0x58;
    sector[2] = 0x90;
    PutU16(sector, 11, 512);   // bytes per sector
    sector[13] = 32;           // sectors per cluster
    PutU16(sector, 14, 36);    // reserved sectors
    sector[16] = 2;            // FAT count
    PutU16(sector, 19, 0);     // 16-bit total sectors, unused on FAT32
    PutU32(sector, 32, total_sectors);
    PutU32(sector, 36, 512);   // sectors per FAT
    PutU32(sector, 44, 2);     // root cluster
    sector[510] = 0x55;
    sector[511] = 0xaa;
    return sector;
}

// A partition table whose first entry names a FAT32 volume. It has no BPB, so
// offering it as a boot sector must be refused.
std::array<std::uint8_t, 512> MakeMasterBootRecord(std::uint32_t partition_lba,
                                                   std::uint32_t partition_sectors)
{
    std::array<std::uint8_t, 512> sector = {};
    sector[0] = 0xfa;
    sector[446 + 4] = 0x0c;  // FAT32 LBA
    PutU32(sector, 446 + 8, partition_lba);
    PutU32(sector, 446 + 12, partition_sectors);
    sector[510] = 0x55;
    sector[511] = 0xaa;
    return sector;
}

}  // namespace

void RunFat32ChdTests(re2dj::test::Context& context)
{
    std::unique_ptr<re2dj::storage::Fat32Volume> volume;
    std::string error;
    RE2DJ_CHECK(context,
                !re2dj::storage::Fat32Volume::Open({}, &volume, &error));
    RE2DJ_CHECK(context,
                !re2dj::storage::Fat32Volume::Open(
                    std::filesystem::path("does-not-exist.chd"), &volume, &error));

    // ---- A boot sector at the start of a partition ----
    {
        constexpr std::uint32_t kPartitionLba = 63;
        constexpr std::uint32_t kPartitionSectors = 200000;
        const std::array<std::uint8_t, 512> boot = MakeBootSector(kPartitionSectors);
        re2dj::storage::Fat32VolumeInfo info;
        RE2DJ_CHECK(context,
                    re2dj::storage::ParseFat32BootSector(
                        boot.data(), boot.size(), kPartitionLba, kPartitionSectors, &info));
        RE2DJ_CHECK_EQ(context, info.partition_lba, std::uint64_t{kPartitionLba});
        RE2DJ_CHECK_EQ(context, info.bytes_per_sector, std::uint32_t{512});
        RE2DJ_CHECK_EQ(context, info.sectors_per_cluster, std::uint32_t{32});
        RE2DJ_CHECK_EQ(context, info.fat_count, std::uint32_t{2});
        RE2DJ_CHECK_EQ(context, info.root_cluster, std::uint32_t{2});
        // Data starts after the reserved sectors and both FATs.
        RE2DJ_CHECK_EQ(context, info.data_lba,
                       std::uint64_t{kPartitionLba} + 36 + 2 * 512);
    }

    // ---- The same boot sector as a whole-disk volume at LBA 0 ----
    {
        constexpr std::uint32_t kImageSectors = 200000;
        const std::array<std::uint8_t, 512> boot = MakeBootSector(kImageSectors);
        re2dj::storage::Fat32VolumeInfo info;
        RE2DJ_CHECK(context,
                    re2dj::storage::ParseFat32BootSector(
                        boot.data(), boot.size(), 0, kImageSectors, &info));
        RE2DJ_CHECK_EQ(context, info.partition_lba, std::uint64_t{0});
        RE2DJ_CHECK_EQ(context, info.data_lba, std::uint64_t{36 + 2 * 512});
    }

    // ---- A partition table is not a boot sector ----
    {
        const std::array<std::uint8_t, 512> mbr = MakeMasterBootRecord(63, 200000);
        re2dj::storage::Fat32VolumeInfo info;
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(
                        mbr.data(), mbr.size(), 0, 200063, &info));
    }

    // ---- Rejections ----
    {
        re2dj::storage::Fat32VolumeInfo info;
        std::array<std::uint8_t, 512> boot = MakeBootSector(200000);

        // A volume that cannot hold what the BPB declares.
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(
                        boot.data(), boot.size(), 0, 1000, &info));

        // No boot signature.
        boot[511] = 0;
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(
                        boot.data(), boot.size(), 0, 200000, &info));
        boot[511] = 0xaa;

        // A cluster size that is not a power of two.
        boot[13] = 24;
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(
                        boot.data(), boot.size(), 0, 200000, &info));
        boot[13] = 32;

        // A root cluster below the first data cluster.
        PutU32(boot, 44, 1);
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(
                        boot.data(), boot.size(), 0, 200000, &info));
        PutU32(boot, 44, 2);

        // Short input.
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(boot.data(), 511, 0, 200000, &info));
        RE2DJ_CHECK(context,
                    !re2dj::storage::ParseFat32BootSector(nullptr, 512, 0, 200000, &info));
    }
}
