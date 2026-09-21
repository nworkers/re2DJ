#include "native_in_process_runner.h"

#include "native_pe_session.h"

namespace re2dj::platform::linux
{
namespace
{

class ImportGateHandlerCleanup
{
public:
    ~ImportGateHandlerCleanup()
    {
        ClearNativeImportGateHandler();
    }
};

}  // namespace

bool RunNativePeInProcess(const std::vector<std::uint8_t>& file,
                          const exe::PeImageInfo& info,
                          std::uint32_t requested_base,
                          NativeImportGateHandler handler,
                          void* context,
                          NativeInProcessRunResult* result,
                          std::string* error)
{
    if (result == nullptr || error == nullptr || file.empty() || handler == nullptr)
    {
        if (error != nullptr) *error = "invalid native in-process runner arguments";
        return false;
    }
    *result = {};
    if (!ConfigureNativeImportGateHandler(handler, context))
    {
        *error = "cannot configure native in-process import handler";
        return false;
    }
    ImportGateHandlerCleanup handler_cleanup;
    NativePeSession session;
    if (!session.Prepare(file,
                         info,
                         requested_base,
                         NativeImportGateBridgeAddress(),
                         NativeImportGateCleanupAddress(),
                         error))
    {
        return false;
    }
    ConfigureNativeImportGateStackRange(session.bootstrap().GuestStackLimit(),
                                        session.bootstrap().GuestStackBase());
    if (!session.RunTlsCallbacks(&result->fault, error))
    {
        CaptureNativeFaultObservation(session.image(),
                                      session.bootstrap(),
                                      result->fault,
                                      &result->fault_observation);
        return false;
    }
    const bool completed = session.RunEntry(&result->exit_code, &result->fault, error);
    CaptureNativeFaultObservation(session.image(),
                                  session.bootstrap(),
                                  result->fault,
                                  &result->fault_observation);
    return completed;
}

}  // namespace re2dj::platform::linux
