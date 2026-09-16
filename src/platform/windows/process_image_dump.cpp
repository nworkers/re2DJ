#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <vector>

#include "process_image_dump.h"

namespace re2dj::platform::windows
{
namespace
{

// Read granularity. A page is the unit the memory manager commits, so it is
// also the unit at which a hole can begin or end.
constexpr std::uint32_t kChunkBytes = 0x1000;

void AppendGap(std::vector<ProcessImageDumpGap>* gaps, std::uint32_t rva, std::uint32_t bytes)
{
    // Merge with the previous gap when they touch, so a large uncommitted
    // region reads as one range rather than hundreds of page-sized entries.
    if (!gaps->empty() && gaps->back().rva + gaps->back().bytes == rva)
    {
        gaps->back().bytes += bytes;
        return;
    }
    gaps->push_back({rva, bytes});
}

bool WriteSidecar(const std::filesystem::path& path,
                  const char* point,
                  std::uint64_t image_base,
                  const ProcessImageDumpAttribution& attribution,
                  const ProcessImageDumpResult& result,
                  std::string* error)
{
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr)
    {
        *error = "cannot create image dump sidecar";
        return false;
    }
    std::fprintf(file, "{\n");
    std::fprintf(file, "  \"target\": \"%s\",\n", attribution.target_id.c_str());
    std::fprintf(file, "  \"executable\": \"%s\",\n", attribution.executable_path.c_str());
    std::fprintf(file, "  \"point\": \"%s\",\n", point);
    std::fprintf(file, "  \"delay_ms\": %u,\n", attribution.delay_milliseconds);
    std::fprintf(file, "  \"image_base\": \"0x%08llx\",\n",
                 static_cast<unsigned long long>(image_base));
    std::fprintf(file, "  \"size_of_image\": \"0x%08x\",\n", attribution.size_of_image);
    std::fprintf(file, "  \"entry_point_rva\": \"0x%08x\",\n", attribution.entry_point_rva);
    std::fprintf(file, "  \"timestamp\": \"0x%08x\",\n", attribution.timestamp);
    std::fprintf(file, "  \"file_size\": %llu,\n",
                 static_cast<unsigned long long>(attribution.file_size));
    // The algorithm is named rather than left implicit: this is not a
    // cryptographic hash and must not be read as one.
    std::fprintf(file, "  \"file_digest_algorithm\": \"fnv1a64\",\n");
    std::fprintf(file, "  \"file_digest\": \"0x%016llx\",\n",
                 static_cast<unsigned long long>(attribution.file_digest));
    std::fprintf(file, "  \"re2dj_version\": \"%s\",\n", attribution.re2dj_version.c_str());
    std::fprintf(file, "  \"bytes_read\": %u,\n", result.bytes_read);
    std::fprintf(file, "  \"layout\": \"virtual\",\n");
    std::fprintf(file, "  \"gaps\": [");
    for (std::size_t index = 0; index < result.gaps.size(); ++index)
    {
        std::fprintf(file,
                     "%s\n    {\"rva\": \"0x%08x\", \"bytes\": \"0x%08x\"}",
                     index == 0 ? "" : ",",
                     result.gaps[index].rva,
                     result.gaps[index].bytes);
    }
    std::fprintf(file, "%s]\n}\n", result.gaps.empty() ? "" : "\n  ");
    const bool closed = std::fclose(file) == 0;
    if (!closed)
    {
        *error = "cannot finish image dump sidecar";
    }
    return closed;
}

}  // namespace

std::uint64_t ComputeImageFileDigest(const std::uint8_t* bytes, std::size_t size)
{
    std::uint64_t digest = 0xcbf29ce484222325ull;
    for (std::size_t index = 0; index < size; ++index)
    {
        digest ^= static_cast<std::uint64_t>(bytes[index]);
        digest *= 0x100000001b3ull;
    }
    return digest;
}

bool WriteProcessImageDump(void* process,
                           std::uint64_t image_base,
                           std::uint32_t size_of_image,
                           const char* point,
                           const std::filesystem::path& image_path,
                           const std::filesystem::path& sidecar_path,
                           const ProcessImageDumpAttribution& attribution,
                           ProcessImageDumpResult* result,
                           std::string* error)
{
    if (process == nullptr || point == nullptr || result == nullptr || error == nullptr)
    {
        return false;
    }
    if (size_of_image == 0)
    {
        *error = "image dump requires a non-zero image size";
        return false;
    }
    *result = {};

    std::FILE* file = nullptr;
    if (_wfopen_s(&file, image_path.c_str(), L"wb") != 0 || file == nullptr)
    {
        *error = "cannot create image dump file";
        return false;
    }

    std::vector<std::uint8_t> chunk(kChunkBytes);
    bool written = true;
    for (std::uint32_t offset = 0; written && offset < size_of_image; offset += kChunkBytes)
    {
        const std::uint32_t bytes =
            size_of_image - offset < kChunkBytes ? size_of_image - offset : kChunkBytes;
        SIZE_T copied = 0;
        const bool read = ReadProcessMemory(process,
                                            reinterpret_cast<const void*>(
                                                static_cast<std::uintptr_t>(image_base + offset)),
                                            chunk.data(),
                                            bytes,
                                            &copied) != FALSE &&
                          copied == bytes;
        if (read)
        {
            result->bytes_read += bytes;
        }
        else
        {
            // Zero-filled so the file keeps its virtual layout and every later
            // offset still equals its RVA. The gap list is what says the bytes
            // are ours rather than the guest's.
            std::fill(chunk.begin(), chunk.begin() + bytes, static_cast<std::uint8_t>(0));
            AppendGap(&result->gaps, offset, bytes);
        }
        written = std::fwrite(chunk.data(), 1, bytes, file) == bytes;
    }
    const bool closed = std::fclose(file) == 0;
    if (!written || !closed)
    {
        *error = "cannot write image dump file";
        return false;
    }
    return WriteSidecar(sidecar_path, point, image_base, attribution, *result, error);
}

}  // namespace re2dj::platform::windows
