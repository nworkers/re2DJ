#include "../native_process_bootstrap.h"

#include "../native_guest_threads.h"
#include "native_compat_mode.h"

namespace re2dj::platform::linux
{

struct NativeProcessBootstrap::Impl
{
    NativeCompatModeRuntime runtime;
    bool initialized = false;
    bool process_exited = false;
    std::uint32_t exit_code = 0;

    bool Run(NativeCompatModeCall call,
             std::uint32_t* result,
             NativeGuestFault* fault,
             std::string* error)
    {
        if (!initialized || process_exited || fault == nullptr || error == nullptr)
        {
            if (error != nullptr) *error = "invalid guest execution arguments";
            return false;
        }
        NativeCompatModeRunResult run;
        if (!runtime.Run(call, &run, fault, error))
        {
            return false;
        }
        if (run.process_exited)
        {
            process_exited = true;
            exit_code = run.exit_code;
        }
        if (result != nullptr)
        {
            *result = run.process_exited ? run.exit_code : run.eax;
        }
        return true;
    }
};

NativeProcessBootstrap::NativeProcessBootstrap() : impl_(new Impl) {}
NativeProcessBootstrap::~NativeProcessBootstrap() { delete impl_; }

bool NativeProcessBootstrap::Initialize(std::uint32_t image_base, std::string* error)
{
    if (error == nullptr || impl_->initialized)
    {
        if (error != nullptr) *error = "invalid native process bootstrap state";
        return false;
    }
    if (!impl_->runtime.Initialize(image_base, NativeCompatModeOptions{}, error))
    {
        return false;
    }
    impl_->initialized = true;
    return true;
}

bool NativeProcessBootstrap::RunTlsCallback(std::uint32_t callback,
                                            std::uint32_t image_base,
                                            NativeGuestFault* fault,
                                            std::string* error)
{
    // PIMAGE_TLS_CALLBACK(DllHandle, DLL_PROCESS_ATTACH, Reserved), as the
    // i386 CallGuestTls pushes it.
    NativeCompatModeCall call;
    call.entry = callback;
    call.arguments = {image_base, 1, 0};
    return impl_->Run(call, nullptr, fault, error);
}

bool NativeProcessBootstrap::RunEntry(std::uint32_t entry,
                                      std::uint32_t* result,
                                      NativeGuestFault* fault,
                                      std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr) *error = "guest entry result is required";
        return false;
    }
    NativeCompatModeCall call;
    call.entry = entry;
    return impl_->Run(call, result, fault, error);
}

std::uint32_t NativeProcessBootstrap::GuestStackBase() const
{
    return impl_->runtime.GuestStackBase();
}

std::uint32_t NativeProcessBootstrap::GuestStackLimit() const
{
    return impl_->runtime.GuestStackLimit();
}

bool NativeProcessBootstrap::IsGuestStackRange(std::uint32_t address, std::uint32_t size) const
{
    const std::uint32_t limit = GuestStackLimit();
    const std::uint32_t base = GuestStackBase();
    return (base != 0 && address >= limit && address <= base && size <= base - address) ||
           NativeGuestThreadMemoryContains(address, size);
}

std::uint32_t NativeProcessBootstrap::Teb() const { return impl_->runtime.Teb(); }

std::uint32_t NativeProcessBootstrap::SehDispatchCount() const
{
    return impl_->runtime.SehDispatchCount();
}

std::uint32_t NativeProcessBootstrap::LastSehHandler() const
{
    return impl_->runtime.LastSehHandler();
}

std::uint32_t NativeProcessBootstrap::LastSehResumedEip() const
{
    return impl_->runtime.LastSehResumedEip();
}

NativeGuestExceptionCounters NativeProcessBootstrap::ExceptionCounters() const
{
    return impl_->runtime.ExceptionCounters();
}

bool NativeProcessBootstrap::GuestProcessExited() const
{
    return impl_->process_exited;
}

std::uint32_t NativeProcessBootstrap::GuestExitCode() const
{
    return impl_->exit_code;
}

}  // namespace re2dj::platform::linux
