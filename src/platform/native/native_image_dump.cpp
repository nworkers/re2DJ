#include "native_image_dump.h"

#include <cstdio>
#include <fstream>
#include <system_error>

namespace re2dj::platform::native
{
namespace
{

std::string JsonString(const std::string& text)
{
    std::string quoted = "\"";
    for (const char character : text)
    {
        if (character == '"' || character == '\\')
        {
            quoted.push_back('\\');
            quoted.push_back(character);
        }
        else if (static_cast<unsigned char>(character) < 0x20)
        {
            char escaped[8] = {};
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(character));
            quoted += escaped;
        }
        else
        {
            quoted.push_back(character);
        }
    }
    quoted.push_back('"');
    return quoted;
}

}  // namespace

std::uint64_t NativeImageDumpDigest(const std::uint8_t* bytes, std::size_t size)
{
    std::uint64_t digest = 0xcbf29ce484222325ull;
    for (std::size_t index = 0; index < size; ++index)
    {
        digest ^= static_cast<std::uint64_t>(bytes[index]);
        digest *= 0x100000001b3ull;
    }
    return digest;
}

bool WriteNativeImageDump(const void* image,
                          const std::filesystem::path& directory,
                          const std::string& stem,
                          const char* point,
                          const NativeImageDumpAttribution& attribution,
                          std::string* error)
{
    if (image == nullptr || point == nullptr || error == nullptr || attribution.size_of_image == 0)
    {
        if (error != nullptr) *error = "invalid image dump arguments";
        return false;
    }
    std::error_code created;
    std::filesystem::create_directories(directory, created);
    const std::filesystem::path image_path = directory / (stem + "." + point + ".image.bin");
    const std::filesystem::path sidecar_path = directory / (stem + "." + point + ".image.json");
    {
        std::ofstream stream(image_path, std::ios::binary | std::ios::trunc);
        stream.write(static_cast<const char*>(image), static_cast<std::streamsize>(attribution.size_of_image));
        if (!stream)
        {
            *error = "cannot write " + image_path.string();
            return false;
        }
    }
    std::ofstream sidecar(sidecar_path, std::ios::trunc);
    char number[32] = {};
    const auto hex32 = [&](std::uint32_t value) {
        std::snprintf(number, sizeof(number), "\"0x%08x\"", static_cast<unsigned>(value));
        return std::string(number);
    };
    std::snprintf(number, sizeof(number), "\"0x%016llx\"", static_cast<unsigned long long>(attribution.file_digest));
    const std::string digest = number;
    // The in-process image is mapped whole, so it has no holes: gaps stays
    // empty, as the format expects when every byte was read.
    sidecar << "{\n"
            << "  \"target\": " << JsonString(attribution.target_id) << ",\n"
            << "  \"executable\": " << JsonString(attribution.executable_path) << ",\n"
            << "  \"point\": " << JsonString(point) << ",\n"
            << "  \"delay_ms\": " << attribution.delay_milliseconds << ",\n"
            << "  \"timestamp\": " << hex32(attribution.timestamp) << ",\n"
            << "  \"size_of_image\": " << hex32(attribution.size_of_image) << ",\n"
            << "  \"entry_point_rva\": " << hex32(attribution.entry_point_rva) << ",\n"
            << "  \"image_base\": " << hex32(attribution.image_base) << ",\n"
            << "  \"file_size\": " << attribution.file_size << ",\n"
            << "  \"file_digest_algorithm\": \"fnv1a64\",\n"
            << "  \"file_digest\": " << digest << ",\n"
            << "  \"re2dj_version\": " << JsonString(attribution.re2dj_version) << ",\n"
            << "  \"layout\": \"virtual\",\n"
            << "  \"source\": \"in-process\",\n"
            << "  \"gaps\": []\n"
            << "}\n";
    if (!sidecar)
    {
        *error = "cannot write " + sidecar_path.string();
        return false;
    }
    error->clear();
    return true;
}

}  // namespace re2dj::platform::native
