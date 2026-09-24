#include "native_in_process_runner.h"

#include "native_pe_session.h"
#include "re2dj/platform/linux/original_runner.h"

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

bool RunConfiguredNativePeInProcess(const std::vector<std::uint8_t>& file,
                                    const exe::PeImageInfo& info,
                                    std::uint32_t requested_base,
                                    NativeImportGateHandler handler,
                                    void* handler_context,
                                    NativePeSessionSetup setup,
                                    void* setup_context,
                                    NativeInProcessRunResult* result,
                                    std::string* error)
{
    if (result == nullptr || error == nullptr || file.empty() || handler == nullptr)
    {
        if (error != nullptr) *error = "invalid native in-process runner arguments";
        return false;
    }
    *result = {};
    if (!ConfigureNativeImportGateHandler(handler, handler_context))
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
    if (setup != nullptr && !setup(&session, setup_context, error))
    {
        return false;
    }
    ConfigureNativeImportGateStackRange(session.bootstrap().GuestStackLimit(),
                                        session.bootstrap().GuestStackBase());
    if (!session.RunTlsCallbacks(&result->fault, error))
    {
        result->seh_dispatch_count = session.bootstrap().SehDispatchCount();
        result->last_seh_handler = session.bootstrap().LastSehHandler();
        result->last_seh_resumed_eip = session.bootstrap().LastSehResumedEip();
        CaptureNativeFaultObservation(session.image(),
                                      session.bootstrap(),
                                      result->fault,
                                      &result->fault_observation);
        return false;
    }
    // A TLS callback that ends the process leaves the entry unrun.
    const bool completed =
        session.bootstrap().GuestProcessExited() ||
        session.RunEntry(&result->exit_code, &result->fault, error);
    result->process_exited = session.bootstrap().GuestProcessExited();
    if (result->process_exited)
    {
        result->exit_code = session.bootstrap().GuestExitCode();
    }
    result->seh_dispatch_count = session.bootstrap().SehDispatchCount();
    result->last_seh_handler = session.bootstrap().LastSehHandler();
    result->last_seh_resumed_eip = session.bootstrap().LastSehResumedEip();
    CaptureNativeFaultObservation(session.image(),
                                  session.bootstrap(),
                                  result->fault,
                                  &result->fault_observation);
    return completed;
}

bool RunNativePeInProcess(const std::vector<std::uint8_t>& file,
                          const exe::PeImageInfo& info,
                          std::uint32_t requested_base,
                          NativeImportGateHandler handler,
                          void* context,
                          NativeInProcessRunResult* result,
                          std::string* error)
{
    return RunConfiguredNativePeInProcess(file,
                                          info,
                                          requested_base,
                                          handler,
                                          context,
                                          nullptr,
                                          nullptr,
                                          result,
                                          error);
}

void CopyNativeFaultObservation(const NativeInProcessRunResult& source,
                                OriginalRunResult* destination)
{
    if (destination == nullptr)
    {
        return;
    }
    destination->seh_dispatch_count = source.seh_dispatch_count;
    destination->last_seh_handler = runtime::GuestAddress(source.last_seh_handler);
    destination->last_seh_resumed_eip = runtime::GuestAddress(source.last_seh_resumed_eip);
    if (source.fault.status_code == 0)
    {
        return;
    }
    OriginalFaultObservation& observation = destination->fault_observation;
    observation.observed = true;
    observation.fault_address = runtime::GuestAddress(source.fault.fault_address);
    observation.signal_code = source.fault.signal_code;
    observation.cpu_error_code = source.fault.cpu_error_code;
    observation.eax = source.fault.eax;
    observation.ebx = source.fault.ebx;
    observation.ecx = source.fault.ecx;
    observation.edx = source.fault.edx;
    observation.esi = source.fault.esi;
    observation.edi = source.fault.edi;
    observation.ebp = source.fault.ebp;
    observation.eflags = source.fault.eflags;
    observation.instruction_window_observed =
        source.fault_observation.instruction_window_observed;
    observation.instruction_window_address =
        runtime::GuestAddress(source.fault_observation.instruction_window_address);
    observation.instruction_window = source.fault_observation.instruction_window;
    observation.stack_words_observed = source.fault_observation.stack_words_observed;
    observation.stack_words = source.fault_observation.stack_words;
    observation.stack_word_count = source.fault_observation.stack_word_count;
    observation.seh_frame_observed = source.fault_observation.seh_frame_observed;
    observation.fs_base = runtime::GuestAddress(source.fault_observation.fs_base);
    observation.seh_frame_address = runtime::GuestAddress(source.fault_observation.seh_frame_address);
    observation.seh_next = source.fault_observation.seh_next;
    observation.seh_handler = runtime::GuestAddress(source.fault_observation.seh_handler);
    observation.seh_handler_window_observed =
        source.fault_observation.seh_handler_window_observed;
    observation.seh_handler_window = source.fault_observation.seh_handler_window;
}

}  // namespace re2dj::platform::linux
