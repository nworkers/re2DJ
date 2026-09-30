#ifndef RE2DJ_HLE_IMPORT_DISPATCHER_H_
#define RE2DJ_HLE_IMPORT_DISPATCHER_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/runtime/execution_backend.h"

namespace re2dj::hle
{

class GuestDeviceSet;
class GuestFiles;
class GuestProcess;
class HostAudio;
class HostPresentation;
class HostProcessLauncher;

// One reading of the host clock as the guest sees it.
struct GuestClockReading
{
    // FILETIME: 100-nanosecond intervals since 1601-01-01 UTC.
    std::uint64_t utc_file_time = 0;
    // Local time minus UTC, in minutes (+540 for Korea).
    std::int32_t local_offset_minutes = 0;
    // GetTickCount: milliseconds of a monotonic clock, wrapping at 2^32.
    std::uint32_t tick_ms = 0;
};

// A guest stdcall function the host calls before the current import returns,
// as Windows calls a window procedure from CreateWindowEx.
struct GuestCall
{
    std::uint32_t function = 0;
    std::vector<std::uint32_t> arguments;
    // Bytes placed in guest memory for the call, such as a CREATESTRUCT. The
    // argument at data_argument becomes their address, and the guest's changes
    // are copied back into data.
    std::vector<std::uint8_t> data;
    int data_argument = -1;
};

class ImportCallServices
{
public:
    virtual ~ImportCallServices() = default;

    virtual bool ReadGuestString(runtime::GuestAddress address,
                                 std::string* value,
                                 std::string* error) const = 0;
    virtual runtime::GuestAddress FindGuestModule(std::string_view name) const = 0;
    virtual runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                                  std::string_view name) const = 0;
    virtual runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                                  std::uint16_t ordinal) const = 0;

    // The services below are optional: a host that does not provide one keeps
    // these defaults, and a handler that needs it fails the call.

    // True when handle is the base of a module the guest can see.
    virtual bool IsGuestModule(runtime::GuestAddress handle) const
    {
        static_cast<void>(handle);
        return false;
    }
    // The file name of the module whose base is handle, such as "kernel32.dll".
    virtual std::string GuestModuleName(runtime::GuestAddress handle) const
    {
        static_cast<void>(handle);
        return {};
    }
    // Copies guest bytes, such as a DeviceIoControl buffer, in or out.
    virtual bool ReadGuestBytes(runtime::GuestAddress address,
                                std::span<std::uint8_t> bytes,
                                std::string* error) const
    {
        static_cast<void>(address);
        static_cast<void>(bytes);
        if (error != nullptr) *error = "guest memory reads are not provided";
        return false;
    }
    virtual bool WriteGuestBytes(runtime::GuestAddress address,
                                 std::span<const std::uint8_t> bytes,
                                 std::string* error) const
    {
        static_cast<void>(address);
        static_cast<void>(bytes);
        if (error != nullptr) *error = "guest memory writes are not provided";
        return false;
    }
    // The devices the guest may open in this run, or null for none.
    virtual GuestDeviceSet* Devices() const { return nullptr; }
    // The files the guest may open in this run, or null for none.
    virtual GuestFiles* Files() const { return nullptr; }
    // The guest's process state (ID, error mode, heap), or null for none.
    virtual GuestProcess* Process() const { return nullptr; }
    // The host clock; false when the host provides none.
    virtual bool ReadClock(GuestClockReading* reading) const
    {
        static_cast<void>(reading);
        return false;
    }
    // The calling thread's Win32 last-error value.
    virtual void SetLastError(std::uint32_t value) const { static_cast<void>(value); }
    virtual std::uint32_t LastError() const { return 0; }
    // The host's presentation, or null for a host that shows nothing.
    virtual HostPresentation* Presentation() const { return nullptr; }
    // The host's sound output, or null for a host that plays nothing.
    virtual HostAudio* Audio() const { return nullptr; }
    // Where CreateProcessA starts a child guest process, or null for a host
    // that starts none.
    virtual HostProcessLauncher* ProcessLauncher() const { return nullptr; }
    // Blocks the guest thread for about milliseconds of host time; false for
    // a host that cannot wait.
    virtual bool WaitMilliseconds(std::uint32_t milliseconds) const
    {
        static_cast<void>(milliseconds);
        return false;
    }
    // The calling guest thread's TEB, or 0 for a host that models none.
    virtual runtime::GuestAddress ThreadEnvironmentBlock() const { return runtime::GuestAddress(); }
    // The calling guest thread's ID; a host without threads has only the main one.
    virtual std::uint32_t CurrentThreadId() const { return 0x00000F04U; }
    // Starts a guest thread with this ID that calls start(parameter) as a
    // ThreadProc once the calling thread next waits or calls an import; when
    // it returns, the host records that through Process()->FinishThread.
    // False for a host without threads.
    virtual bool StartGuestThread(std::uint32_t start,
                                  std::uint32_t parameter,
                                  std::uint32_t thread_id,
                                  std::string* error) const
    {
        static_cast<void>(start);
        static_cast<void>(parameter);
        static_cast<void>(thread_id);
        if (error != nullptr) *error = "guest threads are not provided";
        return false;
    }
    // Runs a guest function to completion and gives its eax. The guest may
    // call imports meanwhile, which dispatch as nested calls.
    virtual bool CallGuest(GuestCall* call, std::uint32_t* result, std::string* error) const
    {
        static_cast<void>(call);
        static_cast<void>(result);
        if (error != nullptr) *error = "guest calls are not provided";
        return false;
    }
};

enum class CallingConvention : std::uint8_t
{
    kStdcall,
    kCdecl,
};

struct ImportCall
{
    const runtime::ImportGate& gate;
    std::span<const std::uint32_t> arguments;
    const ImportCallServices* services = nullptr;
    // Where the guest's call returns to, or 0 when the host does not say.
    std::uint32_t return_address = 0;
    // The guest address of the first argument on the guest stack, or 0 when
    // the host does not say. A variadic handler reads the arguments past its
    // declared ones from here.
    std::uint32_t arguments_address = 0;
};

struct ImportReturn
{
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    // The call does not return to the guest: the guest process ends with
    // exit_code, as kernel32!ExitProcess does. Each backend decides how.
    bool exit_process = false;
    std::uint32_t exit_code = 0;
};

using ImportHandler = bool (*)(const ImportCall& call,
                               ImportReturn* result,
                               std::string* error);

struct ImportBinding
{
    std::string module;
    std::string name;
    std::uint16_t ordinal = 0;
    bool by_ordinal = false;
    CallingConvention calling_convention = CallingConvention::kStdcall;
    std::uint32_t argument_count = 0;
    ImportHandler handler = nullptr;
};

class ImportDispatcher
{
public:
    static constexpr std::uint32_t kMaximumArgumentCount = 64;

    bool Register(ImportBinding binding, std::string* error);

    bool Dispatch(const runtime::ImportGate& gate,
                  const runtime::ExecutionEvent& event,
                  runtime::ExecutionBackend* backend,
                  std::string* error) const;

private:
    const ImportBinding* Find(const runtime::ImportGate& gate) const;

    std::vector<ImportBinding> bindings_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_IMPORT_DISPATCHER_H_
