#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/storage/mame_chd.h"

namespace
{

std::string Tag(std::uint32_t tag)
{
    std::string result(4, '\0');
    result[0] = static_cast<char>((tag >> 24) & 0xff);
    result[1] = static_cast<char>((tag >> 16) & 0xff);
    result[2] = static_cast<char>((tag >> 8) & 0xff);
    result[3] = static_cast<char>(tag & 0xff);
    return result;
}

void PrintPrefix(const std::vector<std::uint8_t>& bytes)
{
    const std::size_t count = std::min<std::size_t>(bytes.size(), 16);
    for (std::size_t index = 0; index < count; ++index)
    {
        std::printf("%02x%s", bytes[index], index + 1 == count ? "" : " ");
    }
}

}  // namespace

int main(int argc, char** argv)
{
    // The optional listing answers where a guest resource actually lives when a
    // relative open fails: the runtime resolves such names against one
    // directory, and only the image says whether the file is there.
    const bool list_requested = argc == 4 && std::string(argv[2]) == "--list";
    const bool dump_requested = (argc == 4 || argc == 5) && std::string(argv[2]) == "--dump";
    if (argc != 2 && !list_requested && !dump_requested)
    {
        std::fprintf(stderr,
                     "usage: re2dj_chd_probe <mame-chd-path> [--list <relative-directory> | --dump <relative-file> [output-path]]\n");
        return 1;
    }

    if (dump_requested)
    {
        std::unique_ptr<re2dj::storage::Fat32Volume> volume;
        std::string filesystem_error;
        if (!re2dj::storage::Fat32Volume::Open(argv[1], &volume, &filesystem_error))
        {
            std::fprintf(stderr, "error: cannot open FAT32 volume: %s\n", filesystem_error.c_str());
            return 2;
        }
        std::vector<std::uint8_t> file_bytes;
        if (!volume->ReadFile(argv[3], &file_bytes, &filesystem_error))
        {
            std::fprintf(stderr, "error: cannot read file %s: %s\n", argv[3], filesystem_error.c_str());
            return 4;
        }
        if (argc == 5)
        {
            std::FILE* out = std::fopen(argv[4], "wb");
            if (out == nullptr)
            {
                std::fprintf(stderr, "error: cannot open output file %s\n", argv[4]);
                return 5;
            }
            std::fwrite(file_bytes.data(), 1, file_bytes.size(), out);
            std::fclose(out);
            std::printf("dumped %zu bytes to %s\n", file_bytes.size(), argv[4]);
        }
        else
        {
            std::fwrite(file_bytes.data(), 1, file_bytes.size(), stdout);
        }
        return 0;
    }

    std::unique_ptr<re2dj::storage::MameChdImage> image;
    std::string error;
    if (!re2dj::storage::MameChdImage::Open(argv[1], &image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 2;
    }

    const re2dj::storage::MameChdInfo& info = image->info();
    std::printf("version=%u logical_bytes=%llu hunk_bytes=%u unit_bytes=%u hunk_count=%llu\n",
                info.version,
                static_cast<unsigned long long>(info.logical_bytes),
                info.hunk_bytes,
                info.unit_bytes,
                static_cast<unsigned long long>(info.hunk_count));
    std::printf("codecs=");
    for (std::size_t index = 0; index < info.codecs.size(); ++index)
    {
        std::printf("%s%s",
                    re2dj::storage::MameChdCodecName(info.codecs[index]).c_str(),
                    index + 1 == info.codecs.size() ? "" : ",");
    }
    std::printf("\nmetadata_count=%zu\n", info.metadata.size());
    for (const re2dj::storage::MameChdMetadata& metadata : info.metadata)
    {
        std::printf("metadata tag=%s flags=%u length=%zu",
                    Tag(metadata.tag).c_str(),
                    metadata.flags,
                    metadata.payload.size());
        const std::string payload(metadata.payload.begin(), metadata.payload.end());
        if (Tag(metadata.tag) == "GDDD")
        {
            std::printf(" value=%s", payload.c_str());
        }
        std::printf("\n");
    }

    std::vector<std::uint8_t> sector;
    if (!image->ReadSector(0, &sector, &error))
    {
        std::fprintf(stderr, "error: cannot read LBA 0: %s\n", error.c_str());
        return 3;
    }
    std::printf("lba0_prefix=");
    PrintPrefix(sector);
    std::printf(" signature=%s\n",
                sector.size() >= 512 && sector[510] == 0x55 && sector[511] == 0xaa
                    ? "55aa"
                    : "not-55aa");

    if (info.hunk_bytes < 16)
    {
        std::fprintf(stderr, "error: CHD hunk is too small for the boundary probe\n");
        return 3;
    }
    std::vector<std::uint8_t> boundary_probe(16, 0);
    const std::uint64_t boundary_offset = info.hunk_bytes - 8;
    if (!image->Read(boundary_offset,
                     boundary_probe.data(),
                     boundary_probe.size(),
                     &error))
    {
        std::fprintf(stderr, "error: cannot read across CHD hunk boundary: %s\n", error.c_str());
        return 3;
    }
    std::printf("cross_hunk_read=ok offset=%llu length=%zu\n",
                static_cast<unsigned long long>(boundary_offset),
                boundary_probe.size());

    // A CHD may contain a CD or another non-FAT volume, so filesystem
    // detection is reported separately from the generic CHD checks above.
    std::unique_ptr<re2dj::storage::Fat32Volume> volume;
    std::string filesystem_error;
    if (!re2dj::storage::Fat32Volume::Open(argv[1], &volume, &filesystem_error))
    {
        std::printf("filesystem=unrecognized reason=%s\n", filesystem_error.c_str());
        return 0;
    }
    const re2dj::storage::Fat32VolumeInfo& filesystem = volume->info();
    std::printf(
        "filesystem=fat32 partition=%u partition_lba=%llu partition_sectors=%llu "
        "bytes_per_sector=%u sectors_per_cluster=%u reserved_sectors=%u fat_count=%u "
        "sectors_per_fat=%u root_cluster=%u data_lba=%llu cluster_count=%u label=%s type=%s\n",
        filesystem.partition_index,
        static_cast<unsigned long long>(filesystem.partition_lba),
        static_cast<unsigned long long>(filesystem.partition_sectors),
        filesystem.bytes_per_sector,
        filesystem.sectors_per_cluster,
        filesystem.reserved_sectors,
        filesystem.fat_count,
        filesystem.sectors_per_fat,
        filesystem.root_cluster,
        static_cast<unsigned long long>(filesystem.data_lba),
        filesystem.cluster_count,
        filesystem.volume_label.c_str(),
        filesystem.filesystem_type.c_str());

    if (list_requested)
    {
        std::vector<re2dj::storage::Fat32Entry> entries;
        if (!volume->ReadDirectory(argv[3], &entries, &filesystem_error))
        {
            std::fprintf(stderr,
                         "error: cannot read directory %s: %s\n",
                         argv[3],
                         filesystem_error.c_str());
            return 4;
        }
        std::printf("listing=%s entries=%zu\n", argv[3], entries.size());
        for (const re2dj::storage::Fat32Entry& entry : entries)
        {
            std::printf("entry name=%s directory=%s size=%u first_cluster=%u\n",
                        entry.name.c_str(),
                        entry.directory ? "true" : "false",
                        entry.size,
                        entry.first_cluster);
        }
        return 0;
    }


    re2dj::storage::Fat32Entry executable;
    if (!volume->Find("EZ2DJ/EZ2DJ.EXE", &executable, &filesystem_error) ||
        executable.directory)
    {
        std::fprintf(stderr,
                     "error: FAT32 filesystem has no EZ2DJ/EZ2DJ.EXE: %s\n",
                     filesystem_error.c_str());
        return 4;
    }
    std::vector<std::uint8_t> executable_bytes;
    if (!volume->ReadFile("EZ2DJ/EZ2DJ.EXE", &executable_bytes, &filesystem_error))
    {
        std::fprintf(stderr, "error: cannot read EZ2DJ.EXE from FAT32: %s\n",
                     filesystem_error.c_str());
        return 4;
    }
    re2dj::exe::PeImageInfo executable_info;
    if (!re2dj::exe::ReadPeImageInfo(executable_bytes.data(),
                                     executable_bytes.size(),
                                     &executable_info,
                                     &filesystem_error))
    {
        std::fprintf(stderr, "error: EZ2DJ.EXE is not a readable PE image: %s\n",
                     filesystem_error.c_str());
        return 4;
    }
    std::printf("filesystem_executable=EZ2DJ/EZ2DJ.EXE first_cluster=%u size=%u\n",
                executable.first_cluster,
                executable.size);
    std::vector<re2dj::storage::Fat32Entry> windows_entries;
    if (volume->ReadDirectory("WINDOWS", &windows_entries, &filesystem_error))
    {
        std::printf("filesystem_windows_entries=%zu%s\n",
                    windows_entries.size(),
                    windows_entries.empty() ? "" :
                        (std::string(" first=") + windows_entries.front().name).c_str());
    }
    std::printf("filesystem_pe=machine=%s magic=%s subsystem=%s image_base=0x%08llx "
                "entry_rva=0x%08x sections=%zu\n",
                std::string(re2dj::exe::MachineName(executable_info.machine)).c_str(),
                std::string(re2dj::exe::MagicName(executable_info.magic)).c_str(),
                std::string(re2dj::exe::SubsystemName(executable_info.subsystem)).c_str(),
                static_cast<unsigned long long>(executable_info.image_base),
                executable_info.entry_point_rva,
                executable_info.sections.size());
    return 0;
}
