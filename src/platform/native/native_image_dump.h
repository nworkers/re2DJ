#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_IMAGE_DUMP_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_IMAGE_DUMP_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

// --image-dump on the in-process runner (task 449, after design 292): the
// guest's main image written out as it is mapped in this process, in its
// virtual layout so a file offset is the RVA, with a JSON sidecar naming the
// build. Protected builds decrypt in place once they run, so the copy taken
// after a delay holds code their disk file does not. The format is the one
// the Windows injection path wrote, so docs/guides/decrypted-image-dump.md
// still applies.
namespace re2dj::platform::native
{

// Which build a dump came from, and when in the run it was taken.
struct NativeImageDumpAttribution
{
    std::string target_id;
    std::string executable_path;
    std::uint32_t timestamp = 0;
    std::uint32_t size_of_image = 0;
    std::uint32_t entry_point_rva = 0;
    std::uint32_t image_base = 0;
    std::uint64_t file_size = 0;
    std::uint64_t file_digest = 0;
    std::string re2dj_version;
    std::uint32_t delay_milliseconds = 0;
};

// Writes image (the mapped image, size_of_image bytes) to
// <directory>/<stem>.<point>.image.bin and the sidecar to .image.json.
bool WriteNativeImageDump(const void* image,
                          const std::filesystem::path& directory,
                          const std::string& stem,
                          const char* point,
                          const NativeImageDumpAttribution& attribution,
                          std::string* error);

// FNV-1a 64-bit over the executable's bytes: tells copies apart, not a
// cryptographic hash.
std::uint64_t NativeImageDumpDigest(const std::uint8_t* bytes, std::size_t size);

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_IMAGE_DUMP_H_
