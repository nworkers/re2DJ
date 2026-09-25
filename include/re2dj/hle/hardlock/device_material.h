#ifndef RE2DJ_HLE_HARDLOCK_DEVICE_MATERIAL_H_
#define RE2DJ_HLE_HARDLOCK_DEVICE_MATERIAL_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "re2dj/hle/hardlock/device.h"
#include "re2dj/hle/hardlock/engine.h"
#include "re2dj/hle/hardlock/handshake_response.h"
#include "re2dj/hle/hardlock/transform_responses.h"

namespace re2dj::hle::hardlock
{

// Where the Hardlock device material for one run may come from. The material
// itself is produced outside this repository; this only decides which of the
// user's files and options apply.
struct HardlockMaterialSources
{
    std::string profile_id;
    // The profile's hardlock_cfg_material_default: consult the cfg paths below
    // when they exist. Their absence is not an error.
    bool use_profile_cfg = false;
    std::filesystem::path config_path;
    std::filesystem::path default_map_path;
    // Explicit options, which outrank cfg. Empty means not given.
    std::string handshake_response_hex;
    std::string descriptor_tail_hex;
    std::string transform_map_path;
    // An explicit request for the device boundary, such as a trace option.
    bool device_requested = false;
};

struct HardlockDeviceMaterial
{
    bool device_enabled = false;
    std::optional<HardlockHandshakeResponse> handshake_response;
    std::optional<std::uint16_t> descriptor_tail_word;
    std::optional<HardlockSeeds> seeds;
    HardlockTransformResponseMap transform_map;
    // The map file actually read, explicit or the profile default.
    std::filesystem::path transform_map_path;
    // Which values a profile default filled in from cfg, for logs that must
    // never carry the values themselves.
    bool cfg_handshake = false;
    bool cfg_tail = false;
    bool cfg_map = false;
};

// Combines the sources: explicit options first, then the profile's cfg
// section and default map when the profile allows them. A missing cfg file,
// section, or map is not an error; a malformed file or value is. Unparseable
// cfg seeds leave seeds unset rather than failing.
bool ResolveHardlockDeviceMaterial(const HardlockMaterialSources& sources,
                                   HardlockDeviceMaterial* material,
                                   std::string* error);

// The device options for an in-process boundary. A host that carries the
// material across a process instead packs and unpacks the same fields.
HardlockDeviceOptions MakeHardlockDeviceOptions(const HardlockDeviceMaterial& material);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_DEVICE_MATERIAL_H_
