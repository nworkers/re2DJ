// Locates a build's legacy port-I/O helpers by signature.
//
// A target profile stores the addresses of the `in`/`out` instructions the
// guest faults on. Obtaining them used to require running the build and
// reading the faulting address, which is circular for a build that stops
// because the stored value is wrong. This tool reads them out of an image
// instead: either a build whose `.text` is plaintext on disk, or the decrypted
// run-time dump produced by `--image-dump`. It decrypts nothing and writes
// nothing.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "re2dj/exe/code_scan.h"

namespace
{

constexpr std::size_t kMaxSites = 64;

void PrintUsage()
{
    std::printf(
        "re2dj_port_helper_scan - locate legacy port-I/O helpers by signature\n"
        "\n"
        "Usage:\n"
        "  re2dj_port_helper_scan <file> [base-address]\n"
        "\n"
        "The file is either a build whose .text is plaintext on disk or a\n"
        "decrypted image dump; a file offset is the RVA in both. The base\n"
        "address defaults to 0, which reports RVAs; pass 0x00400000 to report\n"
        "virtual addresses.\n");
}

bool ParseBaseAddress(const std::string& text, std::uint32_t& out)
{
    try
    {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(text, &consumed, 0);
        if (consumed != text.size() || value > 0xffffffffull)
        {
            return false;
        }
        out = static_cast<std::uint32_t>(value);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& out)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error)
    {
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    if (out.empty())
    {
        return true;
    }
    stream.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return static_cast<std::size_t>(stream.gcount()) == out.size();
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        PrintUsage();
        return 1;
    }

    const std::filesystem::path path(argv[1]);
    std::uint32_t base_address = 0;
    if (argc == 3 && !ParseBaseAddress(argv[2], base_address))
    {
        std::fprintf(stderr, "invalid base address: %s\n", argv[2]);
        return 1;
    }

    std::vector<std::uint8_t> bytes;
    if (!ReadFile(path, bytes))
    {
        std::fprintf(stderr, "cannot read file: %s\n", path.string().c_str());
        return 1;
    }

    bool capped = false;
    std::size_t total = 0;
    const std::vector<re2dj::exe::PortHelperSite> sites =
        re2dj::exe::ScanPortHelpers(bytes.data(), bytes.size(), base_address, kMaxSites,
                                    &capped, &total);

    std::printf("file          : %s\n", path.string().c_str());
    std::printf("size          : %zu\n", bytes.size());
    std::printf("base address  : 0x%08x\n", base_address);
    std::printf("helpers found : %zu\n", total);

    if (sites.empty())
    {
        // The expected result for a protected build read off disk, where .text
        // is ciphertext. Reported as a fact rather than as a failure.
        std::printf(
            "\nNo helper signature is present. For a protected build this is expected\n"
            "on disk: run it with --image-dump and scan the resumed dump instead.\n");
        return 1;
    }

    std::printf("\n  %-10s %-18s %s\n", "helper", "signature", "opcode address");
    for (const re2dj::exe::PortHelperSite& site : sites)
    {
        std::printf("  %-10s 0x%08x         0x%08x\n",
                    re2dj::exe::PortHelperKindName(site.kind), site.signature_offset,
                    site.opcode_address);
    }
    if (capped)
    {
        std::printf("\n  listing capped at %zu of %zu\n", sites.size(), total);
    }
    std::printf(
        "\nThe search is syntactic: these bytes can also occur inside another\n"
        "instruction or in data embedded in code, so each hit is a candidate.\n"
        "The opcode address is what a privileged fault reports and what a target\n"
        "profile stores.\n");
    return 0;
}
