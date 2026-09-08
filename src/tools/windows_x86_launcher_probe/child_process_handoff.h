#ifndef RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_CHILD_PROCESS_HANDOFF_H_
#define RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_CHILD_PROCESS_HANDOFF_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hle/hardlock/handshake_response.h"
#include "re2dj/hle/hardlock/transform_responses.h"

namespace re2dj::tools::windows_x86_launcher_probe
{

struct BootstrapChildHandoffOptions
{
    std::filesystem::path runtime_path;
    std::filesystem::path vfs_source_root;
    std::filesystem::path overlay_root;
    std::filesystem::path chd_path;
    std::filesystem::path vfs_trace_path;
    std::filesystem::path graphics_trace_path;
    // The name the guest uses for its own root, from the profile's guest drive
    // letter and directory. Empty leaves the runtime's own default in place.
    std::string guest_root;
    std::string profile_id;
    std::string device_path_prefix;
    bool dynamic_vfs_resolver = false;
    bool device_mock_lptdi = false;
    bool device_mock_wts_console_session = false;
    bool hardlock_device = false;
    bool hardlock_transform_input_trace = false;
    std::filesystem::path hardlock_transform_input_dump_path;
    bool message_box = false;
    bool hle_d3d3 = false;
    bool fullscreen = false;
    bool hle_directsound = false;
    bool hardlock_handshake_enabled = false;
    re2dj::hle::hardlock::HardlockHandshakeResponse hardlock_handshake = {};
    bool hardlock_tail_enabled = false;
    std::uint16_t hardlock_tail = 0;
    std::vector<re2dj::hle::hardlock::HardlockTransformResponseEntry>
        hardlock_transform_map;
};

struct BootstrapChildHandoffResult
{
    HANDLE process = nullptr;
    HANDLE primary_thread = nullptr;
    DWORD process_id = 0;
    DWORD primary_thread_id = 0;
    std::uintptr_t image_base = 0;
    std::uint32_t runtime_base = 0;
    re2dj::exe::PeImageInfo image_info = {};
    std::vector<std::uint8_t> image_file;
};

bool PrepareBootstrapChildProcess(const DEBUG_EVENT& create_event,
                                  const std::filesystem::path& executable,
                                  const BootstrapChildHandoffOptions& options,
                                  BootstrapChildHandoffResult* result,
                                  std::string* error);

}  // namespace re2dj::tools::windows_x86_launcher_probe

#endif  // RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_CHILD_PROCESS_HANDOFF_H_
