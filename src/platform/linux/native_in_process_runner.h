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

class NativePeSession;
struct OriginalRunResult;

using NativePeSessionSetup = bool (*)(NativePeSession* session,
                                      void* context,
                                      std::string* error);

struct NativeInProcessRunResult
{
    std::uint32_t exit_code = 0;
    // The guest ended its own process through an import such as ExitProcess;
    // exit_code then holds that code rather than the entry return value.
    bool process_exited = false;
    NativeGuestFault fault;
    NativeFaultObservation fault_observation;
    std::uint32_t seh_dispatch_count = 0;
    std::uint32_t last_seh_handler = 0;
    std::uint32_t last_seh_resumed_eip = 0;
};

bool RunNativePeInProcess(const std::vector<std::uint8_t>& file,
                          const exe::PeImageInfo& info,
                          std::uint32_t requested_base,
                          NativeImportGateHandler handler,
                          void* context,
                          NativeInProcessRunResult* result,
                          std::string* error);

bool RunConfiguredNativePeInProcess(const std::vector<std::uint8_t>& file,
                                    const exe::PeImageInfo& info,
                                    std::uint32_t requested_base,
                                    NativeImportGateHandler handler,
                                    void* handler_context,
                                    NativePeSessionSetup setup,
                                    void* setup_context,
                                    NativeInProcessRunResult* result,
                                    std::string* error);

// Copies the fault context of an in-process run into the public result; a run
// without a fault leaves the result untouched.
void CopyNativeFaultObservation(const NativeInProcessRunResult& source,
                                OriginalRunResult* destination);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_IN_PROCESS_RUNNER_H_
