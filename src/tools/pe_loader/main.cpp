// Non-executing PE32 image-loader report.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/runtime/pe_loader.h"
#include "re2dj/version.h"

namespace
{

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitReadError = 2;
constexpr int kExitLoadError = 3;

void PrintUsage()
{
    std::printf(
        "re2dj_pe_loader %s - map a PE32 image without executing it\n"
        "\n"
        "Usage:\n"
        "  re2dj_pe_loader <file> [load-base]\n"
        "  re2dj_pe_loader --hdd <directory> <guest-relative-path> [load-base]\n"
        "\n"
        "The optional load base accepts decimal or 0x-prefixed hexadecimal.\n",
        std::string(re2dj::VersionString()).c_str());
}

bool ReadFile(const std::filesystem::path& path,
              std::vector<std::uint8_t>* bytes,
              std::string* error)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        *error = "cannot open file";
        return false;
    }
    const std::streamoff length = stream.tellg();
    if (length <= 0 || static_cast<std::uintmax_t>(length) >
                           std::numeric_limits<std::size_t>::max())
    {
        *error = "file size is invalid";
        return false;
    }
    bytes->resize(static_cast<std::size_t>(length));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes->data()),
                static_cast<std::streamsize>(bytes->size()));
    if (!stream)
    {
        *error = "cannot read complete file";
        return false;
    }
    return true;
}

bool ParseBase(const char* text, re2dj::runtime::GuestAddress* address)
{
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 0);
    if (text == end || *end != '\0' || value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    *address = re2dj::runtime::GuestAddress(static_cast<std::uint32_t>(value));
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        PrintUsage();
        return argc < 2 ? kExitUsage : kExitOk;
    }

    std::filesystem::path file;
    int base_argument = 2;
    if (std::string(argv[1]) == "--hdd")
    {
        if (argc < 4)
        {
            PrintUsage();
            return kExitUsage;
        }
        re2dj::hdd::HddRoot root;
        std::string open_error;
        if (!re2dj::hdd::HddRoot::Open(std::filesystem::path(argv[2]), &root, &open_error) ||
            !root.ResolveFile(argv[3], &file))
        {
            std::fprintf(stderr, "error: cannot resolve guest executable: %s\n",
                         open_error.c_str());
            return kExitReadError;
        }
        base_argument = 4;
    }
    else
    {
        file = std::filesystem::path(argv[1]);
    }

    re2dj::runtime::GuestAddress requested_base;
    if (base_argument < argc && !ParseBase(argv[base_argument], &requested_base))
    {
        std::fprintf(stderr, "error: invalid 32-bit load base: %s\n", argv[base_argument]);
        return kExitUsage;
    }

    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!ReadFile(file, &bytes, &error))
    {
        std::fprintf(stderr, "error: %s: %s\n", file.string().c_str(), error.c_str());
        return kExitReadError;
    }
    re2dj::exe::PeImageInfo info;
    if (!re2dj::exe::ReadPeImageInfo(bytes.data(), bytes.size(), &info, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return kExitReadError;
    }

    re2dj::runtime::AddressSpace address_space;
    re2dj::runtime::ImportGateTable gates;
    re2dj::runtime::LoadedPeImage loaded;
    if (!re2dj::runtime::LoadPe32Image(bytes.data(),
                                      bytes.size(),
                                      info,
                                      requested_base,
                                      &address_space,
                                      &gates,
                                      &loaded,
                                      &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return kExitLoadError;
    }

    std::printf("file             : %s\n", file.string().c_str());
    std::printf("load base        : 0x%08x\n", loaded.load_base.value());
    std::printf("entry point      : 0x%08x\n", loaded.entry_point.value());
    std::printf("TLS directory    : RVA 0x%08x, size 0x%08x\n",
                loaded.tls_directory_rva,
                loaded.tls_directory_size);
    std::printf("imports (%zu):\n", loaded.imports.size());
    for (const re2dj::runtime::ImportGate& gate : loaded.imports)
    {
        if (gate.by_ordinal)
        {
            std::printf("  0x%08x  %s!#%u\n",
                        gate.address.value(),
                        gate.module.c_str(),
                        static_cast<unsigned>(gate.ordinal));
        }
        else
        {
            std::printf("  0x%08x  %s!%s\n",
                        gate.address.value(),
                        gate.module.c_str(),
                        gate.name.c_str());
        }
    }
    return kExitOk;
}
