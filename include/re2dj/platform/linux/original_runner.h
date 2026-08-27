#ifndef RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_
#define RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_

#include <cstdint>
#include <filesystem>
#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/address_space.h"

namespace re2dj::platform::linux
{

enum class OriginalRunBoundary
{
    kImportGate,
    kProcessExit,
    kFault,
    kStopped,
};

struct OriginalRunResult
{
    OriginalRunBoundary boundary = OriginalRunBoundary::kStopped;
    runtime::GuestAddress load_base;
    runtime::GuestAddress entry_point;
    runtime::GuestAddress instruction_pointer;
    runtime::GuestAddress stack_pointer;
    runtime::GuestAddress gate_address;
    std::uint32_t status_code = 0;
    std::string module;
    std::string name;
    bool by_ordinal = false;
    std::uint16_t ordinal = 0;
};

bool RunOriginalUntilBoundary(const std::filesystem::path& executable_path,
                              const exe::PeImageInfo& image_info,
                              const std::filesystem::path& helper_path,
                              OriginalRunResult* result,
                              std::string* error);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_
