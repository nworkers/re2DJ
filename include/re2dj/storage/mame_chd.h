#ifndef RE2DJ_STORAGE_MAME_CHD_H_
#define RE2DJ_STORAGE_MAME_CHD_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace re2dj::storage
{

enum class MameChdCodec : std::uint8_t
{
    kNone,
    kLzma,
    kZlib,
    kHuffman,
    kFlac,
    kZstd,
    kAvHuff,
    kCdZlib,
    kCdLzma,
    kCdFlac,
    kCdZstd,
    kUnknown,
};

struct MameChdMetadata
{
    std::uint32_t tag = 0;
    std::uint8_t flags = 0;
    std::vector<std::uint8_t> payload;
};

struct MameChdInfo
{
    std::uint32_t version = 0;
    std::uint64_t logical_bytes = 0;
    std::uint64_t metadata_offset = 0;
    std::uint32_t hunk_bytes = 0;
    std::uint32_t unit_bytes = 0;
    std::uint64_t hunk_count = 0;
    std::vector<MameChdCodec> codecs;
    std::vector<MameChdMetadata> metadata;
};

// Read-only MAME CHD hard-disk image backed by libchdr. The original image is
// never modified; callers receive logical bytes reconstructed by libchdr.
class MameChdImage
{
public:
    MameChdImage() = delete;
    ~MameChdImage();

    MameChdImage(const MameChdImage&) = delete;
    MameChdImage& operator=(const MameChdImage&) = delete;

    static bool Open(const std::filesystem::path& path,
                     std::unique_ptr<MameChdImage>* out,
                     std::string* error);

    const MameChdInfo& info() const
    {
        return info_;
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

    // Reads a logical byte range. Reads may cross hunk boundaries and are
    // clipped only by the logical image size.
    bool Read(std::uint64_t offset,
              void* destination,
              std::size_t length,
              std::string* error);

    bool ReadSector(std::uint64_t lba,
                    std::vector<std::uint8_t>* sector,
                    std::string* error);

private:
    MameChdImage(std::filesystem::path path, void* handle, MameChdInfo info);

    std::filesystem::path path_;
    void* handle_ = nullptr;
    MameChdInfo info_;
};

std::string MameChdCodecName(MameChdCodec codec);

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_MAME_CHD_H_
