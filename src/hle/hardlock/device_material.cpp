#include "re2dj/hle/hardlock/device_material.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

#include "re2dj/config/hardlock_secret_config.h"
#include "re2dj/hle/hardlock/api_descriptor.h"

namespace re2dj::hle::hardlock
{
namespace
{

// Accepts the forms the launcher always has: decimal, 0x hex, or 0 octal,
// keeping the low sixteen bits.
bool ParseSeedWord(const std::string& text, std::uint16_t* value)
{
    if (value == nullptr || text.empty())
    {
        return false;
    }
    try
    {
        *value = static_cast<std::uint16_t>(std::stoul(text, nullptr, 0) & 0xffffU);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ReadTransformMap(const std::filesystem::path& path,
                      HardlockTransformResponseMap* map,
                      std::string* error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error = "cannot open Hardlock transform map";
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    return ParseHardlockTransformResponseMap(text, map, error);
}

}  // namespace

bool ResolveHardlockDeviceMaterial(const HardlockMaterialSources& sources,
                                   HardlockDeviceMaterial* material,
                                   std::string* error)
{
    if (material == nullptr || error == nullptr)
    {
        if (error != nullptr) *error = "invalid Hardlock material arguments";
        return false;
    }
    *material = {};
    material->device_enabled = sources.device_requested;
    std::string handshake_hex = sources.handshake_response_hex;
    std::string tail_hex = sources.descriptor_tail_hex;
    std::filesystem::path map_path = sources.transform_map_path;

    if (sources.use_profile_cfg)
    {
        config::HardlockSecretMaterial cfg;
        bool section_found = false;
        if (!config::LoadHardlockProfileMaterial(
                sources.config_path, sources.profile_id, &cfg, &section_found, error))
        {
            return false;
        }
        if (map_path.empty() && !sources.default_map_path.empty())
        {
            std::error_code code;
            if (std::filesystem::exists(sources.default_map_path, code) && !code)
            {
                map_path = sources.default_map_path;
                // A map is only consumed at the device boundary, so choosing
                // one enables that boundary as the explicit option does.
                material->device_enabled = true;
                material->cfg_map = true;
            }
        }
        const bool cfg_has_seeds = !cfg.module_address_hex.empty() && !cfg.seed1_hex.empty() &&
                                   !cfg.seed2_hex.empty() && !cfg.seed3_hex.empty();
        if (cfg_has_seeds)
        {
            HardlockSeeds seeds;
            if (ParseSeedWord(cfg.module_address_hex, &seeds.module_address) &&
                ParseSeedWord(cfg.seed1_hex, &seeds.seed1) &&
                ParseSeedWord(cfg.seed2_hex, &seeds.seed2) &&
                ParseSeedWord(cfg.seed3_hex, &seeds.seed3))
            {
                material->seeds = seeds;
                material->device_enabled = true;
            }
        }
        // The replay values accompany a map or seeds; an explicit option wins.
        if (section_found && (!map_path.empty() || cfg_has_seeds))
        {
            if (handshake_hex.empty() && !cfg.handshake_response_hex.empty())
            {
                handshake_hex = cfg.handshake_response_hex;
                material->cfg_handshake = true;
            }
            if (tail_hex.empty() && !cfg.descriptor_tail_hex.empty())
            {
                tail_hex = cfg.descriptor_tail_hex;
                material->cfg_tail = true;
            }
        }
    }

    if (!tail_hex.empty())
    {
        std::uint16_t tail = 0;
        if (!ParseHardlockApiTailWordHex(tail_hex, &tail, error))
        {
            return false;
        }
        material->descriptor_tail_word = tail;
    }
    if (!handshake_hex.empty())
    {
        HardlockHandshakeResponse response = {};
        if (!ParseHardlockHandshakeResponse(handshake_hex, &response, error))
        {
            return false;
        }
        material->handshake_response = response;
    }
    if (!map_path.empty())
    {
        if (!ReadTransformMap(map_path, &material->transform_map, error))
        {
            return false;
        }
        material->transform_map_path = map_path;
    }
    error->clear();
    return true;
}

HardlockDeviceOptions MakeHardlockDeviceOptions(const HardlockDeviceMaterial& material)
{
    HardlockDeviceOptions options;
    options.handshake_response = material.handshake_response;
    options.descriptor_tail_word = material.descriptor_tail_word;
    options.seeds = material.seeds;
    options.transform_responses = material.transform_map.blocks;
    options.payload_responses = material.transform_map.payloads;
    return options;
}

}  // namespace re2dj::hle::hardlock
