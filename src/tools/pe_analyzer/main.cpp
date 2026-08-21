// Non-executing PE32 header dump.
//
// The loader will need every field printed here, so this tool doubles as the
// verification path for the header reader: run it against a known 32-bit
// Windows binary and compare the output with dumpbin or objdump.

#include <cstdio>
#include <filesystem>
#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/version.h"

namespace
{

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitReadError = 2;

void PrintUsage()
{
    std::printf(
        "re2dj_pe_analyzer %s - dump PE32 headers without loading the image\n"
        "\n"
        "Usage:\n"
        "  re2dj_pe_analyzer <file>\n"
        "  re2dj_pe_analyzer --hdd <directory> <guest-relative-path>\n",
        std::string(re2dj::VersionString()).c_str());
}

const char* DirectoryName(std::size_t index)
{
    switch (index)
    {
    case 0:
        return "export";
    case 1:
        return "import";
    case 2:
        return "resource";
    case 3:
        return "exception";
    case 4:
        return "security";
    case 5:
        return "basereloc";
    case 6:
        return "debug";
    case 7:
        return "architecture";
    case 8:
        return "globalptr";
    case 9:
        return "tls";
    case 10:
        return "load_config";
    case 11:
        return "bound_import";
    case 12:
        return "iat";
    case 13:
        return "delay_import";
    case 14:
        return "com_descriptor";
    default:
        return "reserved";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return kExitUsage;
    }

    std::filesystem::path file;
    if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        PrintUsage();
        return kExitOk;
    }

    if (std::string(argv[1]) == "--hdd")
    {
        if (argc < 4)
        {
            PrintUsage();
            return kExitUsage;
        }
        re2dj::hdd::HddRoot root;
        std::string open_error;
        if (!re2dj::hdd::HddRoot::Open(std::filesystem::path(argv[2]), &root, &open_error))
        {
            std::fprintf(stderr, "error: %s\n", open_error.c_str());
            return kExitReadError;
        }
        if (!root.ResolveFile(argv[3], &file))
        {
            std::fprintf(stderr,
                         "error: '%s' does not resolve to a file under %s\n",
                         argv[3],
                         root.root().string().c_str());
            return kExitReadError;
        }
    }
    else
    {
        file = std::filesystem::path(argv[1]);
    }

    re2dj::exe::PeImageInfo info;
    std::string error;
    if (!re2dj::exe::ReadPeImageInfo(file, &info, &error))
    {
        std::fprintf(stderr, "error: %s: %s\n", file.string().c_str(), error.c_str());
        return kExitReadError;
    }

    std::printf("file            : %s\n", file.string().c_str());
    std::printf("magic           : %s\n", std::string(re2dj::exe::MagicName(info.magic)).c_str());
    std::printf("machine         : 0x%04x (%s)\n",
                info.machine,
                std::string(re2dj::exe::MachineName(info.machine)).c_str());
    std::printf("characteristics : 0x%04x%s\n", info.characteristics, info.is_dll ? " dll" : "");
    std::printf("timestamp       : 0x%08x\n", info.timestamp);
    std::printf("image base      : 0x%08llx\n",
                static_cast<unsigned long long>(info.image_base));
    std::printf("entry point rva : 0x%08x\n", info.entry_point_rva);
    std::printf("size of image   : 0x%08x\n", info.size_of_image);
    std::printf("size of headers : 0x%08x\n", info.size_of_headers);
    std::printf("section align   : 0x%08x\n", info.section_alignment);
    std::printf("file align      : 0x%08x\n", info.file_alignment);
    std::printf("subsystem       : %u (%s) %u.%u\n",
                static_cast<unsigned>(info.subsystem),
                std::string(re2dj::exe::SubsystemName(info.subsystem)).c_str(),
                static_cast<unsigned>(info.major_subsystem_version),
                static_cast<unsigned>(info.minor_subsystem_version));
    std::printf("dll flags       : 0x%04x\n", info.dll_characteristics);
    std::printf("guest format    : %s\n",
                re2dj::exe::IsGuestExecutable(info) ? "yes" : "no");

    std::printf("\nsections (%zu):\n", info.sections.size());
    std::printf("  %-9s %-10s %-10s %-10s %-10s %s\n",
                "name", "vaddr", "vsize", "raw off", "raw size", "flags");
    for (const re2dj::exe::PeSection& section : info.sections)
    {
        std::printf("  %-9s 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
                    section.name.c_str(),
                    section.virtual_address,
                    section.virtual_size,
                    section.raw_offset,
                    section.raw_size,
                    section.characteristics);
    }

    std::printf("\ndata directories (%zu):\n", info.data_directories.size());
    for (std::size_t index = 0; index < info.data_directories.size(); ++index)
    {
        const re2dj::exe::PeDataDirectory& directory = info.data_directories[index];
        if (directory.virtual_address == 0 && directory.size == 0)
        {
            continue;
        }
        std::printf("  %-15s rva 0x%08x  size 0x%08x\n",
                    DirectoryName(index),
                    directory.virtual_address,
                    directory.size);
    }

    return kExitOk;
}
