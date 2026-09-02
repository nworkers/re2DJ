// Byte-statistics judge for regions that are supposed to become x86 code.
//
// The protected ez2dj4th image is ciphertext throughout, so a Hardlock
// transform response candidate is judged by whether the region it should
// decrypt stops reading as ciphertext. This tool performs no decryption and
// knows no transform: it measures bytes and reports a verdict.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "re2dj/analysis/code_region_score.h"
#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_root.h"
#include "re2dj/storage/fat32_chd.h"
#include "re2dj/version.h"

namespace
{

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitReadError = 2;
// Distinct from a read failure so a candidate judgement loop can tell "the run
// produced no code" from "the run could not be measured at all".
constexpr int kExitNotCodeLike = 3;

void PrintUsage()
{
    std::printf(
        "re2dj_code_score %s - judge whether a byte region reads as x86 code or ciphertext\n"
        "\n"
        "Usage:\n"
        "  re2dj_code_score <file> [options]\n"
        "  re2dj_code_score --hdd <directory> --guest-path <relative> [options]\n"
        "  re2dj_code_score --chd <image> --guest-path <relative> [options]\n"
        "\n"
        "Options:\n"
        "  --section <name>   score only this PE section\n"
        "  --raw              score the byte range instead of walking PE sections\n"
        "  --offset <bytes>   start of the raw range (implies --raw)\n"
        "  --length <bytes>   length of the raw range (implies --raw)\n"
        "  --chunk <bytes>    chunk size for per-chunk rows, 0 disables them\n"
        "  --require-code     exit %d unless some scored region reads as code\n"
        "\n"
        "Numbers accept decimal or 0x hexadecimal.\n",
        std::string(re2dj::VersionString()).c_str(),
        kExitNotCodeLike);
}

bool ParseSize(const char* text, std::size_t* out)
{
    if (text == nullptr || *text == 0 || out == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 0);
    if (end == text || *end != 0)
    {
        return false;
    }
    *out = static_cast<std::size_t>(value);
    return true;
}

bool ReadWholeFile(const std::filesystem::path& path,
                   std::vector<std::uint8_t>* bytes,
                   std::string* error)
{
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code)
    {
        *error = code.message();
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error = "cannot open file";
        return false;
    }
    bytes->resize(static_cast<std::size_t>(size));
    if (size != 0 &&
        !stream.read(reinterpret_cast<char*>(bytes->data()),
                     static_cast<std::streamsize>(size)))
    {
        *error = "cannot read file";
        return false;
    }
    return true;
}

void PrintHeader()
{
    std::printf("%-20s %10s %8s %10s %8s %7s  %s\n",
                "region",
                "bytes",
                "entropy",
                "prologues",
                "cc-runs",
                "zero%",
                "verdict");
}

void PrintRow(const std::string& label, const re2dj::analysis::CodeRegionScore& score)
{
    std::printf("%-20s %10zu %8.4f %10zu %8zu %6.2f%%  %s\n",
                label.c_str(),
                score.byte_count,
                score.entropy_bits_per_byte,
                score.prologue_count,
                score.padding_run_count,
                score.zero_byte_share * 100.0,
                std::string(re2dj::analysis::CodeRegionVerdictName(score.verdict)).c_str());
}

// Scores one region and its chunks, returning true when any of them reads as
// code. A chunk-level hit matters on its own: one response decrypts a single
// 32 KiB chunk, which would stay invisible in a whole-section average.
bool ScoreRegion(const std::string& label,
                 std::span<const std::uint8_t> bytes,
                 std::size_t chunk_size)
{
    const re2dj::analysis::CodeRegionScore summary = re2dj::analysis::ScoreCodeRegion(bytes);
    PrintRow(label, summary);
    bool code_like = summary.verdict == re2dj::analysis::CodeRegionVerdict::kCodeLike;
    if (chunk_size == 0 || bytes.size() <= chunk_size)
    {
        return code_like;
    }
    const std::vector<re2dj::analysis::CodeRegionScore> chunks =
        re2dj::analysis::ScoreCodeRegionChunks(bytes, chunk_size);
    for (std::size_t index = 0; index < chunks.size(); ++index)
    {
        char chunk_label[32] = {};
        std::snprintf(chunk_label,
                      sizeof(chunk_label),
                      "  +0x%06llx",
                      static_cast<unsigned long long>(index * chunk_size));
        PrintRow(chunk_label, chunks[index]);
        code_like = code_like ||
                    chunks[index].verdict == re2dj::analysis::CodeRegionVerdict::kCodeLike;
    }
    return code_like;
}

}  // namespace

int main(int argc, char** argv)
{
    std::filesystem::path file;
    std::filesystem::path hdd_root;
    std::filesystem::path chd_image;
    std::string guest_path;
    std::string section_filter;
    bool raw = false;
    std::size_t offset = 0;
    std::size_t length = 0;
    bool length_given = false;
    std::size_t chunk = re2dj::analysis::kDefaultChunkBytes;
    bool require_code = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string option = argv[index];
        if (option == "--help" || option == "-h")
        {
            PrintUsage();
            return kExitOk;
        }
        else if (option == "--hdd" && index + 1 < argc)
        {
            hdd_root = std::filesystem::path(argv[++index]);
        }
        else if (option == "--chd" && index + 1 < argc)
        {
            chd_image = std::filesystem::path(argv[++index]);
        }
        else if (option == "--guest-path" && index + 1 < argc)
        {
            guest_path = argv[++index];
        }
        else if (option == "--section" && index + 1 < argc)
        {
            section_filter = argv[++index];
        }
        else if (option == "--raw")
        {
            raw = true;
        }
        else if (option == "--offset" && index + 1 < argc)
        {
            if (!ParseSize(argv[++index], &offset))
            {
                std::fprintf(stderr, "error: --offset expects a number\n");
                return kExitUsage;
            }
            raw = true;
        }
        else if (option == "--length" && index + 1 < argc)
        {
            if (!ParseSize(argv[++index], &length))
            {
                std::fprintf(stderr, "error: --length expects a number\n");
                return kExitUsage;
            }
            length_given = true;
            raw = true;
        }
        else if (option == "--chunk" && index + 1 < argc)
        {
            if (!ParseSize(argv[++index], &chunk))
            {
                std::fprintf(stderr, "error: --chunk expects a number\n");
                return kExitUsage;
            }
        }
        else if (option == "--require-code")
        {
            require_code = true;
        }
        else if (!option.empty() && option.front() != '-' && file.empty())
        {
            file = std::filesystem::path(option);
        }
        else
        {
            std::fprintf(stderr, "error: unrecognized argument %s\n", option.c_str());
            PrintUsage();
            return kExitUsage;
        }
    }

    const bool from_hdd = !hdd_root.empty();
    const bool from_chd = !chd_image.empty();
    const int sources = static_cast<int>(!file.empty()) + static_cast<int>(from_hdd) +
                        static_cast<int>(from_chd);
    if (sources != 1)
    {
        std::fprintf(stderr, "error: name exactly one source: a file, --hdd, or --chd\n");
        PrintUsage();
        return kExitUsage;
    }
    if ((from_hdd || from_chd) && guest_path.empty())
    {
        std::fprintf(stderr, "error: --hdd and --chd require --guest-path\n");
        return kExitUsage;
    }

    std::vector<std::uint8_t> bytes;
    std::string error;
    std::string source_label;
    if (from_chd)
    {
        std::unique_ptr<re2dj::storage::Fat32Volume> volume;
        if (!re2dj::storage::Fat32Volume::Open(chd_image, &volume, &error))
        {
            std::fprintf(stderr, "error: %s: %s\n", chd_image.string().c_str(), error.c_str());
            return kExitReadError;
        }
        if (!volume->ReadFile(guest_path, &bytes, &error))
        {
            std::fprintf(stderr, "error: %s: %s\n", guest_path.c_str(), error.c_str());
            return kExitReadError;
        }
        source_label = chd_image.string() + ":" + guest_path;
    }
    else
    {
        std::filesystem::path resolved = file;
        if (from_hdd)
        {
            re2dj::hdd::HddRoot root;
            if (!re2dj::hdd::HddRoot::Open(hdd_root, &root, &error))
            {
                std::fprintf(stderr, "error: %s\n", error.c_str());
                return kExitReadError;
            }
            if (!root.ResolveFile(guest_path, &resolved))
            {
                std::fprintf(stderr,
                             "error: %s does not resolve to a file under %s\n",
                             guest_path.c_str(),
                             root.root().string().c_str());
                return kExitReadError;
            }
        }
        if (!ReadWholeFile(resolved, &bytes, &error))
        {
            std::fprintf(stderr, "error: %s: %s\n", resolved.string().c_str(), error.c_str());
            return kExitReadError;
        }
        source_label = resolved.string();
    }

    std::printf("source          : %s\n", source_label.c_str());
    std::printf("bytes           : %zu\n", bytes.size());
    std::printf("chunk           : 0x%llx\n", static_cast<unsigned long long>(chunk));
    std::printf("thresholds      : code entropy <= %.1f with a prologue, "
                "ciphertext entropy >= %.1f with none (heuristic)\n",
                re2dj::analysis::kCodeEntropyCeiling,
                re2dj::analysis::kCiphertextEntropyFloor);

    re2dj::exe::PeImageInfo info;
    std::string pe_error;
    const bool is_pe =
        !raw && re2dj::exe::ReadPeImageInfo(bytes.data(), bytes.size(), &info, &pe_error);
    bool code_like = false;
    std::printf("\n");
    PrintHeader();
    if (is_pe)
    {
        std::size_t scored_sections = 0;
        for (const re2dj::exe::PeSection& section : info.sections)
        {
            if (!section_filter.empty() && section.name != section_filter)
            {
                continue;
            }
            if (section.raw_size == 0 || section.raw_offset >= bytes.size())
            {
                continue;
            }
            const std::size_t available = bytes.size() - section.raw_offset;
            const std::size_t size =
                (std::min)(static_cast<std::size_t>(section.raw_size), available);
            const std::span<const std::uint8_t> region =
                std::span<const std::uint8_t>(bytes).subspan(section.raw_offset, size);
            code_like = ScoreRegion(section.name, region, chunk) || code_like;
            ++scored_sections;
        }
        if (scored_sections == 0)
        {
            std::fprintf(stderr,
                         "error: no PE section matched %s\n",
                         section_filter.empty() ? "the image" : section_filter.c_str());
            return kExitReadError;
        }
    }
    else
    {
        if (offset >= bytes.size())
        {
            std::fprintf(stderr,
                         "error: offset 0x%llx is past the end of %zu bytes\n",
                         static_cast<unsigned long long>(offset),
                         bytes.size());
            return kExitReadError;
        }
        const std::size_t available = bytes.size() - offset;
        const std::size_t size = length_given ? (std::min)(length, available) : available;
        char label[32] = {};
        std::snprintf(label, sizeof(label), "0x%08llx", static_cast<unsigned long long>(offset));
        const std::span<const std::uint8_t> region =
            std::span<const std::uint8_t>(bytes).subspan(offset, size);
        code_like = ScoreRegion(label, region, chunk);
    }

    std::printf("\nany region code-like : %s\n", code_like ? "yes" : "no");
    if (require_code && !code_like)
    {
        return kExitNotCodeLike;
    }
    return kExitOk;
}
