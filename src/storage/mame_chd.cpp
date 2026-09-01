#include "re2dj/storage/mame_chd.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <libchdr/chd.h>

namespace re2dj::storage
{

namespace
{

MameChdCodec DecodeCodec(std::uint32_t id)
{
    switch (id)
    {
    case CHD_CODEC_NONE:
        return MameChdCodec::kNone;
    case CHD_CODEC_LZMA:
        return MameChdCodec::kLzma;
    case CHD_CODEC_ZLIB:
        return MameChdCodec::kZlib;
    case CHD_CODEC_HUFFMAN:
        return MameChdCodec::kHuffman;
    case CHD_CODEC_FLAC:
        return MameChdCodec::kFlac;
    case CHD_CODEC_ZSTD:
        return MameChdCodec::kZstd;
    case CHD_CODEC_AVHUFF:
        return MameChdCodec::kAvHuff;
    case CHD_CODEC_CD_ZLIB:
        return MameChdCodec::kCdZlib;
    case CHD_CODEC_CD_LZMA:
        return MameChdCodec::kCdLzma;
    case CHD_CODEC_CD_FLAC:
        return MameChdCodec::kCdFlac;
    case CHD_CODEC_CD_ZSTD:
        return MameChdCodec::kCdZstd;
    default:
        return MameChdCodec::kUnknown;
    }
}

std::string ChdError(chd_error error)
{
    const char* text = chd_error_string(error);
    return text == nullptr ? "unknown libchdr error" : text;
}

}  // namespace

std::string MameChdCodecName(MameChdCodec codec)
{
    switch (codec)
    {
    case MameChdCodec::kNone:
        return "none";
    case MameChdCodec::kLzma:
        return "lzma";
    case MameChdCodec::kZlib:
        return "zlib";
    case MameChdCodec::kHuffman:
        return "huff";
    case MameChdCodec::kFlac:
        return "flac";
    case MameChdCodec::kZstd:
        return "zstd";
    case MameChdCodec::kAvHuff:
        return "avhuff";
    case MameChdCodec::kCdZlib:
        return "cdzlib";
    case MameChdCodec::kCdLzma:
        return "cdlzma";
    case MameChdCodec::kCdFlac:
        return "cdflac";
    case MameChdCodec::kCdZstd:
        return "cdzstd";
    case MameChdCodec::kUnknown:
    default:
        return "unknown";
    }
}

MameChdImage::MameChdImage(std::filesystem::path path, void* handle, MameChdInfo info)
    : path_(std::move(path)), handle_(handle), info_(std::move(info))
{
}

MameChdImage::~MameChdImage()
{
    if (handle_ != nullptr)
    {
        chd_close(static_cast<chd_file*>(handle_));
    }
}

bool MameChdImage::Open(const std::filesystem::path& path,
                        std::unique_ptr<MameChdImage>* out,
                        std::string* error)
{
    const auto fail = [error](std::string message)
    {
        if (error != nullptr)
        {
            *error = std::move(message);
        }
        return false;
    };
    if (out == nullptr || path.empty())
    {
        return fail("CHD path or output is empty");
    }

    chd_file* handle = nullptr;
    const std::string native_path = path.string();
    const chd_error open_error = chd_open(native_path.c_str(), CHD_OPEN_READ, nullptr, &handle);
    if (open_error != CHDERR_NONE || handle == nullptr)
    {
        return fail("cannot open CHD '" + path.string() + "': " + ChdError(open_error));
    }

    const chd_header* header = chd_get_header(handle);
    if (header == nullptr || header->hunkbytes == 0 || header->unitbytes == 0 ||
        header->logicalbytes == 0)
    {
        chd_close(handle);
        return fail("libchdr returned an invalid CHD header");
    }
    MameChdInfo info;
    info.version = header->version;
    info.logical_bytes = header->logicalbytes;
    info.metadata_offset = header->metaoffset;
    info.hunk_bytes = header->hunkbytes;
    info.unit_bytes = header->unitbytes;
    info.hunk_count = header->totalhunks;
    for (const std::uint32_t codec : header->compression)
    {
        info.codecs.push_back(DecodeCodec(codec));
    }

    // libchdr exposes the complete metadata chain through wildcard lookup.
    // Keep the buffer bounded; original hard-disk geometry records are tiny.
    constexpr std::uint32_t kMetadataBufferBytes = 1024 * 1024;
    std::vector<std::uint8_t> metadata_buffer(kMetadataBufferBytes);
    for (std::uint32_t index = 0; index < 1024; ++index)
    {
        std::uint32_t result_length = 0;
        std::uint32_t result_tag = 0;
        std::uint8_t result_flags = 0;
        const chd_error metadata_error = chd_get_metadata(handle,
                                                          CHDMETATAG_WILDCARD,
                                                          index,
                                                          metadata_buffer.data(),
                                                          static_cast<std::uint32_t>(
                                                              metadata_buffer.size()),
                                                          &result_length,
                                                          &result_tag,
                                                          &result_flags);
        if (metadata_error == CHDERR_METADATA_NOT_FOUND)
        {
            break;
        }
        if (metadata_error != CHDERR_NONE || result_length > metadata_buffer.size())
        {
            chd_close(handle);
            return fail("cannot read CHD metadata: " + ChdError(metadata_error));
        }
        MameChdMetadata metadata;
        metadata.tag = result_tag;
        metadata.flags = result_flags;
        metadata.payload.assign(metadata_buffer.begin(),
                                metadata_buffer.begin() + result_length);
        info.metadata.push_back(std::move(metadata));
    }

    *out = std::unique_ptr<MameChdImage>(new MameChdImage(path, handle, std::move(info)));
    return true;
}

bool MameChdImage::Read(std::uint64_t offset,
                        void* destination,
                        std::size_t length,
                        std::string* error)
{
    if (handle_ == nullptr || offset > info_.logical_bytes ||
        length > info_.logical_bytes - offset)
    {
        if (error != nullptr)
        {
            *error = "CHD logical read range is outside the image";
        }
        return false;
    }
    if (length == 0)
    {
        return true;
    }
    if (destination == nullptr)
    {
        if (error != nullptr)
        {
            *error = "CHD logical read destination is null";
        }
        return false;
    }
    auto* output = static_cast<std::uint8_t*>(destination);
    std::size_t remaining = length;
    std::uint64_t current_offset = offset;
    std::vector<std::uint8_t> hunk(info_.hunk_bytes);
    while (remaining != 0)
    {
        const std::uint64_t hunk_number = current_offset / info_.hunk_bytes;
        if (hunk_number > std::numeric_limits<std::uint32_t>::max())
        {
            if (error != nullptr)
            {
                *error = "CHD hunk index exceeds libchdr's API range";
            }
            return false;
        }
        const std::size_t within =
            static_cast<std::size_t>(current_offset % info_.hunk_bytes);
        const std::size_t count = std::min(
            remaining, static_cast<std::size_t>(info_.hunk_bytes) - within);
        const chd_error read_error =
            chd_read(static_cast<chd_file*>(handle_), static_cast<std::uint32_t>(hunk_number), hunk.data());
        if (read_error != CHDERR_NONE)
        {
            if (error != nullptr)
            {
                *error = "cannot read CHD hunk " + std::to_string(hunk_number) + ": " +
                         ChdError(read_error);
            }
            return false;
        }
        std::copy_n(hunk.begin() + static_cast<std::ptrdiff_t>(within), count, output);
        output += count;
        current_offset += count;
        remaining -= count;
    }
    return true;
}

bool MameChdImage::ReadSector(std::uint64_t lba,
                              std::vector<std::uint8_t>* sector,
                              std::string* error)
{
    if (sector == nullptr || info_.unit_bytes == 0 ||
        info_.logical_bytes % info_.unit_bytes != 0 ||
        lba >= info_.logical_bytes / info_.unit_bytes)
    {
        if (error != nullptr)
        {
            *error = "CHD sector index is outside the image";
        }
        return false;
    }
    sector->assign(info_.unit_bytes, 0);
    return Read(lba * info_.unit_bytes, sector->data(), sector->size(), error);
}

}  // namespace re2dj::storage
