#include "re2dj/hle/modules/kernel32_module.h"

#include <algorithm>
#include <array>
#include <optional>
#include <limits>
#include <span>
#include <vector>
#include <string>
#include <utility>

#include "re2dj/hle/guest_devices.h"
#include "re2dj/hle/guest_files.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/win32_time.h"
#include "re2dj/hle/hardlock/device_call.h"
#include "re2dj/hle/modules/resolve_only_modules.h"

namespace re2dj::hle::modules
{
namespace
{

bool ReturnZero(const ImportCall&, ImportReturn* result, std::string* error)
{
    if (result == nullptr)
    {
        if (error != nullptr)
        {
            *error = "kernel32 result is null";
        }
        return false;
    }
    *result = {};
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

bool GetModuleHandleA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 GetModuleHandleA argument shape is invalid";
        }
        return false;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    if (call.arguments[0] == 0)
    {
        // NULL names the main image.
        GuestProcess* process = call.services->Process();
        result->eax = process == nullptr ? 0U : process->image_base();
        return true;
    }
    std::string name;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]),
                                        &name,
                                        error))
    {
        return false;
    }
    result->eax = call.services->FindGuestModule(name).value();
    return true;
}

bool GetProcAddress(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 2)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 GetProcAddress argument shape is invalid";
        }
        return false;
    }
    if (call.services == nullptr || call.arguments[0] == 0 || call.arguments[1] == 0)
    {
        return true;
    }

    const runtime::GuestAddress module(call.arguments[0]);
    if ((call.arguments[1] & 0xFFFF0000U) == 0)
    {
        result->eax = call.services
                          ->FindGuestExport(module,
                                            static_cast<std::uint16_t>(call.arguments[1]))
                          .value();
        return true;
    }

    std::string name;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]),
                                        &name,
                                        error))
    {
        return false;
    }
    result->eax = call.services->FindGuestExport(module, name).value();
    return true;
}

bool GetVersion(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error))
    {
        return false;
    }
    result->eax = kKernel32GuestVersion;
    return true;
}

// CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
// dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile). Device names the
// run provides open the device; other names open guest files (GuestFiles).
// A failed device open reports ERROR_INVALID_NAME for a "\\.\" name, the
// value 4th's "\\.\NTICE" open received on the Windows host. A file outside
// the guest root is not modelled and stops the call.
bool CreateFileA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kGenericRead = 0x80000000U;
    constexpr std::uint32_t kGenericWrite = 0x40000000U;
    constexpr std::uint32_t kFileAppendData = 0x00000004U;
    constexpr std::uint32_t kDelete = 0x00010000U;
    if (!ReturnZero(call, result, error) || call.arguments.size() != 7)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 CreateFileA argument shape is invalid";
        }
        return false;
    }
    result->eax = kKernel32InvalidHandle;
    if (call.services == nullptr)
    {
        return true;
    }
    std::string name;
    std::string read_error;
    const bool name_read =
        call.arguments[0] != 0 &&
        call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &name, &read_error);
    GuestDeviceSet* devices = call.services->Devices();
    if (name_read && devices != nullptr)
    {
        const std::uint32_t handle = devices->Open(name);
        if (handle != 0)
        {
            result->eax = handle;
            call.services->SetLastError(kWin32ErrorSuccess);
            return true;
        }
    }
    const bool device_name = name_read && name.rfind("\\\\.\\", 0) == 0;
    GuestFiles* files = call.services->Files();
    if (name_read && !device_name && files != nullptr && files->configured())
    {
        const std::uint32_t access = call.arguments[1];
        const GuestFiles::OpenResult opened =
            files->Open(name,
                        (access & kGenericRead) != 0,
                        (access & (kGenericWrite | kFileAppendData | kDelete)) != 0,
                        call.arguments[4]);
        if (opened.outside_root)
        {
            if (error != nullptr) *error = "kernel32 CreateFileA of a path outside the guest root is not modelled";
            return false;
        }
        if (opened.handle != 0)
        {
            result->eax = opened.handle;
        }
        call.services->SetLastError(opened.error);
        return true;
    }
    call.services->SetLastError(device_name ? kWin32ErrorInvalidName : kWin32ErrorFileNotFound);
    return true;
}

// DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize,
// lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped), synchronous
// only. Buffers at the same address and size share one host buffer, as the
// driver sees an in-place request on Windows.
bool DeviceIoControl(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 8)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 DeviceIoControl argument shape is invalid";
        }
        return false;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    const std::uint32_t handle = call.arguments[0];
    const std::uint32_t control_code = call.arguments[1];
    const std::uint32_t input_address = call.arguments[2];
    const std::uint32_t input_size = call.arguments[3];
    const std::uint32_t output_address = call.arguments[4];
    const std::uint32_t output_size = call.arguments[5];
    const std::uint32_t bytes_returned_address = call.arguments[6];
    GuestDeviceSet* devices = call.services->Devices();
    if (devices == nullptr || !devices->IsOpen(handle))
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    if (call.arguments[7] != 0 || input_size > kKernel32MaximumIoBufferSize ||
        output_size > kKernel32MaximumIoBufferSize ||
        (input_size != 0 && input_address == 0) || (output_size != 0 && output_address == 0))
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }

    std::string memory_error;
    std::vector<std::uint8_t> output(output_size);
    if (output_size != 0 &&
        !call.services->ReadGuestBytes(runtime::GuestAddress(output_address), output, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    const bool in_place = input_size != 0 && input_address == output_address &&
                          input_size == output_size;
    std::vector<std::uint8_t> input;
    if (!in_place && input_size != 0)
    {
        input.resize(input_size);
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(input_address), input, &memory_error))
        {
            call.services->SetLastError(kWin32ErrorNoAccess);
            return true;
        }
    }
    const std::span<const std::uint8_t> input_span =
        in_place ? std::span<const std::uint8_t>(output) : std::span<const std::uint8_t>(input);

    // Handlers have no clock; the request counts carry no timing here.
    const hardlock::HardlockDeviceCall device_call =
        devices->Control(handle, control_code, input_span, output, 0);
    if (!device_call.handled)
    {
        call.services->SetLastError(kWin32ErrorInvalidFunction);
        return true;
    }
    if (device_call.succeeded && output_size != 0 &&
        !call.services->WriteGuestBytes(runtime::GuestAddress(output_address), output, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    if (bytes_returned_address != 0)
    {
        std::array<std::uint8_t, 4> bytes = {};
        const std::uint32_t count = device_call.bytes_returned;
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            bytes[index] = static_cast<std::uint8_t>(count >> (index * 8));
        }
        if (!call.services->WriteGuestBytes(
                runtime::GuestAddress(bytes_returned_address), bytes, &memory_error))
        {
            call.services->SetLastError(kWin32ErrorNoAccess);
            return true;
        }
    }
    call.services->SetLastError(device_call.win32_error);
    result->eax = device_call.succeeded ? 1U : 0U;
    return true;
}

bool CloseHandle(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 CloseHandle argument shape is invalid";
        }
        return false;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    GuestDeviceSet* devices = call.services->Devices();
    GuestProcess* process = call.services->Process();
    if ((devices != nullptr && devices->Close(call.arguments[0])) ||
        (process != nullptr && (process->CloseProcessHandle(call.arguments[0]) ||
                                process->CloseEvent(call.arguments[0]))) ||
        (call.services->Files() != nullptr && call.services->Files()->Close(call.arguments[0])))
    {
        result->eax = 1;
        return true;
    }
    call.services->SetLastError(kWin32ErrorInvalidHandle);
    return true;
}

bool GetLastError(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error))
    {
        return false;
    }
    result->eax = call.services == nullptr ? 0 : call.services->LastError();
    return true;
}

bool SetLastError(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 SetLastError argument shape is invalid";
        }
        return false;
    }
    if (call.services != nullptr)
    {
        call.services->SetLastError(call.arguments[0]);
    }
    return true;
}

// ExitProcess(uExitCode) never returns: it asks the backend to end the guest
// process with the given code.
bool ExitProcess(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 ExitProcess argument shape is invalid";
        }
        return false;
    }
    result->exit_process = true;
    result->exit_code = call.arguments[0];
    return true;
}

// Writes a little-endian DWORD at offset into bytes.
void StoreDword(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
    }
}

bool CheckArgumentCount(const ImportCall& call,
                        ImportReturn* result,
                        std::size_t count,
                        const char* message,
                        std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != count)
    {
        if (error != nullptr && error->empty())
        {
            *error = message;
        }
        return false;
    }
    return true;
}

bool GetCurrentProcess(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetCurrentProcess argument shape is invalid", error))
    {
        return false;
    }
    result->eax = kKernel32CurrentProcess;
    return true;
}

bool GetCurrentProcessId(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetCurrentProcessId argument shape is invalid", error))
    {
        return false;
    }
    result->eax = GuestProcess::kProcessId;
    return true;
}

// The guest starts with an empty environment. Without HL_SEARCH the Hardlock
// API keeps its default search order.
bool GetEnvironmentVariableA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 GetEnvironmentVariableA argument shape is invalid", error))
    {
        return false;
    }
    if (call.services != nullptr)
    {
        call.services->SetLastError(kWin32ErrorEnvironmentVariableNotFound);
    }
    return true;
}

bool SetErrorMode(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 SetErrorMode argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        if (error != nullptr) *error = "kernel32 SetErrorMode needs the guest process";
        return false;
    }
    result->eax = process->ExchangeErrorMode(call.arguments[0]);
    return true;
}

// Facade modules are the only libraries; a name GetModuleHandleA would find
// "loads" as that module.
bool LoadLibraryA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 LoadLibraryA argument shape is invalid", error))
    {
        return false;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    std::string name;
    std::string read_error;
    if (call.arguments[0] != 0 &&
        call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &name, &read_error))
    {
        result->eax = call.services->FindGuestModule(name).value();
    }
    call.services->SetLastError(result->eax != 0 ? kWin32ErrorSuccess : kWin32ErrorModuleNotFound);
    return true;
}

// Facade modules stay mapped for the whole run, so freeing one only succeeds.
bool FreeLibrary(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 FreeLibrary argument shape is invalid", error))
    {
        return false;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    if (call.services->IsGuestModule(runtime::GuestAddress(call.arguments[0])))
    {
        result->eax = 1;
    }
    else
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
    }
    return true;
}

// GetVersionExA reports the same 6.2.9200 NT version as GetVersion, with no
// service pack and the workstation product type.
bool GetVersionExA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetVersionExA argument shape is invalid", error))
    {
        return false;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    const runtime::GuestAddress info(call.arguments[0]);
    std::array<std::uint8_t, 4> size_bytes = {};
    std::string memory_error;
    if (call.arguments[0] == 0 || !call.services->ReadGuestBytes(info, size_bytes, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    const std::uint32_t size = static_cast<std::uint32_t>(size_bytes[0]) |
                               (static_cast<std::uint32_t>(size_bytes[1]) << 8) |
                               (static_cast<std::uint32_t>(size_bytes[2]) << 16) |
                               (static_cast<std::uint32_t>(size_bytes[3]) << 24);
    if (size != kKernel32OsVersionInfoSize && size != kKernel32OsVersionInfoExSize)
    {
        call.services->SetLastError(kWin32ErrorInsufficientBuffer);
        return true;
    }
    // dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber,
    // dwPlatformId, then an empty szCSDVersion. The EX tail (service pack,
    // suite mask) stays zero except wProductType at offset 154.
    constexpr std::uint32_t kPlatformNt = 2;
    constexpr std::uint8_t kWorkstation = 1;
    std::vector<std::uint8_t> bytes(size, 0);
    StoreDword(bytes, 0, size);
    StoreDword(bytes, 4, kKernel32GuestVersion & 0xFFU);
    StoreDword(bytes, 8, (kKernel32GuestVersion >> 8) & 0xFFU);
    StoreDword(bytes, 12, kKernel32GuestVersion >> 16);
    StoreDword(bytes, 16, kPlatformNt);
    if (size == kKernel32OsVersionInfoExSize)
    {
        bytes[154] = kWorkstation;
    }
    if (!call.services->WriteGuestBytes(info, bytes, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    result->eax = 1;
    return true;
}

// Reports a GuestProcess memory outcome: FALSE with the Win32 error, or a
// handler failure for a request the model does not cover.
bool FinishMemoryCall(const ImportCall& call,
                      GuestMemoryResult outcome,
                      ImportReturn* result,
                      std::string* error)
{
    switch (outcome)
    {
    case GuestMemoryResult::kOk:
        call.services->SetLastError(kWin32ErrorSuccess);
        return true;
    case GuestMemoryResult::kInvalidParameter:
        result->eax = 0;
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    case GuestMemoryResult::kInvalidAddress:
        result->eax = 0;
        call.services->SetLastError(kWin32ErrorInvalidAddress);
        return true;
    case GuestMemoryResult::kUnsupported:
        break;
    }
    if (error != nullptr)
    {
        *error = "kernel32 " + call.gate.name + " request is outside the modelled memory API";
    }
    return false;
}

GuestProcess* RequireProcess(const ImportCall& call, std::string* error)
{
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr && error != nullptr)
    {
        *error = "kernel32 " + call.gate.name + " needs the guest process";
    }
    return process;
}

// OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId): only the guest's
// own process exists, and every access right is granted for it.
bool OpenProcess(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 OpenProcess argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    result->eax = process->OpenProcess(call.arguments[2]);
    call.services->SetLastError(result->eax != 0 ? kWin32ErrorSuccess : kWin32ErrorInvalidParameter);
    return true;
}

// VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect). Newly
// committed pages read as zero, as on Windows.
bool VirtualAlloc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 4, "kernel32 VirtualAlloc argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    std::uint32_t allocated = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
    const GuestMemoryResult outcome = process->VirtualAlloc(
        call.arguments[0], call.arguments[1], call.arguments[2], call.arguments[3], &allocated, &committed);
    if (outcome == GuestMemoryResult::kOk)
    {
        std::string memory_error;
        for (const auto& [address, size] : committed)
        {
            const std::vector<std::uint8_t> zeros(size, 0);
            if (!call.services->WriteGuestBytes(runtime::GuestAddress(address), zeros, &memory_error))
            {
                if (error != nullptr)
                {
                    *error = "kernel32 VirtualAlloc cannot zero committed pages: " + memory_error;
                }
                return false;
            }
        }
        result->eax = allocated;
    }
    return FinishMemoryCall(call, outcome, result, error);
}

// VirtualFree(lpAddress, dwSize, dwFreeType).
bool VirtualFree(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 VirtualFree argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const GuestMemoryResult outcome =
        process->VirtualFree(call.arguments[0], call.arguments[1], call.arguments[2]);
    result->eax = outcome == GuestMemoryResult::kOk ? 1U : 0U;
    return FinishMemoryCall(call, outcome, result, error);
}

// VirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect). The new
// protection is recorded and reported, not applied to host memory.
bool VirtualProtect(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 4, "kernel32 VirtualProtect argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t old_slot = call.arguments[3];
    std::array<std::uint8_t, 4> probe = {};
    std::string memory_error;
    if (old_slot == 0 ||
        !call.services->ReadGuestBytes(runtime::GuestAddress(old_slot), probe, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    std::uint32_t old_protect = 0;
    const GuestMemoryResult outcome = process->VirtualProtect(
        call.arguments[0], call.arguments[1], call.arguments[2], &old_protect);
    if (outcome == GuestMemoryResult::kOk)
    {
        StoreDword(probe, 0, old_protect);
        if (!call.services->WriteGuestBytes(runtime::GuestAddress(old_slot), probe, &memory_error))
        {
            call.services->SetLastError(kWin32ErrorNoAccess);
            return true;
        }
        result->eax = 1;
    }
    return FinishMemoryCall(call, outcome, result, error);
}

// LocalAlloc(uFlags, uBytes) for fixed blocks from the guest heap.
bool LocalAlloc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kLmemZeroInit = 0x40U;
    if (!CheckArgumentCount(call, result, 2, "kernel32 LocalAlloc argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t flags = call.arguments[0];
    const std::uint32_t size = call.arguments[1];
    if ((flags & ~kLmemZeroInit) != 0)
    {
        if (error != nullptr) *error = "kernel32 LocalAlloc supports only LMEM_FIXED and LMEM_ZEROINIT";
        return false;
    }
    const std::uint32_t block = process->Allocate(size);
    if (block == 0)
    {
        if (error != nullptr) *error = "kernel32 LocalAlloc found no guest heap room";
        return false;
    }
    if ((flags & kLmemZeroInit) != 0 && size != 0)
    {
        const std::vector<std::uint8_t> zeros(size, 0);
        std::string memory_error;
        if (!call.services->WriteGuestBytes(runtime::GuestAddress(block), zeros, &memory_error))
        {
            process->Free(block);
            if (error != nullptr) *error = "kernel32 LocalAlloc cannot zero its block: " + memory_error;
            return false;
        }
    }
    result->eax = block;
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

// LocalFree(hMem): NULL on success or for NULL; an unknown block comes back
// with ERROR_INVALID_HANDLE.
bool LocalFree(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 LocalFree argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[0] != 0 && !process->Free(call.arguments[0]))
    {
        result->eax = call.arguments[0];
        call.services->SetLastError(kWin32ErrorInvalidHandle);
    }
    return true;
}

// ReadProcessMemory/WriteProcessMemory(hProcess, lpBaseAddress, lpBuffer,
// nSize, lpNumberOfBytes) on the guest's own process. Windows' kernel32 lifts a
// read-only page's protection for the write, so only committed, accessible
// pages matter; anything else copies nothing and reports ERROR_PARTIAL_COPY.
bool CopyProcessMemory(const ImportCall& call, bool write, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call,
                            result,
                            5,
                            write ? "kernel32 WriteProcessMemory argument shape is invalid"
                                  : "kernel32 ReadProcessMemory argument shape is invalid",
                            error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t target = call.arguments[1];
    const std::uint32_t buffer = call.arguments[2];
    const std::uint32_t size = call.arguments[3];
    const std::uint32_t count_slot = call.arguments[4];
    if (!process->IsProcessHandle(call.arguments[0]))
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    if (size > kKernel32MaximumIoBufferSize)
    {
        if (error != nullptr) *error = "kernel32 process memory copy exceeds the modelled size";
        return false;
    }
    std::vector<std::uint8_t> bytes(size);
    std::string memory_error;
    const std::uint32_t source = write ? buffer : target;
    const std::uint32_t destination = write ? target : buffer;
    const bool copied =
        (size == 0 || process->Accessible(target, size)) &&
        call.services->ReadGuestBytes(runtime::GuestAddress(source), bytes, &memory_error) &&
        call.services->WriteGuestBytes(runtime::GuestAddress(destination), bytes, &memory_error);
    if (count_slot != 0)
    {
        std::array<std::uint8_t, 4> count = {};
        StoreDword(count, 0, copied ? size : 0U);
        call.services->WriteGuestBytes(runtime::GuestAddress(count_slot), count, &memory_error);
    }
    if (!copied)
    {
        call.services->SetLastError(kWin32ErrorPartialCopy);
        return true;
    }
    result->eax = 1;
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

bool ReadProcessMemory(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return CopyProcessMemory(call, false, result, error);
}

bool WriteProcessMemory(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return CopyProcessMemory(call, true, result, error);
}

// Win32 heap flags (winnt.h) the heap exports read.
constexpr std::uint32_t kHeapNoSerialize = 0x00000001U;
constexpr std::uint32_t kHeapGenerateExceptions = 0x00000004U;
constexpr std::uint32_t kHeapZeroMemory = 0x00000008U;
constexpr std::uint32_t kHeapReallocInPlaceOnly = 0x00000010U;

// Serialization is moot for one guest thread; raising exceptions on failure
// is not modelled, so a caller asking for it stops instead.
bool CheckHeapFlags(const ImportCall& call, std::uint32_t flags, std::uint32_t allowed, std::string* error)
{
    if ((flags & ~(allowed | kHeapNoSerialize)) != 0)
    {
        if (error != nullptr)
        {
            *error = "kernel32 " + call.gate.name + " flags are outside the modelled heap";
        }
        return false;
    }
    return true;
}

bool ZeroGuestBytes(const ImportCall& call, std::uint32_t address, std::uint32_t size, std::string* error)
{
    if (size == 0)
    {
        return true;
    }
    const std::vector<std::uint8_t> zeros(size, 0);
    std::string memory_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(address), zeros, &memory_error))
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " cannot zero a block: " + memory_error;
        return false;
    }
    return true;
}

// HeapCreate(flOptions, dwInitialSize, dwMaximumSize).
bool HeapCreate(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 HeapCreate argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr || !CheckHeapFlags(call, call.arguments[0], 0x00040000U, error))
    {
        return false;
    }
    result->eax = process->CreateHeap(call.arguments[2]);
    call.services->SetLastError(result->eax != 0 ? kWin32ErrorSuccess : kWin32ErrorNotEnoughMemory);
    return true;
}

// HeapDestroy(hHeap).
bool HeapDestroy(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 HeapDestroy argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    result->eax = process->DestroyHeap(call.arguments[0]) ? 1U : 0U;
    if (result->eax == 0)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
    }
    return true;
}

// HeapAlloc(hHeap, dwFlags, dwBytes).
bool HeapAlloc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 HeapAlloc argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr || !CheckHeapFlags(call, call.arguments[1], kHeapZeroMemory, error))
    {
        return false;
    }
    GuestHeap* heap = process->FindHeap(call.arguments[0]);
    if (heap == nullptr)
    {
        if (error != nullptr) *error = "kernel32 HeapAlloc names no heap";
        return false;
    }
    const std::uint32_t block = heap->Allocate(call.arguments[2]);
    if (block != 0 && (call.arguments[1] & kHeapZeroMemory) != 0 &&
        !ZeroGuestBytes(call, block, call.arguments[2], error))
    {
        return false;
    }
    result->eax = block;
    return true;
}

// HeapFree(hHeap, dwFlags, lpMem): freeing NULL succeeds.
bool HeapFree(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 HeapFree argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr || !CheckHeapFlags(call, call.arguments[1], 0, error))
    {
        return false;
    }
    GuestHeap* heap = process->FindHeap(call.arguments[0]);
    const bool freed = call.arguments[2] == 0 || (heap != nullptr && heap->Free(call.arguments[2]));
    result->eax = freed ? 1U : 0U;
    if (!freed)
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
    }
    return true;
}

// HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes): grows in place when it can,
// otherwise moves the block unless HEAP_REALLOC_IN_PLACE_ONLY forbids it.
bool HeapReAlloc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 4, "kernel32 HeapReAlloc argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    const std::uint32_t flags = call.arguments[1];
    if (process == nullptr ||
        !CheckHeapFlags(call, flags, kHeapZeroMemory | kHeapReallocInPlaceOnly, error))
    {
        return false;
    }
    GuestHeap* heap = process->FindHeap(call.arguments[0]);
    const std::uint32_t block = call.arguments[2];
    const std::uint32_t size = call.arguments[3];
    const std::optional<std::uint32_t> old_size =
        heap == nullptr ? std::nullopt : heap->BlockSize(block);
    if (!old_size.has_value())
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    std::uint32_t target = 0;
    if (heap->ResizeInPlace(block, size))
    {
        target = block;
    }
    else if ((flags & kHeapReallocInPlaceOnly) == 0)
    {
        target = heap->Allocate(size);
        if (target != 0)
        {
            std::vector<std::uint8_t> bytes(std::min(*old_size, size));
            std::string memory_error;
            if (!call.services->ReadGuestBytes(runtime::GuestAddress(block), bytes, &memory_error) ||
                !call.services->WriteGuestBytes(runtime::GuestAddress(target), bytes, &memory_error))
            {
                if (error != nullptr) *error = "kernel32 HeapReAlloc cannot move a block: " + memory_error;
                return false;
            }
            heap->Free(block);
        }
    }
    if (target == 0)
    {
        call.services->SetLastError(kWin32ErrorNotEnoughMemory);
        return true;
    }
    if ((flags & kHeapZeroMemory) != 0 && size > *old_size &&
        !ZeroGuestBytes(call, target + *old_size, size - *old_size, error))
    {
        return false;
    }
    result->eax = target;
    return true;
}

// HeapSize(hHeap, dwFlags, lpMem): the requested size, or (SIZE_T)-1.
bool HeapSize(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 HeapSize argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr || !CheckHeapFlags(call, call.arguments[1], 0, error))
    {
        return false;
    }
    GuestHeap* heap = process->FindHeap(call.arguments[0]);
    const std::optional<std::uint32_t> size =
        heap == nullptr ? std::nullopt : heap->BlockSize(call.arguments[2]);
    result->eax = size.value_or(0xFFFFFFFFU);
    return true;
}

// The system code page of a Korean Windows, the machine the original ran on.
// Conversions cover CP949's single bytes: ASCII, 0x80 and 0xFF, whose
// characters were measured with MultiByteToWideChar(949) on a Korean
// Windows 11 host. Double-byte text stops the call rather than guess the
// table.
constexpr std::uint32_t kGuestAnsiCodePage = 949;
constexpr std::uint32_t kCp949Byte80Character = 0x0080;
constexpr std::uint32_t kCp949ByteFfCharacter = 0xF8F7;

// The UTF-16 unit of a CP949 single byte, or nothing for a lead byte.
std::optional<std::uint32_t> Cp949SingleByteToUnit(std::uint32_t byte)
{
    if (byte < 0x80)
    {
        return byte;
    }
    if (byte == 0x80)
    {
        return kCp949Byte80Character;
    }
    if (byte == 0xFF)
    {
        return kCp949ByteFfCharacter;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> Cp949UnitToSingleByte(std::uint32_t unit)
{
    if (unit < 0x80)
    {
        return unit;
    }
    if (unit == kCp949Byte80Character)
    {
        return 0x80U;
    }
    if (unit == kCp949ByteFfCharacter)
    {
        return 0xFFU;
    }
    return std::nullopt;
}
constexpr std::uint32_t kCodePageAnsi = 0;
constexpr std::uint32_t kCodePageOem = 1;

bool IsGuestCodePage(std::uint32_t code_page)
{
    return code_page == kCodePageAnsi || code_page == kCodePageOem ||
           code_page == kGuestAnsiCodePage;
}

// Writes bytes to guest memory or fails the handler with the gate's name.
bool PutGuestBytes(const ImportCall& call,
                   std::uint32_t address,
                   std::span<const std::uint8_t> bytes,
                   std::string* error)
{
    std::string memory_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(address), bytes, &memory_error))
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + ": " + memory_error;
        return false;
    }
    return true;
}

// Places bytes in a new process-heap block, returning its address or 0.
std::uint32_t PlaceOnProcessHeap(const ImportCall& call,
                                 GuestProcess* process,
                                 std::span<const std::uint8_t> bytes,
                                 std::string* error)
{
    const std::uint32_t block = process->Allocate(static_cast<std::uint32_t>(bytes.size()));
    if (block == 0)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " found no process heap room";
        return 0;
    }
    return PutGuestBytes(call, block, bytes, error) ? block : 0;
}

// GetStartupInfoA(lpStartupInfo): as CreateProcess with a zeroed
// STARTUPINFO leaves it, cb aside.
bool GetStartupInfoA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetStartupInfoA argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    constexpr std::uint32_t kStartupInfoSize = 68;
    std::array<std::uint8_t, kStartupInfoSize> info = {};
    StoreDword(info, 0, kStartupInfoSize);
    return PutGuestBytes(call, call.arguments[0], info, error);
}

// GetStdHandle(nStdHandle): a GUI process has no console, so the three
// standard handles are NULL.
bool GetStdHandle(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetStdHandle argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t which = call.arguments[0];
    if (which != 0xFFFFFFF6U && which != 0xFFFFFFF5U && which != 0xFFFFFFF4U)
    {
        result->eax = kKernel32InvalidHandle;
        if (call.services != nullptr) call.services->SetLastError(kWin32ErrorInvalidHandle);
    }
    return true;
}

// GetFileType(hFile): only devices are open handles, and none is a file.
bool GetFileType(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetFileType argument shape is invalid", error))
    {
        return false;
    }
    GuestDeviceSet* devices = call.services == nullptr ? nullptr : call.services->Devices();
    if (devices != nullptr && devices->IsOpen(call.arguments[0]))
    {
        if (error != nullptr) *error = "kernel32 GetFileType of a device is not modelled";
        return false;
    }
    if (call.services != nullptr) call.services->SetLastError(kWin32ErrorInvalidHandle);
    return true;
}

// SetHandleCount(uNumber) is a no-op on Win32 and returns its argument.
bool SetHandleCount(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 SetHandleCount argument shape is invalid", error))
    {
        return false;
    }
    result->eax = call.arguments[0];
    return true;
}

bool GetCurrentThreadId(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetCurrentThreadId argument shape is invalid", error))
    {
        return false;
    }
    result->eax = GuestProcess::kThreadId;
    return true;
}

// GetCommandLineA(): the quoted module path, placed once on the process heap.
bool GetCommandLineA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetCommandLineA argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    if (process->command_line() == 0)
    {
        const std::string text = "\"" + process->module_path() + "\"";
        std::vector<std::uint8_t> bytes(text.begin(), text.end());
        bytes.push_back(0);
        const std::uint32_t block = PlaceOnProcessHeap(call, process, bytes, error);
        if (block == 0)
        {
            return false;
        }
        process->set_command_line(block);
    }
    result->eax = process->command_line();
    return true;
}

// GetEnvironmentStrings(A/W): the guest environment is empty, an empty block
// being just its terminator. Each call returns a new block, freed by the
// matching FreeEnvironmentStrings.
bool GetEnvironmentBlock(const ImportCall& call, std::size_t unit, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetEnvironmentStrings argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::vector<std::uint8_t> terminator(unit * 2, 0);
    result->eax = PlaceOnProcessHeap(call, process, terminator, error);
    return result->eax != 0;
}

bool GetEnvironmentStringsA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return GetEnvironmentBlock(call, 1, result, error);
}

bool GetEnvironmentStringsW(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return GetEnvironmentBlock(call, 2, result, error);
}

bool FreeEnvironmentStrings(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 FreeEnvironmentStrings argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    result->eax = process->Free(call.arguments[0]) ? 1U : 0U;
    return true;
}

bool GetACP(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetACP argument shape is invalid", error))
    {
        return false;
    }
    result->eax = kGuestAnsiCodePage;
    return true;
}

bool GetOEMCP(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetOEMCP argument shape is invalid", error))
    {
        return false;
    }
    result->eax = kGuestAnsiCodePage;
    return true;
}

// GetCPInfo(CodePage, lpCPInfo): CP949 is double-byte with lead bytes
// 0x81-0xFE and '?' as the default character.
bool GetCPInfo(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 GetCPInfo argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    if (!IsGuestCodePage(call.arguments[0]))
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    // MaxCharSize, DefaultChar[2], LeadByte[12].
    std::array<std::uint8_t, 20> info = {};
    StoreDword(info, 0, 2);
    info[4] = '?';
    info[6] = 0x81;
    info[7] = 0xFE;
    if (!PutGuestBytes(call, call.arguments[1], info, error))
    {
        return false;
    }
    result->eax = 1;
    return true;
}

// Reads a count-or-terminated guest string of unit-sized characters; a count
// of -1 includes the terminator, as the conversion functions define it.
bool ReadGuestUnits(const ImportCall& call,
                    std::uint32_t address,
                    std::uint32_t count,
                    std::size_t unit,
                    std::vector<std::uint32_t>* units,
                    std::string* error)
{
    constexpr std::uint32_t kMaximumUnits = 64U * 1024U;
    units->clear();
    std::string memory_error;
    for (std::uint32_t index = 0; count == 0xFFFFFFFFU || index < count; ++index)
    {
        if (index >= kMaximumUnits)
        {
            if (error != nullptr) *error = "kernel32 " + call.gate.name + " string is too long";
            return false;
        }
        std::array<std::uint8_t, 2> bytes = {};
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(address + index * unit),
                                           std::span<std::uint8_t>(bytes.data(), unit),
                                           &memory_error))
        {
            if (error != nullptr) *error = "kernel32 " + call.gate.name + ": " + memory_error;
            return false;
        }
        const std::uint32_t value = unit == 1 ? bytes[0] : (bytes[0] | (bytes[1] << 8));
        units->push_back(value);
        if (count == 0xFFFFFFFFU && value == 0)
        {
            break;
        }
    }
    return true;
}

// MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte,
// lpWideCharStr, cchWideChar) and WideCharToMultiByte(CodePage, dwFlags,
// lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpDefaultChar,
// lpUsedDefaultChar) for CP949's single-byte characters.
bool ConvertText(const ImportCall& call, bool to_wide, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, to_wide ? 6 : 8,
                            to_wide ? "kernel32 MultiByteToWideChar argument shape is invalid"
                                    : "kernel32 WideCharToMultiByte argument shape is invalid",
                            error) ||
        call.services == nullptr)
    {
        return false;
    }
    if (!IsGuestCodePage(call.arguments[0]))
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    std::vector<std::uint32_t> units;
    if (!ReadGuestUnits(call, call.arguments[2], call.arguments[3], to_wide ? 1 : 2, &units, error))
    {
        return false;
    }
    for (std::uint32_t& value : units)
    {
        const std::optional<std::uint32_t> converted =
            to_wide ? Cp949SingleByteToUnit(value) : Cp949UnitToSingleByte(value);
        if (!converted.has_value())
        {
            if (error != nullptr) *error = "kernel32 " + call.gate.name + " of double-byte text is not modelled";
            return false;
        }
        value = *converted;
    }
    const std::uint32_t output = call.arguments[4];
    const std::uint32_t capacity = call.arguments[5];
    const auto count = static_cast<std::uint32_t>(units.size());
    if (capacity == 0)
    {
        result->eax = count;
        return true;
    }
    if (capacity < count)
    {
        call.services->SetLastError(kWin32ErrorInsufficientBuffer);
        return true;
    }
    const std::size_t unit = to_wide ? 2 : 1;
    std::vector<std::uint8_t> bytes(units.size() * unit, 0);
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        bytes[index * unit] = static_cast<std::uint8_t>(units[index]);
        if (unit == 2)
        {
            bytes[index * unit + 1] = static_cast<std::uint8_t>(units[index] >> 8);
        }
    }
    if (!PutGuestBytes(call, output, bytes, error))
    {
        return false;
    }
    if (!to_wide && call.arguments[7] != 0)
    {
        std::array<std::uint8_t, 4> used = {};
        if (!PutGuestBytes(call, call.arguments[7], used, error))
        {
            return false;
        }
    }
    result->eax = count;
    return true;
}

bool MultiByteToWideChar(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ConvertText(call, true, result, error);
}

bool WideCharToMultiByte(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ConvertText(call, false, result, error);
}

// GetModuleFileNameA(hModule, lpFilename, nSize): the main image's guest
// path for NULL or its base, "C:\WINDOWS\system32\<dll>" for a facade module.
// A short buffer receives a truncated, terminated copy and
// ERROR_INSUFFICIENT_BUFFER, as on Windows XP and later.
// lstrcpynA(lpString1, lpString2, iMaxLength): copies at most iMaxLength - 1
// bytes and a NUL, returning lpString1. Measured on Windows 11: 0 writes
// nothing, the length is unsigned (-1 copies the whole string), a NULL source
// returns NULL, and the last error never changes.
bool LstrcpynA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 lstrcpynA argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t destination = call.arguments[0];
    const std::uint32_t maximum = call.arguments[2];
    if (call.arguments[1] == 0)
    {
        return true;
    }
    result->eax = destination;
    if (maximum == 0)
    {
        return true;
    }
    std::string text;
    std::string read_error;
    // An empty string reads as a failure with no error text.
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]), &text, &read_error) &&
        !read_error.empty())
    {
        if (error != nullptr) *error = "kernel32 lstrcpynA cannot read the source: " + read_error;
        return false;
    }
    const std::size_t copied = std::min<std::size_t>(text.size(), maximum - 1U);
    std::vector<std::uint8_t> bytes(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(copied));
    bytes.push_back(0);
    return PutGuestBytes(call, destination, bytes, error);
}

bool GetModuleFileNameA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 GetModuleFileNameA argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t module = call.arguments[0];
    std::string path;
    if (module == 0 || module == process->image_base())
    {
        path = process->module_path();
    }
    else if (call.services->IsGuestModule(runtime::GuestAddress(module)))
    {
        path = "C:\\WINDOWS\\system32\\" + call.services->GuestModuleName(runtime::GuestAddress(module));
    }
    else
    {
        call.services->SetLastError(kWin32ErrorModuleNotFound);
        return true;
    }
    const std::uint32_t size = call.arguments[2];
    if (size == 0)
    {
        call.services->SetLastError(kWin32ErrorInsufficientBuffer);
        return true;
    }
    const std::size_t copied = std::min<std::size_t>(path.size(), size - 1);
    std::vector<std::uint8_t> bytes(path.begin(), path.begin() + static_cast<std::ptrdiff_t>(copied));
    bytes.push_back(0);
    if (!PutGuestBytes(call, call.arguments[1], bytes, error))
    {
        return false;
    }
    if (copied < path.size())
    {
        result->eax = size;
        call.services->SetLastError(kWin32ErrorInsufficientBuffer);
    }
    else
    {
        result->eax = static_cast<std::uint32_t>(copied);
        call.services->SetLastError(kWin32ErrorSuccess);
    }
    return true;
}

// The C-locale class bits (winnls.h C1_*) of an ASCII character.
std::uint16_t AsciiCharClass(std::uint32_t value)
{
    constexpr std::uint16_t kUpper = 0x0001;
    constexpr std::uint16_t kLower = 0x0002;
    constexpr std::uint16_t kDigit = 0x0004;
    constexpr std::uint16_t kSpace = 0x0008;
    constexpr std::uint16_t kPunct = 0x0010;
    constexpr std::uint16_t kControl = 0x0020;
    constexpr std::uint16_t kBlank = 0x0040;
    constexpr std::uint16_t kHexDigit = 0x0080;
    constexpr std::uint16_t kAlpha = 0x0100;
    if (value == '\t')
    {
        return kControl | kSpace | kBlank;
    }
    if (value >= '\n' && value <= '\r')
    {
        return kControl | kSpace;
    }
    if (value < 0x20 || value == 0x7F)
    {
        return kControl;
    }
    if (value == ' ')
    {
        return kSpace | kBlank;
    }
    if (value >= '0' && value <= '9')
    {
        return kDigit | kHexDigit;
    }
    if (value >= 'A' && value <= 'Z')
    {
        return kUpper | kAlpha | (value <= 'F' ? kHexDigit : 0);
    }
    if (value >= 'a' && value <= 'z')
    {
        return kLower | kAlpha | (value <= 'f' ? kHexDigit : 0);
    }
    return kPunct;
}

// CT_CTYPE1 bits (winnls.h C1_*) of an ASCII character, as GetStringTypeW
// reported them on a Korean Windows 11 host: the C-locale classes plus
// C1_DEFINED.
std::uint16_t AsciiCharType1(std::uint32_t value)
{
    constexpr std::uint16_t kDefined = 0x0200;
    return kDefined | AsciiCharClass(value);
}

// GetStringTypeW(dwInfoType, lpSrcStr, cchSrc, lpCharType) for CT_CTYPE1 over
// the characters of CP949's single bytes, as measured on Windows (U+0080
// C1_CNTRL|C1_DEFINED, U+F8F7 C1_DEFINED); other types and characters stop
// the call.
bool GetStringTypeW(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kCharType1 = 1;
    if (!CheckArgumentCount(call, result, 4, "kernel32 GetStringTypeW argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    if (call.arguments[0] != kCharType1)
    {
        if (error != nullptr) *error = "kernel32 GetStringTypeW supports CT_CTYPE1 only";
        return false;
    }
    std::vector<std::uint32_t> units;
    if (!ReadGuestUnits(call, call.arguments[1], call.arguments[2], 2, &units, error))
    {
        return false;
    }
    std::vector<std::uint8_t> types(units.size() * 2, 0);
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        std::uint16_t type = 0;
        if (units[index] <= 0x7F)
        {
            type = AsciiCharType1(units[index]);
        }
        else if (units[index] == kCp949Byte80Character)
        {
            type = 0x0220;
        }
        else if (units[index] == kCp949ByteFfCharacter)
        {
            type = 0x0200;
        }
        else
        {
            if (error != nullptr) *error = "kernel32 GetStringTypeW of this character is not modelled";
            return false;
        }
        types[index * 2] = static_cast<std::uint8_t>(type);
        types[index * 2 + 1] = static_cast<std::uint8_t>(type >> 8);
    }
    if (!PutGuestBytes(call, call.arguments[3], types, error))
    {
        return false;
    }
    result->eax = 1;
    return true;
}

// LCMapStringW(Locale, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest) and
// LCMapStringA with LCMAP_LOWERCASE or LCMAP_UPPERCASE over CP949's
// single-byte characters. Measured on a Korean Windows 11 host: only the ASCII
// letters change, U+0080 and U+F8F7 map to themselves, and cchDest 0 returns
// the length. Other flags and characters stop the call.
bool MapStringCase(const ImportCall& call, bool wide, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kLowercase = 0x00000100U;
    constexpr std::uint32_t kUppercase = 0x00000200U;
    if (!CheckArgumentCount(call, result, 6,
                            wide ? "kernel32 LCMapStringW argument shape is invalid"
                                 : "kernel32 LCMapStringA argument shape is invalid",
                            error) ||
        call.services == nullptr)
    {
        return false;
    }
    const std::uint32_t flags = call.arguments[1];
    if (flags != kLowercase && flags != kUppercase)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " supports only case mapping";
        return false;
    }
    std::vector<std::uint32_t> units;
    const std::size_t unit = wide ? 2 : 1;
    if (!ReadGuestUnits(call, call.arguments[2], call.arguments[3], unit, &units, error))
    {
        return false;
    }
    for (std::uint32_t& value : units)
    {
        const bool known = wide ? (value <= 0x7F || value == kCp949Byte80Character ||
                                   value == kCp949ByteFfCharacter)
                                : Cp949SingleByteToUnit(value).has_value();
        if (!known)
        {
            if (error != nullptr) *error = "kernel32 " + call.gate.name + " of this character is not modelled";
            return false;
        }
        if (flags == kLowercase && value >= 'A' && value <= 'Z')
        {
            value += 'a' - 'A';
        }
        else if (flags == kUppercase && value >= 'a' && value <= 'z')
        {
            value -= 'a' - 'A';
        }
    }
    const auto count = static_cast<std::uint32_t>(units.size());
    const std::uint32_t capacity = call.arguments[5];
    if (capacity == 0)
    {
        result->eax = count;
        return true;
    }
    if (capacity < count)
    {
        call.services->SetLastError(kWin32ErrorInsufficientBuffer);
        return true;
    }
    std::vector<std::uint8_t> bytes(units.size() * unit, 0);
    for (std::size_t index = 0; index < units.size(); ++index)
    {
        bytes[index * unit] = static_cast<std::uint8_t>(units[index]);
        if (wide)
        {
            bytes[index * unit + 1] = static_cast<std::uint8_t>(units[index] >> 8);
        }
    }
    if (!PutGuestBytes(call, call.arguments[4], bytes, error))
    {
        return false;
    }
    result->eax = count;
    return true;
}

bool LCMapStringW(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MapStringCase(call, true, result, error);
}

bool LCMapStringA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MapStringCase(call, false, result, error);
}

// IsProcessorFeaturePresent(ProcessorFeature) for the features whose answer
// is the same on every x86 host this runs on (winnt.h PF_*): no FDIV erratum
// or FPU emulation, and CMPXCHG8B, MMX, RDTSC, SSE, and SSE2 present. Any
// other feature stops the call rather than answer for an unknown host.
bool IsProcessorFeaturePresent(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 IsProcessorFeaturePresent argument shape is invalid", error))
    {
        return false;
    }
    switch (call.arguments[0])
    {
    case 0:  // PF_FLOATING_POINT_PRECISION_ERRATA
    case 1:  // PF_FLOATING_POINT_EMULATED
        result->eax = 0;
        return true;
    case 2:   // PF_COMPARE_EXCHANGE_DOUBLE
    case 3:   // PF_MMX_INSTRUCTIONS_AVAILABLE
    case 6:   // PF_XMMI_INSTRUCTIONS_AVAILABLE
    case 8:   // PF_RDTSC_INSTRUCTION_AVAILABLE
    case 10:  // PF_XMMI64_INSTRUCTIONS_AVAILABLE
        result->eax = 1;
        return true;
    default:
        if (error != nullptr) *error = "kernel32 IsProcessorFeaturePresent feature is not modelled";
        return false;
    }
}

// SetUnhandledExceptionFilter(lpTopLevelExceptionFilter).
bool SetUnhandledExceptionFilter(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 SetUnhandledExceptionFilter argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    result->eax = process->ExchangeUnhandledExceptionFilter(call.arguments[0]);
    return true;
}

// CreateEventA(lpEventAttributes, bManualReset, bInitialState, lpName) for
// unnamed events; a name would have to be shared across processes, which is
// not modelled.
bool CreateEventA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 4, "kernel32 CreateEventA argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[3] != 0)
    {
        if (error != nullptr) *error = "kernel32 CreateEventA of a named event is not modelled";
        return false;
    }
    result->eax = process->CreateEvent(call.arguments[1] != 0, call.arguments[2] != 0);
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

// SetEvent(hEvent) and ResetEvent(hEvent).
bool ChangeEvent(const ImportCall& call, bool signaled, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 SetEvent/ResetEvent argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestEvent* event = process->FindEvent(call.arguments[0]);
    if (event == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    event->signaled = signaled;
    result->eax = 1;
    return true;
}

bool SetEvent(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ChangeEvent(call, true, result, error);
}

bool ResetEvent(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return ChangeEvent(call, false, result, error);
}

// WaitForSingleObject(hHandle, dwMilliseconds) on an event. A signalled event
// returns WAIT_OBJECT_0 (an auto-reset one resets); an unsignalled one with a
// zero timeout returns WAIT_TIMEOUT. Only this guest thread exists, so a wait
// that would block can never be released: it stops the call.
bool WaitForSingleObject(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kWaitObject0 = 0x00000000U;
    constexpr std::uint32_t kWaitTimeout = 0x00000102U;
    constexpr std::uint32_t kWaitFailed = 0xFFFFFFFFU;
    if (!CheckArgumentCount(call, result, 2, "kernel32 WaitForSingleObject argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestEvent* event = process->FindEvent(call.arguments[0]);
    if (event == nullptr)
    {
        result->eax = kWaitFailed;
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    if (event->signaled)
    {
        if (!event->manual_reset)
        {
            event->signaled = false;
        }
        result->eax = kWaitObject0;
        return true;
    }
    if (call.arguments[1] == 0)
    {
        result->eax = kWaitTimeout;
        return true;
    }
    if (error != nullptr) *error = "kernel32 WaitForSingleObject would block the only guest thread";
    return false;
}

bool RequireClock(const ImportCall& call, GuestClockReading* reading, std::string* error)
{
    if (call.services == nullptr || !call.services->ReadClock(reading))
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " needs the host clock";
        return false;
    }
    return true;
}

// GetSystemTime(lpSystemTime) and GetLocalTime(lpSystemTime): the host clock
// in UTC, or shifted by the host's local offset.
bool WriteClockTime(const ImportCall& call, bool local, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetSystemTime/GetLocalTime argument shape is invalid", error))
    {
        return false;
    }
    GuestClockReading reading;
    if (!RequireClock(call, &reading, error))
    {
        return false;
    }
    std::int64_t file_time = static_cast<std::int64_t>(reading.utc_file_time);
    if (local)
    {
        file_time += static_cast<std::int64_t>(reading.local_offset_minutes) * 60 *
                     static_cast<std::int64_t>(kFileTimeTicksPerSecond);
    }
    return PutGuestBytes(call,
                         call.arguments[0],
                         EncodeSystemTime(FileTimeToSystemTime(static_cast<std::uint64_t>(file_time))),
                         error);
}

bool GetSystemTime(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return WriteClockTime(call, false, result, error);
}

bool GetLocalTime(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return WriteClockTime(call, true, result, error);
}

// SystemTimeToFileTime(lpSystemTime, lpFileTime).
bool SystemTimeToFileTime(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 SystemTimeToFileTime argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    std::array<std::uint8_t, 16> bytes = {};
    std::string memory_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[0]), bytes, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    const std::optional<std::uint64_t> file_time = hle::SystemTimeToFileTime(DecodeSystemTime(bytes));
    if (!file_time.has_value())
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    std::array<std::uint8_t, 8> out = {};
    StoreDword(out, 0, static_cast<std::uint32_t>(*file_time));
    StoreDword(out, 4, static_cast<std::uint32_t>(*file_time >> 32));
    if (!PutGuestBytes(call, call.arguments[1], out, error))
    {
        return false;
    }
    result->eax = 1;
    return true;
}

bool GetTickCount(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetTickCount argument shape is invalid", error))
    {
        return false;
    }
    GuestClockReading reading;
    if (!RequireClock(call, &reading, error))
    {
        return false;
    }
    result->eax = reading.tick_ms;
    return true;
}

// GetTimeZoneInformation(lpTimeZoneInformation): the host's current offset
// as the standard bias, with no daylight rule (TIME_ZONE_ID_UNKNOWN) and
// empty zone names. A Korean Windows names its zone in Hangul, which the
// guest would convert as double-byte CP949, not modelled yet.
bool GetTimeZoneInformation(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kTimeZoneInformationSize = 172;
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetTimeZoneInformation argument shape is invalid", error))
    {
        return false;
    }
    GuestClockReading reading;
    if (!RequireClock(call, &reading, error))
    {
        return false;
    }
    // Bias is UTC minus local time, in minutes; everything after it is zero.
    std::array<std::uint8_t, kTimeZoneInformationSize> information = {};
    StoreDword(information, 0, static_cast<std::uint32_t>(-reading.local_offset_minutes));
    if (!PutGuestBytes(call, call.arguments[0], information, error))
    {
        return false;
    }
    result->eax = 0;  // TIME_ZONE_ID_UNKNOWN
    return true;
}

// ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead,
// lpOverlapped) on a guest file, synchronously; overlapped reads stop.
bool ReadFile(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 5, "kernel32 ReadFile argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    if (call.arguments[4] != 0)
    {
        if (error != nullptr) *error = "kernel32 ReadFile with OVERLAPPED is not modelled";
        return false;
    }
    GuestFiles* files = call.services->Files();
    std::vector<std::uint8_t> bytes;
    const std::uint32_t outcome = files == nullptr ? kWin32ErrorInvalidHandle
                                                   : files->Read(call.arguments[0], call.arguments[2], &bytes);
    if (outcome == kWin32ErrorSuccess && !bytes.empty() && !PutGuestBytes(call, call.arguments[1], bytes, error))
    {
        return false;
    }
    if (call.arguments[3] != 0)
    {
        std::array<std::uint8_t, 4> count = {};
        StoreDword(count, 0, static_cast<std::uint32_t>(bytes.size()));
        if (!PutGuestBytes(call, call.arguments[3], count, error))
        {
            return false;
        }
    }
    result->eax = outcome == kWin32ErrorSuccess ? 1U : 0U;
    call.services->SetLastError(outcome);
    return true;
}

// WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten,
// lpOverlapped) on a guest file opened for writing.
bool WriteFile(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 5, "kernel32 WriteFile argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    if (call.arguments[4] != 0)
    {
        if (error != nullptr) *error = "kernel32 WriteFile with OVERLAPPED is not modelled";
        return false;
    }
    if (call.arguments[2] > kKernel32MaximumIoBufferSize * 16U)
    {
        if (error != nullptr) *error = "kernel32 WriteFile size exceeds the modelled bound";
        return false;
    }
    std::vector<std::uint8_t> bytes(call.arguments[2]);
    std::string memory_error;
    if (!bytes.empty() &&
        !call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[1]), bytes, &memory_error))
    {
        call.services->SetLastError(kWin32ErrorNoAccess);
        return true;
    }
    GuestFiles* files = call.services->Files();
    const std::uint32_t outcome = files == nullptr ? kWin32ErrorInvalidHandle : files->Write(call.arguments[0], bytes);
    if (call.arguments[3] != 0)
    {
        std::array<std::uint8_t, 4> count = {};
        StoreDword(count, 0, outcome == kWin32ErrorSuccess ? call.arguments[2] : 0U);
        if (!PutGuestBytes(call, call.arguments[3], count, error))
        {
            return false;
        }
    }
    result->eax = outcome == kWin32ErrorSuccess ? 1U : 0U;
    call.services->SetLastError(outcome);
    return true;
}

// SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod):
// the new position's low DWORD, the high one through the pointer, or
// INVALID_SET_FILE_POINTER with the error.
bool SetFilePointer(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kInvalidSetFilePointer = 0xFFFFFFFFU;
    if (!CheckArgumentCount(call, result, 4, "kernel32 SetFilePointer argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    std::int64_t distance = static_cast<std::int32_t>(call.arguments[1]);
    const std::uint32_t high_slot = call.arguments[2];
    std::array<std::uint8_t, 4> high = {};
    std::string memory_error;
    if (high_slot != 0)
    {
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(high_slot), high, &memory_error))
        {
            result->eax = kInvalidSetFilePointer;
            call.services->SetLastError(kWin32ErrorNoAccess);
            return true;
        }
        const std::uint32_t high_value = static_cast<std::uint32_t>(high[0]) | (static_cast<std::uint32_t>(high[1]) << 8) |
                                         (static_cast<std::uint32_t>(high[2]) << 16) | (static_cast<std::uint32_t>(high[3]) << 24);
        distance = static_cast<std::int64_t>((static_cast<std::uint64_t>(high_value) << 32) | call.arguments[1]);
    }
    GuestFiles* files = call.services->Files();
    std::uint64_t position = 0;
    const std::uint32_t outcome = files == nullptr ? kWin32ErrorInvalidHandle
                                                   : files->Seek(call.arguments[0], distance, call.arguments[3], &position);
    if (outcome != kWin32ErrorSuccess)
    {
        result->eax = kInvalidSetFilePointer;
        call.services->SetLastError(outcome);
        return true;
    }
    if (high_slot != 0)
    {
        StoreDword(high, 0, static_cast<std::uint32_t>(position >> 32));
        if (!PutGuestBytes(call, high_slot, high, error))
        {
            return false;
        }
    }
    result->eax = static_cast<std::uint32_t>(position);
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

// GetFileSize(hFile, lpFileSizeHigh): the low DWORD, the high one through the
// pointer, or INVALID_FILE_SIZE with the error.
bool GetFileSize(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 GetFileSize argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    GuestFiles* files = call.services->Files();
    std::uint64_t size = 0;
    const std::uint32_t outcome = files == nullptr ? kWin32ErrorInvalidHandle : files->Size(call.arguments[0], &size);
    if (outcome != kWin32ErrorSuccess)
    {
        result->eax = 0xFFFFFFFFU;
        call.services->SetLastError(outcome);
        return true;
    }
    if (call.arguments[1] != 0)
    {
        std::array<std::uint8_t, 4> high = {};
        StoreDword(high, 0, static_cast<std::uint32_t>(size >> 32));
        if (!PutGuestBytes(call, call.arguments[1], high, error))
        {
            return false;
        }
    }
    result->eax = static_cast<std::uint32_t>(size);
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

// Exports the protection resolves, most while rebuilding the original
// program's import table, without calling them yet (kernel32 Win32
// signatures; RtlUnwind forwards to ntdll on Windows).
constexpr ResolveOnlyExport kKernel32ResolveOnly[] = {
    {"Sleep", 1}, {"CreateThread", 6},
    {"FindFirstFileA", 2},
    {"SetCurrentDirectoryA", 1}, {"GetWindowsDirectoryA", 2}, {"DeleteFileA", 1},
    {"GlobalMemoryStatus", 1}, {"CreateProcessA", 10},
    {"GetThreadPriority", 1}, {"SetThreadPriority", 2}, {"TerminateThread", 2},
    {"GetCurrentDirectoryA", 2},
    {"FindNextFileA", 2}, {"FindClose", 1},
    {"SetEndOfFile", 1},
    {"SetConsoleCtrlHandler", 2},
    {"RaiseException", 4},
   
    {"TerminateProcess", 2},
    {"IsBadWritePtr", 2},
    {"IsBadReadPtr", 2},
   
    {"UnhandledExceptionFilter", 1}, {"IsBadCodePtr", 1}, {"GetStringTypeA", 5},
    {"GetFileAttributesA", 1},
    {"RtlUnwind", 4},
    {"lstrcmpA", 2}, {"lstrlenA", 1}, {"SetEnvironmentVariableA", 2},
    {"CompareStringW", 6}, {"CompareStringA", 6}, {"FlushFileBuffers", 1},
    {"SetStdHandle", 2},
};

GuestExportDescriptor MakeExport(std::string name,
                                 std::uint32_t argument_count,
                                 ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

}  // namespace

GuestModuleDescriptor MakeKernel32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "kernel32.dll";
    descriptor.aliases = {"kernel32"};
    descriptor.exports.push_back(MakeExport("GetModuleHandleA", 1, &GetModuleHandleA));
    descriptor.exports.push_back(MakeExport("GetProcAddress", 2, &GetProcAddress));
    descriptor.exports.push_back(MakeExport("GetVersion", 0, &GetVersion));
    descriptor.exports.push_back(MakeExport("CreateFileA", 7, &CreateFileA));
    descriptor.exports.push_back(MakeExport("ExitProcess", 1, &ExitProcess));
    descriptor.exports.push_back(MakeExport("DeviceIoControl", 8, &DeviceIoControl));
    descriptor.exports.push_back(MakeExport("CloseHandle", 1, &CloseHandle));
    descriptor.exports.push_back(MakeExport("GetLastError", 0, &GetLastError));
    descriptor.exports.push_back(MakeExport("SetLastError", 1, &SetLastError));
    descriptor.exports.push_back(MakeExport("GetCurrentProcess", 0, &GetCurrentProcess));
    descriptor.exports.push_back(MakeExport("GetCurrentProcessId", 0, &GetCurrentProcessId));
    descriptor.exports.push_back(
        MakeExport("GetEnvironmentVariableA", 3, &GetEnvironmentVariableA));
    descriptor.exports.push_back(MakeExport("SetErrorMode", 1, &SetErrorMode));
    descriptor.exports.push_back(MakeExport("LoadLibraryA", 1, &LoadLibraryA));
    descriptor.exports.push_back(MakeExport("FreeLibrary", 1, &FreeLibrary));
    descriptor.exports.push_back(MakeExport("GetVersionExA", 1, &GetVersionExA));
    descriptor.exports.push_back(MakeExport("OpenProcess", 3, &OpenProcess));
    descriptor.exports.push_back(MakeExport("VirtualAlloc", 4, &VirtualAlloc));
    descriptor.exports.push_back(MakeExport("VirtualFree", 3, &VirtualFree));
    descriptor.exports.push_back(MakeExport("VirtualProtect", 4, &VirtualProtect));
    descriptor.exports.push_back(MakeExport("LocalAlloc", 2, &LocalAlloc));
    descriptor.exports.push_back(MakeExport("LocalFree", 1, &LocalFree));
    descriptor.exports.push_back(MakeExport("ReadProcessMemory", 5, &ReadProcessMemory));
    descriptor.exports.push_back(MakeExport("WriteProcessMemory", 5, &WriteProcessMemory));
    descriptor.exports.push_back(MakeExport("HeapCreate", 3, &HeapCreate));
    descriptor.exports.push_back(MakeExport("HeapDestroy", 1, &HeapDestroy));
    descriptor.exports.push_back(MakeExport("HeapAlloc", 3, &HeapAlloc));
    descriptor.exports.push_back(MakeExport("HeapFree", 3, &HeapFree));
    descriptor.exports.push_back(MakeExport("HeapReAlloc", 4, &HeapReAlloc));
    descriptor.exports.push_back(MakeExport("HeapSize", 3, &HeapSize));
    descriptor.exports.push_back(MakeExport("GetStartupInfoA", 1, &GetStartupInfoA));
    descriptor.exports.push_back(MakeExport("GetStdHandle", 1, &GetStdHandle));
    descriptor.exports.push_back(MakeExport("GetFileType", 1, &GetFileType));
    descriptor.exports.push_back(MakeExport("SetHandleCount", 1, &SetHandleCount));
    descriptor.exports.push_back(MakeExport("GetCurrentThreadId", 0, &GetCurrentThreadId));
    descriptor.exports.push_back(MakeExport("GetCommandLineA", 0, &GetCommandLineA));
    descriptor.exports.push_back(MakeExport("GetEnvironmentStrings", 0, &GetEnvironmentStringsA));
    descriptor.exports.push_back(MakeExport("GetEnvironmentStringsW", 0, &GetEnvironmentStringsW));
    descriptor.exports.push_back(MakeExport("FreeEnvironmentStringsA", 1, &FreeEnvironmentStrings));
    descriptor.exports.push_back(MakeExport("FreeEnvironmentStringsW", 1, &FreeEnvironmentStrings));
    descriptor.exports.push_back(MakeExport("GetACP", 0, &GetACP));
    descriptor.exports.push_back(MakeExport("GetOEMCP", 0, &GetOEMCP));
    descriptor.exports.push_back(MakeExport("GetCPInfo", 2, &GetCPInfo));
    descriptor.exports.push_back(MakeExport("MultiByteToWideChar", 6, &MultiByteToWideChar));
    descriptor.exports.push_back(MakeExport("WideCharToMultiByte", 8, &WideCharToMultiByte));
    descriptor.exports.push_back(MakeExport("GetModuleFileNameA", 3, &GetModuleFileNameA));
    descriptor.exports.push_back(MakeExport("lstrcpynA", 3, &LstrcpynA));
    descriptor.exports.push_back(MakeExport("GetStringTypeW", 4, &GetStringTypeW));
    descriptor.exports.push_back(MakeExport("LCMapStringW", 6, &LCMapStringW));
    descriptor.exports.push_back(MakeExport("LCMapStringA", 6, &LCMapStringA));
    descriptor.exports.push_back(
        MakeExport("IsProcessorFeaturePresent", 1, &IsProcessorFeaturePresent));
    descriptor.exports.push_back(
        MakeExport("SetUnhandledExceptionFilter", 1, &SetUnhandledExceptionFilter));
    descriptor.exports.push_back(MakeExport("CreateEventA", 4, &CreateEventA));
    descriptor.exports.push_back(MakeExport("SetEvent", 1, &SetEvent));
    descriptor.exports.push_back(MakeExport("ResetEvent", 1, &ResetEvent));
    descriptor.exports.push_back(MakeExport("WaitForSingleObject", 2, &WaitForSingleObject));
    descriptor.exports.push_back(MakeExport("GetTickCount", 0, &GetTickCount));
    descriptor.exports.push_back(MakeExport("GetSystemTime", 1, &GetSystemTime));
    descriptor.exports.push_back(MakeExport("GetLocalTime", 1, &GetLocalTime));
    descriptor.exports.push_back(MakeExport("SystemTimeToFileTime", 2, &SystemTimeToFileTime));
    descriptor.exports.push_back(
        MakeExport("GetTimeZoneInformation", 1, &GetTimeZoneInformation));
    descriptor.exports.push_back(MakeExport("ReadFile", 5, &ReadFile));
    descriptor.exports.push_back(MakeExport("WriteFile", 5, &WriteFile));
    descriptor.exports.push_back(MakeExport("SetFilePointer", 4, &SetFilePointer));
    descriptor.exports.push_back(MakeExport("GetFileSize", 2, &GetFileSize));
    AddResolveOnlyExports(&descriptor, kKernel32ResolveOnly);
    // The protection probes for DOS extenders (Phar Lap TNT, Borland 32-bit)
    // with these names; no Windows kernel32 exports them.
    descriptor.absent_exports = {"IsTNT", "Borland32"};
    return descriptor;
}

}  // namespace re2dj::hle::modules
