#ifndef RE2DJ_TOOLS_WINDOWS_ORIGINAL_PROCESS_PROBE_IAT_VERIFIER_H_
#define RE2DJ_TOOLS_WINDOWS_ORIGINAL_PROCESS_PROBE_IAT_VERIFIER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"

namespace re2dj::tools::windows_original_process_probe
{

struct IatModuleCount
{
    std::string module;
    std::uint32_t slot_count = 0;
};

struct IatVerificationResult
{
    std::uint32_t slot_count = 0;
    std::vector<IatModuleCount> modules;
};

bool VerifySuspendedIat(HANDLE process,
                        std::uintptr_t image_base,
                        const exe::PeImageInfo& info,
                        const std::uint8_t* file,
                        std::size_t file_size,
                        IatVerificationResult* result,
                        std::string* error);

bool FindIatSlotByName(const exe::PeImageInfo& info,
                       const std::uint8_t* file,
                       std::size_t file_size,
                       const std::string& module,
                       const std::string& function,
                       std::uint32_t* slot_rva,
                       std::string* error);

bool FindIatSlotsByName(const exe::PeImageInfo& info,
                        const std::uint8_t* file,
                        std::size_t file_size,
                        const std::string& module,
                        const std::string& function,
                        std::vector<std::uint32_t>* slot_rvas,
                        std::string* error);

bool FindIatSlotByOrdinal(const exe::PeImageInfo& info,
                          const std::uint8_t* file,
                          std::size_t file_size,
                          const std::string& module,
                          std::uint16_t ordinal,
                          std::uint32_t* slot_rva,
                          std::string* error);

}  // namespace re2dj::tools::windows_original_process_probe

#endif  // RE2DJ_TOOLS_WINDOWS_ORIGINAL_PROCESS_PROBE_IAT_VERIFIER_H_
