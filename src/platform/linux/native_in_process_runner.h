#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_IN_PROCESS_RUNNER_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_IN_PROCESS_RUNNER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "native_fault_observation.h"
#include "native_import_bridge.h"
#include "native_process_bootstrap.h"
#include "re2dj/exe/pe_image.h"

namespace re2dj::platform::linux
{

struct NativeInProcessRunResult
{
    std::uint32_t exit_code = 0;
    NativeGuestFault fault;
    NativeFaultObservation fault_observation;
};

bool RunNativePeInProcess(const std::vector<std::uint8_t>& file,
                          const exe::PeImageInfo& info,
                          std::uint32_t requested_base,
                          NativeImportGateHandler handler,
                          void* context,
                          NativeInProcessRunResult* result,
                          std::string* error);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_IN_PROCESS_RUNNER_H_
