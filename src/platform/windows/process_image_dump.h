#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace re2dj::platform::windows
{

// Saves the main image of a running guest process to a file.
//
// The protected builds carry no original code on disk: their packer decrypts
// in place once the protection is satisfied, so the only copy of the real code
// exists inside a running process. This writes that copy out, which is what
// turns those targets from "static analysis impossible" into a search.
//
// The dump keeps the image's virtual layout, so a file offset is the RVA. That
// is the property that makes it usable: an address from a diagnostic log or a
// profile's helper RVA can be looked up directly. Rewriting into file-offset
// layout would break that correspondence, and nothing here needs it, because
// the goal is analysis rather than a runnable executable.
struct ProcessImageDumpAttribution
{
    std::string target_id;
    std::string executable_path;
    // PE header values, the same ones the target profile fingerprints use, so
    // a dump can be matched back to the build it came from.
    std::uint32_t timestamp = 0;
    std::uint32_t size_of_image = 0;
    std::uint32_t entry_point_rva = 0;
    // The on-disk file this process was started from. Size and digest together
    // catch a dump that was taken from a different copy of the executable.
    std::uint64_t file_size = 0;
    std::uint64_t file_digest = 0;
    std::string re2dj_version;
    // How long the guest had been running when the dump was taken. Zero for a
    // dump taken while the process is still stopped.
    std::uint32_t delay_milliseconds = 0;
};

// A span of the image that could not be read. Zero-filled in the dump file and
// listed here, because a dump that hides its holes while looking complete
// cannot be used as evidence.
struct ProcessImageDumpGap
{
    std::uint32_t rva = 0;
    std::uint32_t bytes = 0;
};

struct ProcessImageDumpResult
{
    std::uint32_t bytes_read = 0;
    std::vector<ProcessImageDumpGap> gaps;
};

// Writes `image_path` and the attribution sidecar `sidecar_path`. `point` names
// where in the run the dump was taken and is recorded in the sidecar.
//
// Reads page by page rather than in one call: an uncommitted page anywhere in
// the image would fail a single whole-image read, which would lose the location
// of every hole along with it.
bool WriteProcessImageDump(void* process,
                           std::uint64_t image_base,
                           std::uint32_t size_of_image,
                           const char* point,
                           const std::filesystem::path& image_path,
                           const std::filesystem::path& sidecar_path,
                           const ProcessImageDumpAttribution& attribution,
                           ProcessImageDumpResult* result,
                           std::string* error);

// FNV-1a 64-bit over the bytes of the on-disk executable.
//
// Deliberately not a cryptographic hash: the repository has no such
// implementation, and the question this answers is "did this dump come from
// the file I think it did", not "could someone forge this".
std::uint64_t ComputeImageFileDigest(const std::uint8_t* bytes, std::size_t size);

}  // namespace re2dj::platform::windows
