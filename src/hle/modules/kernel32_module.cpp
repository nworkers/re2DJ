#include "re2dj/hle/modules/kernel32_module.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <limits>
#include <span>
#include <vector>
#include <string>
#include <utility>

#include "re2dj/hle/guest_devices.h"
#include "re2dj/hle/guest_files.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/private_profile.h"
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
    // GetCurrentProcess's pseudo-handle closes successfully, as measured on
    // Windows 11 (task 428).
    if (call.arguments[0] == GuestProcess::kCurrentProcessHandle)
    {
        result->eax = 1;
        return true;
    }
    if (call.services == nullptr)
    {
        return true;
    }
    GuestDeviceSet* devices = call.services->Devices();
    GuestProcess* process = call.services->Process();
    if ((devices != nullptr && devices->Close(call.arguments[0])) ||
        (process != nullptr && (process->CloseProcessHandle(call.arguments[0]) ||
                                process->CloseChildHandle(call.arguments[0]) ||
                                process->CloseEvent(call.arguments[0]) ||
                                process->CloseThreadHandle(call.arguments[0]))) ||
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
constexpr std::uint32_t kHeapZeroMemory = 0x00000008U;
constexpr std::uint32_t kHeapReallocInPlaceOnly = 0x00000010U;

// Serialization is moot while one guest thread runs at a time; raising exceptions on failure
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

// HeapValidate(hHeap, dwFlags, lpMem), as measured on Windows 11: TRUE for a
// NULL lpMem (the whole heap, which the model keeps consistent) or the start
// of a live block of that heap; FALSE for a pointer inside a block, a freed
// block, or another heap's block. The flags are not checked and the last
// error is left alone. A handle that names no heap raises an access
// violation there, which is not modelled.
bool HeapValidate(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 3, "kernel32 HeapValidate argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const GuestHeap* heap = process->FindHeap(call.arguments[0]);
    if (heap == nullptr)
    {
        if (error != nullptr) *error = "kernel32 HeapValidate of a handle that names no heap is not modelled";
        return false;
    }
    const std::uint32_t memory = call.arguments[2];
    result->eax = memory == 0 || heap->BlockSize(memory).has_value() ? 1U : 0U;
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

// GetCurrentDirectoryA(nBufferLength, lpBuffer), as measured on Windows 11:
// a buffer longer than the path receives it and its length is returned; a
// shorter one receives nothing and the length it needs, terminator included,
// is returned. The last error never changes. A NULL buffer that claims room
// faults on Windows, which is not modelled.
bool GetCurrentDirectoryA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 2)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 GetCurrentDirectoryA argument shape is invalid";
        }
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 GetCurrentDirectoryA needs the guest file model";
        return false;
    }
    const std::string path = files->CurrentDirectory();
    const auto length = static_cast<std::uint32_t>(path.size());
    const std::uint32_t size = call.arguments[0];
    const std::uint32_t buffer = call.arguments[1];
    if (size <= length)
    {
        result->eax = length + 1;
        return true;
    }
    if (buffer == 0)
    {
        if (error != nullptr) *error = "kernel32 GetCurrentDirectoryA into a NULL buffer faults on Windows";
        return false;
    }
    std::vector<std::uint8_t> bytes(path.begin(), path.end());
    bytes.push_back(0);
    if (!PutGuestBytes(call, buffer, bytes, error))
    {
        return false;
    }
    result->eax = length;
    return true;
}

// SetCurrentDirectoryA(lpPathName) under GuestFiles' rules, as measured on
// Windows 11: TRUE leaves the last error alone; a NULL path is FALSE with
// ERROR_INVALID_PARAMETER. A directory outside the guest root is not
// modelled and stops the call.
bool SetCurrentDirectoryA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 SetCurrentDirectoryA argument shape is invalid";
        }
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 SetCurrentDirectoryA needs the guest file model";
        return false;
    }
    if (call.arguments[0] == 0)
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    std::string path;
    std::string read_error;
    // An empty string reads as a failure with no error text.
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &path, &read_error) &&
        !read_error.empty())
    {
        if (error != nullptr) *error = "kernel32 SetCurrentDirectoryA cannot read the path: " + read_error;
        return false;
    }
    bool outside_root = false;
    const std::uint32_t failure = files->SetCurrentDirectory(path, &outside_root);
    if (outside_root)
    {
        if (error != nullptr) *error = "kernel32 SetCurrentDirectoryA of a directory outside the guest root is not modelled: " + path;
        return false;
    }
    if (failure != kWin32ErrorSuccess)
    {
        call.services->SetLastError(failure);
        return true;
    }
    result->eax = 1;
    return true;
}

// A profile file's text for the GetPrivateProfile* handlers, which read it
// through GuestFiles relative to the current directory. kOpenFailed carries
// the open's Win32 error; a bare name (which Windows looks up in its own
// directory), a file outside the guest root, and a read failure stop.
enum class ProfileFile
{
    kRead,
    kOpenFailed,
    kStop,
};

ProfileFile ReadProfileFile(const ImportCall& call,
                            GuestFiles* files,
                            const std::string& file_name,
                            std::string* text,
                            std::uint32_t* open_error,
                            std::string* error)
{
    if (file_name.find_first_of("\\/") == std::string::npos)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " of a file in the Windows directory is not modelled: " + file_name;
        return ProfileFile::kStop;
    }
    const GuestFiles::OpenResult opened = files->Open(file_name, true, false, kOpenExisting);
    if (opened.outside_root)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " of a file outside the guest root is not modelled: " + file_name;
        return ProfileFile::kStop;
    }
    if (opened.handle == 0)
    {
        *open_error = opened.error;
        return ProfileFile::kOpenFailed;
    }
    std::uint64_t size = 0;
    std::vector<std::uint8_t> bytes;
    const bool read = files->Size(opened.handle, &size) == kWin32ErrorSuccess &&
                      files->Read(opened.handle, static_cast<std::uint32_t>(size), &bytes) == kWin32ErrorSuccess;
    files->Close(opened.handle);
    if (!read)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " cannot read " + file_name;
        return ProfileFile::kStop;
    }
    text->assign(bytes.begin(), bytes.end());
    return ProfileFile::kRead;
}

// Reads an optional guest string argument: false stops the handler, an
// argument of 0 leaves present false.
bool ReadOptionalString(const ImportCall& call, std::uint32_t address, std::string* value, bool* present, std::string* error)
{
    *present = address != 0;
    if (address == 0)
    {
        return true;
    }
    std::string read_error;
    // An empty string reads as a failure with no error text.
    if (!call.services->ReadGuestString(runtime::GuestAddress(address), value, &read_error) && !read_error.empty())
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " cannot read its arguments: " + read_error;
        return false;
    }
    return true;
}

// GetPrivateProfileIntA(lpAppName, lpKeyName, nDefault, lpFileName) over the
// guest's files, with the reading rules in private_profile.h, as measured on
// Windows 11: a value found sets the last error to 0; a missing key or
// section returns nDefault with ERROR_FILE_NOT_FOUND, a file that cannot be
// opened nDefault with the open's error; a null section or key returns 0 with
// the last error 0. [GAMEASSIGNMENTS] DemoVolume is the product's own setting
// and leaves the last error alone, as on the Windows product. A null file
// name stops, as does anything ReadProfileFile stops on.
bool GetPrivateProfileIntA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileIntA argument shape is invalid";
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileIntA needs the guest file model";
        return false;
    }
    if (call.arguments[0] == 0 || call.arguments[1] == 0)
    {
        result->eax = 0;
        call.services->SetLastError(kWin32ErrorSuccess);
        return true;
    }
    std::string section;
    std::string key;
    std::string file_name;
    bool present = false;
    bool file_present = false;
    if (!ReadOptionalString(call, call.arguments[0], &section, &present, error) ||
        !ReadOptionalString(call, call.arguments[1], &key, &present, error) ||
        !ReadOptionalString(call, call.arguments[3], &file_name, &file_present, error))
    {
        return false;
    }
    if (!file_present)
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileIntA of win.ini (a null file name) is not modelled";
        return false;
    }
    const std::uint32_t default_value = call.arguments[2];
    if (const std::optional<std::uint32_t> configured =
            PrivateProfileIntOverride(section, key, kDefaultDemoVolume))
    {
        result->eax = *configured;
        return true;
    }
    std::string text;
    std::uint32_t open_error = 0;
    switch (ReadProfileFile(call, files, file_name, &text, &open_error, error))
    {
    case ProfileFile::kStop:
        return false;
    case ProfileFile::kOpenFailed:
        result->eax = default_value;
        call.services->SetLastError(open_error);
        return true;
    case ProfileFile::kRead:
        break;
    }
    const std::optional<std::string> value = FindPrivateProfileValue(text, section, key);
    if (!value.has_value())
    {
        result->eax = default_value;
        call.services->SetLastError(kWin32ErrorFileNotFound);
        return true;
    }
    result->eax = ParsePrivateProfileInt(*value, default_value);
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

// GetPrivateProfileStringA(lpAppName, lpKeyName, lpDefault, lpReturnedString,
// nSize, lpFileName), as measured on Windows 11:
// - a value found, without one pair of matching quotes around it, with the
//   last error 0, or ERROR_MORE_DATA when it fills the buffer (cut to
//   nSize - 1, or exactly that long);
// - a missing key, section, or file gives lpDefault without trailing spaces
//   (none when null) with ERROR_FILE_NOT_FOUND or the open's error, cut to
//   the buffer without ERROR_MORE_DATA;
// - a null key lists the section's keys and a null section the section
//   names, each NUL-ended with one more NUL, cut to nSize - 2 with
//   ERROR_MORE_DATA.
// The number of characters written, the final NULs left out, is returned.
// A null buffer or file name, and a listing of a missing section or file,
// stop.
bool GetPrivateProfileStringA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kErrorMoreData = 234;
    if (result == nullptr || call.arguments.size() != 6)
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileStringA argument shape is invalid";
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileStringA needs the guest file model";
        return false;
    }
    std::string section;
    std::string key;
    std::string default_value;
    std::string file_name;
    bool section_present = false;
    bool key_present = false;
    bool default_present = false;
    bool file_present = false;
    if (!ReadOptionalString(call, call.arguments[0], &section, &section_present, error) ||
        !ReadOptionalString(call, call.arguments[1], &key, &key_present, error) ||
        !ReadOptionalString(call, call.arguments[2], &default_value, &default_present, error) ||
        !ReadOptionalString(call, call.arguments[5], &file_name, &file_present, error))
    {
        return false;
    }
    const std::uint32_t buffer = call.arguments[3];
    const std::uint32_t size = call.arguments[4];
    if (buffer == 0 || !file_present)
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileStringA of a null buffer or file name is not modelled";
        return false;
    }
    std::string text;
    std::uint32_t open_error = 0;
    const ProfileFile read = ReadProfileFile(call, files, file_name, &text, &open_error, error);
    if (read == ProfileFile::kStop)
    {
        return false;
    }
    PrivateProfileCopy copy;
    std::uint32_t last_error = kWin32ErrorSuccess;
    if (!section_present || !key_present)
    {
        std::optional<std::vector<std::string>> names;
        if (read == ProfileFile::kRead)
        {
            names = section_present ? ListPrivateProfileKeys(text, section) : ListPrivateProfileSections(text);
        }
        if (!names.has_value())
        {
            if (error != nullptr) *error = "kernel32 GetPrivateProfileStringA listing a missing section or file is not modelled";
            return false;
        }
        copy = CopyPrivateProfileList(*names, size);
        last_error = copy.truncated ? kErrorMoreData : kWin32ErrorSuccess;
    }
    else
    {
        const std::optional<std::string> value =
            read == ProfileFile::kRead ? FindPrivateProfileValue(text, section, key) : std::nullopt;
        if (value.has_value())
        {
            copy = CopyPrivateProfileString(PrivateProfileStringValue(*value), size);
            last_error = copy.truncated ? kErrorMoreData : kWin32ErrorSuccess;
        }
        else
        {
            copy = CopyPrivateProfileString(PrivateProfileStringDefault(default_value), size);
            last_error = read == ProfileFile::kOpenFailed ? open_error : kWin32ErrorFileNotFound;
        }
    }
    if (!copy.bytes.empty() && !PutGuestBytes(call, buffer, copy.bytes, error))
    {
        return false;
    }
    result->eax = copy.length;
    call.services->SetLastError(last_error);
    return true;
}

// WritePrivateProfileStringA(lpAppName, lpKeyName, lpString, lpFileName)
// over the guest's files, with the rewrite rules in private_profile.h as
// measured on Windows 11 (design 434): the file is read, rewritten and
// written back whole, into the overlay; a missing file starts empty; TRUE
// with the last error left alone. A missing directory is FALSE with the
// open's error. A null section or file name stops, as does anything
// ReadProfileFile stops on.
bool WritePrivateProfileStringA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 4)
    {
        if (error != nullptr) *error = "kernel32 WritePrivateProfileStringA argument shape is invalid";
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 WritePrivateProfileStringA needs the guest file model";
        return false;
    }
    std::string section;
    std::string key;
    std::string value;
    std::string file_name;
    bool section_present = false;
    bool key_present = false;
    bool value_present = false;
    bool file_present = false;
    if (!ReadOptionalString(call, call.arguments[0], &section, &section_present, error) ||
        !ReadOptionalString(call, call.arguments[1], &key, &key_present, error) ||
        !ReadOptionalString(call, call.arguments[2], &value, &value_present, error) ||
        !ReadOptionalString(call, call.arguments[3], &file_name, &file_present, error))
    {
        return false;
    }
    if (!section_present || !file_present)
    {
        if (error != nullptr) *error = "kernel32 WritePrivateProfileStringA of a null section or file name is not modelled";
        return false;
    }
    std::string text;
    std::uint32_t open_error = 0;
    const ProfileFile read = ReadProfileFile(call, files, file_name, &text, &open_error, error);
    if (read == ProfileFile::kStop)
    {
        return false;
    }
    if (read == ProfileFile::kOpenFailed && open_error != kWin32ErrorFileNotFound)
    {
        result->eax = 0;
        call.services->SetLastError(open_error);
        return true;
    }
    const std::string updated =
        UpdatePrivateProfile(text,
                             section,
                             key_present ? std::optional<std::string_view>(key) : std::nullopt,
                             value_present ? std::optional<std::string_view>(value) : std::nullopt);
    const GuestFiles::OpenResult opened = files->Open(file_name, false, true, kCreateAlways);
    if (opened.outside_root)
    {
        if (error != nullptr) *error = "kernel32 WritePrivateProfileStringA outside the guest root is not modelled: " + file_name;
        return false;
    }
    if (opened.handle == 0)
    {
        result->eax = 0;
        call.services->SetLastError(opened.error);
        return true;
    }
    const std::vector<std::uint8_t> bytes(updated.begin(), updated.end());
    const std::uint32_t written = files->Write(opened.handle, bytes);
    files->Close(opened.handle);
    if (written != kWin32ErrorSuccess)
    {
        if (error != nullptr) *error = "kernel32 WritePrivateProfileStringA cannot write " + file_name;
        return false;
    }
    result->eax = 1;
    return true;
}

// GetPrivateProfileSectionNamesA(lpReturnBuffer, nSize, lpFileName), as
// measured on Windows 11: the section names as GetPrivateProfileStringA
// lists them (every occurrence, NUL after each and one more, cut to
// nSize - 2 with ERROR_MORE_DATA, last error 0 otherwise); a file that cannot
// be opened writes one NUL and returns 0 with the open's error. A null buffer
// or file name stops.
bool GetPrivateProfileSectionNamesA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kErrorMoreData = 234;
    if (result == nullptr || call.arguments.size() != 3)
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileSectionNamesA argument shape is invalid";
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileSectionNamesA needs the guest file model";
        return false;
    }
    std::string file_name;
    bool file_present = false;
    if (!ReadOptionalString(call, call.arguments[2], &file_name, &file_present, error))
    {
        return false;
    }
    const std::uint32_t buffer = call.arguments[0];
    const std::uint32_t size = call.arguments[1];
    if (buffer == 0 || !file_present)
    {
        if (error != nullptr) *error = "kernel32 GetPrivateProfileSectionNamesA of a null buffer or file name is not modelled";
        return false;
    }
    std::string text;
    std::uint32_t open_error = 0;
    switch (ReadProfileFile(call, files, file_name, &text, &open_error, error))
    {
    case ProfileFile::kStop:
        return false;
    case ProfileFile::kOpenFailed:
    {
        const std::uint8_t terminator[1] = {0};
        if (size != 0 && !PutGuestBytes(call, buffer, terminator, error))
        {
            return false;
        }
        result->eax = 0;
        call.services->SetLastError(open_error);
        return true;
    }
    case ProfileFile::kRead:
        break;
    }
    const PrivateProfileCopy copy = CopyPrivateProfileList(ListPrivateProfileSections(text), size);
    if (!copy.bytes.empty() && !PutGuestBytes(call, buffer, copy.bytes, error))
    {
        return false;
    }
    result->eax = copy.length;
    call.services->SetLastError(copy.truncated ? kErrorMoreData : kWin32ErrorSuccess);
    return true;
}

// DeleteFileA(lpFileName) under GuestFiles, with the errors Microsoft
// documents (design 437): TRUE with the last error left alone, as the measured
// file APIs do; otherwise FALSE with GuestFiles::Delete's error. 6th deletes
// Remember 1st's bookkeeping.ini before handing its credits over. A null
// name or a path outside the guest root stops.
bool DeleteFileA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        if (error != nullptr) *error = "kernel32 DeleteFileA argument shape is invalid";
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 DeleteFileA needs the guest file model";
        return false;
    }
    if (call.arguments[0] == 0)
    {
        if (error != nullptr) *error = "kernel32 DeleteFileA of a null name is not modelled";
        return false;
    }
    std::string path;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &path, error))
    {
        return false;
    }
    bool outside_root = false;
    const std::uint32_t deleted = files->Delete(path, &outside_root);
    if (outside_root)
    {
        if (error != nullptr) *error = "kernel32 DeleteFileA of a path outside the guest root is not modelled: " + path;
        return false;
    }
    result->eax = deleted == kWin32ErrorSuccess ? 1 : 0;
    if (deleted != kWin32ErrorSuccess)
    {
        call.services->SetLastError(deleted);
    }
    return true;
}

// GetFileAttributesA(lpFileName) under GuestFiles, as measured on Windows 11:
// the attributes on success with the last error left alone, and
// INVALID_FILE_ATTRIBUTES with the walk's error on failure; a null name is
// ERROR_PATH_NOT_FOUND. A path outside the guest root stops.
bool GetFileAttributesA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        if (error != nullptr) *error = "kernel32 GetFileAttributesA argument shape is invalid";
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 GetFileAttributesA needs the guest file model";
        return false;
    }
    result->eax = storage::kInvalidFileAttributes;
    if (call.arguments[0] == 0)
    {
        call.services->SetLastError(kWin32ErrorPathNotFound);
        return true;
    }
    std::string path;
    std::string read_error;
    // An empty string reads as a failure with no error text.
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &path, &read_error) &&
        !read_error.empty())
    {
        if (error != nullptr) *error = "kernel32 GetFileAttributesA cannot read the path: " + read_error;
        return false;
    }
    bool outside_root = false;
    const storage::GuestFileAttributes attributes = files->Attributes(path, &outside_root);
    if (outside_root)
    {
        if (error != nullptr) *error = "kernel32 GetFileAttributesA of a path outside the guest root is not modelled: " + path;
        return false;
    }
    if (attributes.error != kWin32ErrorSuccess)
    {
        call.services->SetLastError(attributes.error);
        return true;
    }
    result->eax = attributes.attributes;
    return true;
}

// RtlUnwind(TargetFrame, TargetIp, ExceptionRecord, ReturnValue), as
// measured on Windows 11 (design 404):
// - each SEH frame above the target, innermost first, has its handler called
//   with the record flagged EXCEPTION_UNWINDING and is then unlinked from the
//   TEB's list, which ends at the target;
// - with no record the handlers get STATUS_UNWIND at the caller's return
//   address, and a record given is flagged in place;
// - the call returns to its caller with ReturnValue in eax; TargetIp is not
//   used.
// A null target (an exit unwind) and a target not on the list stop.
bool RtlUnwind(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kStatusUnwind = 0xC0000027U;
    constexpr std::uint32_t kExceptionUnwinding = 0x2U;
    constexpr std::uint32_t kEndOfList = 0xFFFFFFFFU;
    constexpr std::uint32_t kRecordSize = 80;
    constexpr std::uint32_t kContextSize = 716;
    constexpr std::uint32_t kDispatcherContextSize = 8;
    if (result == nullptr || call.arguments.size() != 4 || call.services == nullptr)
    {
        if (error != nullptr) *error = "kernel32 RtlUnwind argument shape is invalid";
        return false;
    }
    const std::uint32_t teb = call.services->ThreadEnvironmentBlock().value();
    GuestProcess* process = call.services->Process();
    if (teb == 0 || process == nullptr)
    {
        if (error != nullptr) *error = "kernel32 RtlUnwind needs the guest TEB and process heap";
        return false;
    }
    const std::uint32_t target = call.arguments[0];
    if (target == 0)
    {
        if (error != nullptr) *error = "kernel32 RtlUnwind of every frame (an exit unwind) is not modelled";
        return false;
    }
    const auto read_u32 = [&](std::uint32_t address, std::uint32_t* value) {
        std::array<std::uint8_t, 4> bytes{};
        std::string memory_error;
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(address), bytes, &memory_error))
        {
            if (error != nullptr) *error = "kernel32 RtlUnwind: " + memory_error;
            return false;
        }
        std::memcpy(value, bytes.data(), sizeof(*value));
        return true;
    };
    const auto write_u32 = [&](std::uint32_t address, std::uint32_t value) {
        std::array<std::uint8_t, 4> bytes{};
        std::memcpy(bytes.data(), &value, sizeof(value));
        return PutGuestBytes(call, address, bytes, error);
    };

    // The record the handlers see, its CONTEXT, and a dispatcher context, in
    // one heap block for the length of the call.
    const std::uint32_t block = process->Allocate(kRecordSize + kContextSize + kDispatcherContextSize);
    if (block == 0)
    {
        if (error != nullptr) *error = "kernel32 RtlUnwind has no heap room for its record";
        return false;
    }
    std::uint32_t record = call.arguments[2];
    std::uint32_t flags = 0;
    bool ok = true;
    if (record == 0)
    {
        std::array<std::uint8_t, kRecordSize + kContextSize + kDispatcherContextSize> bytes{};
        const std::uint32_t words[] = {kStatusUnwind, 0, 0, call.return_address};
        std::memcpy(bytes.data(), words, sizeof(words));
        ok = PutGuestBytes(call, block, bytes, error);
        record = block;
    }
    else
    {
        std::array<std::uint8_t, kContextSize + kDispatcherContextSize> bytes{};
        ok = PutGuestBytes(call, block + kRecordSize, bytes, error);
    }
    ok = ok && read_u32(record + 4, &flags) && write_u32(record + 4, flags | kExceptionUnwinding);

    std::uint32_t frame = 0;
    ok = ok && read_u32(teb, &frame);
    while (ok && frame != target)
    {
        if (frame == kEndOfList)
        {
            if (error != nullptr) *error = "kernel32 RtlUnwind to a frame not on the SEH list is not modelled";
            ok = false;
            break;
        }
        std::uint32_t next = 0;
        std::uint32_t handler = 0;
        ok = read_u32(frame, &next) && read_u32(frame + 4, &handler);
        if (!ok)
        {
            break;
        }
        GuestCall handler_call;
        handler_call.function = handler;
        handler_call.arguments = {record, frame, block + kRecordSize, block + kRecordSize + kContextSize};
        std::uint32_t disposition = 0;
        std::string call_error;
        if (!call.services->CallGuest(&handler_call, &disposition, &call_error))
        {
            if (error != nullptr) *error = "kernel32 RtlUnwind cannot call a handler: " + call_error;
            ok = false;
            break;
        }
        ok = write_u32(teb, next);
        frame = next;
    }
    process->Free(block);
    if (!ok)
    {
        return false;
    }
    result->eax = call.arguments[3];
    return true;
}

// FindFirstFileA(lpFileName, lpFindFileData) under GuestFiles' search, as
// measured on Windows 11: success returns the search handle and leaves the
// last error alone; failure is INVALID_HANDLE_VALUE with the search's error,
// ERROR_PATH_NOT_FOUND for a null name. A null data pointer is
// ERROR_INVALID_PARAMETER, as the Windows product answers. A directory
// outside the guest root is not modelled and stops the call.
bool FindFirstFileA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 2)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 FindFirstFileA argument shape is invalid";
        }
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->configured())
    {
        if (error != nullptr) *error = "kernel32 FindFirstFileA needs the guest file model";
        return false;
    }
    result->eax = kKernel32InvalidHandle;
    if (call.arguments[1] == 0)
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    std::string name;
    std::string read_error;
    if (call.arguments[0] != 0 &&
        !call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &name, &read_error) &&
        !read_error.empty())
    {
        if (error != nullptr) *error = "kernel32 FindFirstFileA cannot read the name: " + read_error;
        return false;
    }
    const GuestFiles::FindResult found = files->FindFirst(name);
    if (found.outside_root)
    {
        if (error != nullptr) *error = "kernel32 FindFirstFileA outside the guest root is not modelled: " + name;
        return false;
    }
    if (found.handle == 0)
    {
        call.services->SetLastError(found.error);
        return true;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&found.first);
    if (!PutGuestBytes(call, call.arguments[1], std::span<const std::uint8_t>(bytes, sizeof(found.first)), error))
    {
        files->FindClose(found.handle);
        return false;
    }
    result->eax = found.handle;
    return true;
}

// FindNextFileA(hFindFile, lpFindFileData): the next match, TRUE leaving the
// last error alone; FALSE with ERROR_NO_MORE_FILES at the end (measured), or
// ERROR_INVALID_HANDLE for a handle that is no search.
bool FindNextFileA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 2)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 FindNextFileA argument shape is invalid";
        }
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || call.arguments[1] == 0)
    {
        if (call.services != nullptr) call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    storage::GuestFindData data;
    const std::uint32_t failure = files->FindNext(call.arguments[0], &data);
    if (failure != kWin32ErrorSuccess)
    {
        call.services->SetLastError(failure);
        return true;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&data);
    if (!PutGuestBytes(call, call.arguments[1], std::span<const std::uint8_t>(bytes, sizeof(data)), error))
    {
        return false;
    }
    result->eax = 1;
    return true;
}

// FindClose(hFindFile): TRUE leaving the last error alone, or FALSE with
// ERROR_INVALID_HANDLE for a handle that is no open search.
bool FindClose(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != 1)
    {
        if (error != nullptr && error->empty())
        {
            *error = "kernel32 FindClose argument shape is invalid";
        }
        return false;
    }
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files == nullptr || !files->FindClose(call.arguments[0]))
    {
        if (call.services != nullptr) call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    result->eax = 1;
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
// STARTUPINFO leaves it, cb aside. A process a launcher started also gets
// back the cbReserved2 bytes its launcher passed in lpReserved2, copied into
// its own memory as Windows 11 copies them into the child (task 431); the
// copy is placed once on the process heap.
bool GetStartupInfoA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetStartupInfoA argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    constexpr std::uint32_t kStartupInfoSize = 68;
    constexpr std::size_t kReservedSizeOffset = 0x32;
    constexpr std::size_t kReservedOffset = 0x34;
    std::array<std::uint8_t, kStartupInfoSize> info = {};
    StoreDword(info, 0, kStartupInfoSize);
    GuestProcess* process = call.services->Process();
    if (process != nullptr && !process->startup().reserved.empty())
    {
        const std::vector<std::uint8_t>& reserved = process->startup().reserved;
        if (process->startup_reserved_address() == 0)
        {
            const std::uint32_t block = PlaceOnProcessHeap(call, process, reserved, error);
            if (block == 0)
            {
                return false;
            }
            process->set_startup_reserved_address(block);
        }
        info[kReservedSizeOffset] = static_cast<std::uint8_t>(reserved.size());
        info[kReservedSizeOffset + 1] = static_cast<std::uint8_t>(reserved.size() >> 8);
        StoreDword(info, kReservedOffset, process->startup_reserved_address());
    }
    return PutGuestBytes(call, call.arguments[0], info, error);
}

// GetFullPathNameA(lpFileName, nBufferLength, lpBuffer, lpFilePart), as
// measured on Windows 11 (task 431): the path resolved against the guest's
// current directory (GuestFiles::FullPath), whether or not it exists; its
// length without the terminator, or, for a buffer too short, the size it
// needs with the buffer and file part left alone; the file part at the last
// component, or NULL after a trailing separator; the last error left alone.
// An empty name is 0 with ERROR_INVALID_NAME.
bool GetFullPathNameA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 4, "kernel32 GetFullPathNameA argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    GuestFiles* files = call.services->Files();
    if (files == nullptr)
    {
        if (error != nullptr) *error = "kernel32 GetFullPathNameA needs the guest files";
        return false;
    }
    std::array<std::uint8_t, 1> first = {};
    std::string read_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[0]), first, &read_error))
    {
        if (error != nullptr) *error = "kernel32 GetFullPathNameA cannot read the name: " + read_error;
        return false;
    }
    if (first[0] == 0)
    {
        call.services->SetLastError(kWin32ErrorInvalidName);
        return true;
    }
    std::string name;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[0]), &name, &read_error))
    {
        if (error != nullptr) *error = "kernel32 GetFullPathNameA cannot read the name: " + read_error;
        return false;
    }
    std::string full;
    if (!files->FullPath(name, &full))
    {
        if (error != nullptr) *error = "kernel32 GetFullPathNameA of this path is not modelled: " + name;
        return false;
    }
    const std::uint32_t length = static_cast<std::uint32_t>(full.size());
    if (call.arguments[2] == 0 || call.arguments[1] <= length)
    {
        result->eax = length + 1;
        return true;
    }
    std::vector<std::uint8_t> bytes(full.begin(), full.end());
    bytes.push_back(0);
    if (!PutGuestBytes(call, call.arguments[2], bytes, error))
    {
        return false;
    }
    if (call.arguments[3] != 0)
    {
        const std::size_t separator = full.find_last_of('\\');
        const std::uint32_t part = separator + 1 >= full.size()
                                       ? 0U
                                       : call.arguments[2] + static_cast<std::uint32_t>(separator + 1);
        std::array<std::uint8_t, 4> value = {};
        StoreDword(value, 0, part);
        if (!PutGuestBytes(call, call.arguments[3], value, error))
        {
            return false;
        }
    }
    result->eax = length;
    return true;
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

// GetFileType(hFile), as measured on Windows 11: an open file is
// FILE_TYPE_DISK whatever its access, leaving the last error alone; a closed,
// NULL, INVALID_HANDLE_VALUE or unknown handle is FILE_TYPE_UNKNOWN with
// ERROR_INVALID_HANDLE. A device handle is not modelled.
bool GetFileType(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kFileTypeDisk = 1;
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
    GuestFiles* files = call.services == nullptr ? nullptr : call.services->Files();
    if (files != nullptr && files->IsOpen(call.arguments[0]))
    {
        result->eax = kFileTypeDisk;
        return true;
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
    result->eax = call.services == nullptr ? GuestProcess::kThreadId : call.services->CurrentThreadId();
    return true;
}

// IsBadReadPtr(lp, ucb) / IsBadWritePtr(lp, ucb), as measured on Windows 11:
// 0 when every byte can be read (or, for a write, written), nonzero
// otherwise, 0 for a zero size whatever the pointer; the last error is left
// alone. Readable is what the guest memory services can read; writable also
// needs a recorded page protection that allows writing, where one is kept.
bool IsBadPointer(const ImportCall& call, ImportReturn* result, bool write, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 IsBad*Ptr argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t address = call.arguments[0];
    const std::uint32_t size = call.arguments[1];
    if (size == 0)
    {
        return true;
    }
    if (address + size - 1 < address)
    {
        result->eax = 1;
        return true;
    }
    constexpr std::uint32_t kChunk = GuestProcess::kPageSize;
    std::array<std::uint8_t, kChunk> scratch{};
    const GuestProcess* process = call.services->Process();
    for (std::uint32_t offset = 0; offset < size; offset += kChunk)
    {
        const std::uint32_t length = std::min(kChunk, size - offset);
        std::string memory_error;
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(address + offset),
                                           std::span<std::uint8_t>(scratch.data(), length), &memory_error))
        {
            result->eax = 1;
            return true;
        }
    }
    if (write && process != nullptr)
    {
        constexpr std::uint32_t kWritable = 0x04 | 0x08 | 0x40 | 0x80;  // (EXECUTE_)READWRITE, (EXECUTE_)WRITECOPY
        const std::uint32_t first_page = address & ~(GuestProcess::kPageSize - 1);
        for (std::uint32_t page = first_page; page - first_page < size + (address - first_page);
             page += GuestProcess::kPageSize)
        {
            const std::uint32_t protection = process->PageProtection(page);
            if (protection != 0 && (protection & kWritable) == 0)
            {
                result->eax = 1;
                return true;
            }
        }
    }
    return true;
}

bool IsBadReadPtr(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return IsBadPointer(call, result, false, error);
}

bool IsBadWritePtr(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return IsBadPointer(call, result, true, error);
}

// GetCurrentThread(): the pseudo-handle (HANDLE)-2.
bool GetCurrentThread(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 GetCurrentThread argument shape is invalid", error))
    {
        return false;
    }
    result->eax = GuestProcess::kCurrentThreadHandle;
    return true;
}

// Reads or writes guest dwords for the handlers below, failing the handler
// with the gate's name.
bool ReadGuestWords(const ImportCall& call, std::uint32_t address, std::span<std::uint32_t> words, std::string* error)
{
    std::string memory_error;
    const std::span<std::uint8_t> bytes(reinterpret_cast<std::uint8_t*>(words.data()), words.size_bytes());
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(address), bytes, &memory_error))
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + ": " + memory_error;
        return false;
    }
    return true;
}

bool WriteGuestWords(const ImportCall& call,
                     std::uint32_t address,
                     std::span<const std::uint32_t> words,
                     std::string* error)
{
    return PutGuestBytes(
        call, address,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(words.data()), words.size_bytes()), error);
}

// A CRITICAL_SECTION's dwords: DebugInfo, LockCount, RecursionCount,
// OwningThread, LockSemaphore, SpinCount.
// CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter,
// dwCreationFlags, lpThreadId), as measured on Windows 11: a handle, the new
// ID written when lpThreadId is given, and the last error left alone. The
// thread runs once the caller next waits or calls an import. A suspended
// start (CREATE_SUSPENDED) or no ThreadProc is not modelled.
bool CreateThread(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kCreateSuspended = 0x00000004U;
    if (!CheckArgumentCount(call, result, 6, "kernel32 CreateThread argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t start = call.arguments[2];
    if (start == 0 || (call.arguments[4] & kCreateSuspended) != 0)
    {
        if (error != nullptr) *error = "kernel32 CreateThread without a ThreadProc or suspended is not modelled";
        return false;
    }
    std::uint32_t thread_id = 0;
    const std::uint32_t handle = process->CreateThread(start, call.arguments[3], &thread_id);
    if (!call.services->StartGuestThread(start, call.arguments[3], thread_id, error))
    {
        process->FinishThread(thread_id, 0);
        process->CloseThreadHandle(handle);
        return false;
    }
    if (call.arguments[5] != 0)
    {
        const std::uint32_t id_word[1] = {thread_id};
        if (!WriteGuestWords(call, call.arguments[5], id_word, error))
        {
            return false;
        }
    }
    result->eax = handle;
    return true;
}

// The THREAD_PRIORITY_* values SetThreadPriority takes, as measured: IDLE
// (-15), LOWEST (-2) through HIGHEST (2), and TIME_CRITICAL (15).
bool IsThreadPriority(std::int32_t priority)
{
    return priority == -15 || priority == 15 || (priority >= -2 && priority <= 2);
}

// SetThreadPriority(hThread, nPriority) / GetThreadPriority(hThread) on the
// GetCurrentThread pseudo-handle or a thread handle, finished threads
// included, as measured on Windows 11. Another handle is ERROR_INVALID_HANDLE
// and a value outside THREAD_PRIORITY_* ERROR_INVALID_PARAMETER, each with
// 0 (GetThreadPriority: THREAD_PRIORITY_ERROR_RETURN). Host threads all run
// at one priority; the value is only kept.
//
// The thread hThread names: *thread, or neither it nor anything else when
// the pseudo-handle names the main thread (*main). False for another handle.
bool FindPriorityThread(const ImportCall& call, GuestProcess* process, GuestThread** thread, bool* main)
{
    *main = false;
    if (call.arguments[0] == GuestProcess::kCurrentThreadHandle)
    {
        *thread = process->FindThread(call.services->CurrentThreadId());
        *main = *thread == nullptr;
        return true;
    }
    *thread = process->FindThreadHandle(call.arguments[0]);
    return *thread != nullptr;
}

bool SetThreadPriority(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 SetThreadPriority argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestThread* thread = nullptr;
    bool main = false;
    const auto priority = static_cast<std::int32_t>(call.arguments[1]);
    if (!FindPriorityThread(call, process, &thread, &main))
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    if (!IsThreadPriority(priority))
    {
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    if (main)
    {
        process->set_main_thread_priority(priority);
    }
    else
    {
        thread->priority = priority;
    }
    result->eax = 1;
    return true;
}

bool GetThreadPriority(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kThreadPriorityErrorReturn = 0x7FFFFFFFU;
    if (!CheckArgumentCount(call, result, 1, "kernel32 GetThreadPriority argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestThread* thread = nullptr;
    bool main = false;
    if (!FindPriorityThread(call, process, &thread, &main))
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        result->eax = kThreadPriorityErrorReturn;
        return true;
    }
    result->eax = static_cast<std::uint32_t>(main ? process->main_thread_priority() : thread->priority);
    return true;
}

constexpr std::size_t kCriticalSectionWords = 6;
constexpr std::uint32_t kCriticalSectionFree = 0xFFFFFFFFU;
constexpr std::uint32_t kCriticalSectionOwned = 0xFFFFFFFEU;

// InitializeCriticalSection(lpCriticalSection), as measured on Windows 11:
// DebugInfo -1, LockCount -1 (free), no owner, and the default spin count
// with its flags, 0x020007D0. The last error is left alone.
bool InitializeCriticalSection(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 InitializeCriticalSection argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t words[kCriticalSectionWords] = {0xFFFFFFFFU, kCriticalSectionFree, 0, 0, 0, 0x020007D0U};
    return WriteGuestWords(call, call.arguments[0], words, error);
}

// EnterCriticalSection(lpCriticalSection): a free section becomes the
// calling thread's (LockCount -2, count 1, owner its ID), an owned one counts
// up, as measured on Windows 11. A section another thread owns is waited
// for, in 1 ms steps, while that thread can still leave it; with no other
// thread running, the wait could never end and stops.
bool EnterCriticalSection(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 EnterCriticalSection argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t self = call.services == nullptr ? GuestProcess::kThreadId : call.services->CurrentThreadId();
    std::uint32_t words[kCriticalSectionWords] = {};
    for (;;)
    {
        if (!ReadGuestWords(call, call.arguments[0], words, error))
        {
            return false;
        }
        if (words[1] == kCriticalSectionFree || words[3] == self)
        {
            break;
        }
        GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
        if (process == nullptr || process->running_threads() == 0 || !call.services->WaitMilliseconds(1))
        {
            if (error != nullptr) *error = "kernel32 EnterCriticalSection of a section another thread owns would block for good";
            return false;
        }
    }
    if (words[1] == kCriticalSectionFree)
    {
        words[1] = kCriticalSectionOwned;
        words[2] = 1;
        words[3] = self;
    }
    else
    {
        ++words[2];
    }
    return WriteGuestWords(call, call.arguments[0], words, error);
}

// LeaveCriticalSection(lpCriticalSection): the count goes down, and at zero
// the section is free again with no owner, as measured on Windows 11.
// Leaving a section this thread does not own stops.
bool LeaveCriticalSection(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 LeaveCriticalSection argument shape is invalid", error))
    {
        return false;
    }
    std::uint32_t words[kCriticalSectionWords] = {};
    if (!ReadGuestWords(call, call.arguments[0], words, error))
    {
        return false;
    }
    const std::uint32_t self = call.services == nullptr ? GuestProcess::kThreadId : call.services->CurrentThreadId();
    if (words[3] != self || words[2] == 0)
    {
        if (error != nullptr) *error = "kernel32 LeaveCriticalSection of a section this thread does not own is not modelled";
        return false;
    }
    if (--words[2] == 0)
    {
        words[1] = kCriticalSectionFree;
        words[3] = 0;
    }
    return WriteGuestWords(call, call.arguments[0], words, error);
}

// DeleteCriticalSection(lpCriticalSection): every dword zero, as measured.
bool DeleteCriticalSection(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 DeleteCriticalSection argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t words[kCriticalSectionWords] = {};
    return WriteGuestWords(call, call.arguments[0], words, error);
}

// InterlockedIncrement / InterlockedDecrement(lpAddend): the new value. One
// guest thread runs at a time, so a plain read and write is atomic enough.
bool InterlockedAdd(const ImportCall& call, ImportReturn* result, std::uint32_t delta, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 Interlocked argument shape is invalid", error))
    {
        return false;
    }
    std::uint32_t value[1] = {};
    if (!ReadGuestWords(call, call.arguments[0], value, error))
    {
        return false;
    }
    value[0] += delta;
    result->eax = value[0];
    return WriteGuestWords(call, call.arguments[0], value, error);
}

bool InterlockedIncrement(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return InterlockedAdd(call, result, 1, error);
}

bool InterlockedDecrement(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return InterlockedAdd(call, result, 0xFFFFFFFFU, error);
}

// The TEB's TLS slot for index, or 0 when the host models no TEB.
std::uint32_t TlsSlotAddress(const ImportCall& call, std::uint32_t index)
{
    const std::uint32_t teb = call.services->ThreadEnvironmentBlock().value();
    return teb == 0 ? 0 : teb + GuestProcess::kTebTlsSlots + index * 4;
}

// TlsAlloc(): the lowest free index, its TEB slot zero, as on Windows 11,
// where the first index a program gets is 1. The last error is left alone.
// Running out of the TEB's 64 slots stops (the expansion slots are not
// modelled).
bool TlsAlloc(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 0, "kernel32 TlsAlloc argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = call.services->Process();
    if (process == nullptr || call.services->ThreadEnvironmentBlock().value() == 0)
    {
        if (error != nullptr) *error = "kernel32 TlsAlloc needs the guest process and TEB";
        return false;
    }
    const std::uint32_t index = process->AllocateTls();
    if (index == GuestProcess::kTlsOutOfIndexes)
    {
        if (error != nullptr) *error = "kernel32 TlsAlloc beyond the TEB's slots is not modelled";
        return false;
    }
    const std::uint32_t zero[1] = {};
    result->eax = index;
    return WriteGuestWords(call, TlsSlotAddress(call, index), zero, error);
}

// TlsFree(dwTlsIndex): TRUE for an allocated index, whose slot is cleared;
// FALSE with ERROR_INVALID_PARAMETER otherwise (measured).
bool TlsFree(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 TlsFree argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = call.services->Process();
    if (process == nullptr || call.services->ThreadEnvironmentBlock().value() == 0)
    {
        if (error != nullptr) *error = "kernel32 TlsFree needs the guest process and TEB";
        return false;
    }
    const std::uint32_t index = call.arguments[0];
    if (!process->FreeTls(index))
    {
        result->eax = 0;
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return true;
    }
    const std::uint32_t zero[1] = {};
    result->eax = 1;
    return WriteGuestWords(call, TlsSlotAddress(call, index), zero, error);
}

// TlsGetValue(dwTlsIndex) / TlsSetValue(dwTlsIndex, lpTlsValue) on the TEB's
// slots, as measured on Windows 11: any index below 64, allocated or not,
// works; a get sets the last error to 0, a set leaves it alone. An index past
// the expansion slots (1088 in all) is ERROR_INVALID_PARAMETER; an expansion
// slot itself stops.
enum class TlsIndexCheck
{
    kUsable,
    kInvalid,  // answered: 0 with ERROR_INVALID_PARAMETER
    kStop,
};

TlsIndexCheck CheckTlsIndex(const ImportCall& call, ImportReturn* result, std::uint32_t index, std::string* error)
{
    constexpr std::uint32_t kAllTlsSlots = 1088;
    if (call.services->ThreadEnvironmentBlock().value() == 0)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " needs the guest TEB";
        return TlsIndexCheck::kStop;
    }
    if (index >= kAllTlsSlots)
    {
        result->eax = 0;
        call.services->SetLastError(kWin32ErrorInvalidParameter);
        return TlsIndexCheck::kInvalid;
    }
    if (index >= GuestProcess::kTlsSlots)
    {
        if (error != nullptr) *error = "kernel32 " + call.gate.name + " of an expansion slot is not modelled";
        return TlsIndexCheck::kStop;
    }
    return TlsIndexCheck::kUsable;
}

bool TlsGetValue(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 TlsGetValue argument shape is invalid", error))
    {
        return false;
    }
    const TlsIndexCheck check = CheckTlsIndex(call, result, call.arguments[0], error);
    if (check != TlsIndexCheck::kUsable)
    {
        return check == TlsIndexCheck::kInvalid;
    }
    std::uint32_t value[1] = {};
    if (!ReadGuestWords(call, TlsSlotAddress(call, call.arguments[0]), value, error))
    {
        return false;
    }
    result->eax = value[0];
    call.services->SetLastError(kWin32ErrorSuccess);
    return true;
}

bool TlsSetValue(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 TlsSetValue argument shape is invalid", error))
    {
        return false;
    }
    const TlsIndexCheck check = CheckTlsIndex(call, result, call.arguments[0], error);
    if (check != TlsIndexCheck::kUsable)
    {
        return check == TlsIndexCheck::kInvalid;
    }
    const std::uint32_t value[1] = {call.arguments[1]};
    result->eax = 1;
    return WriteGuestWords(call, TlsSlotAddress(call, call.arguments[0]), value, error);
}

// GetCommandLineA(): the quoted module path, or the command line a launcher
// started the process with (task 431), placed once on the process heap.
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
        const std::string text = !process->startup().command_line.empty()
                                     ? process->startup().command_line
                                     : "\"" + process->module_path() + "\"";
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

// UnhandledExceptionFilter(ExceptionInfo): the CRT's last resort for an
// exception no guest handler took. On Windows with no debugger and no filter
// of the guest's own, the process then ends with Windows Error Reporting; a
// guest filter would be called first, which is not modelled. The call stops
// with the exception's code, address, and parameters, read from the
// EXCEPTION_POINTERS, so the run reports what went wrong.
bool UnhandledExceptionFilter(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 1, "kernel32 UnhandledExceptionFilter argument shape is invalid", error))
    {
        return false;
    }
    std::uint32_t pointers[2] = {};
    std::uint32_t record[5] = {};
    if (!ReadGuestWords(call, call.arguments[0], pointers, error) || !ReadGuestWords(call, pointers[0], record, error))
    {
        return false;
    }
    std::uint32_t parameters[2] = {};
    const std::uint32_t count = std::min<std::uint32_t>(record[4], 2);
    if (count != 0 && !ReadGuestWords(call, pointers[0] + 20, std::span<std::uint32_t>(parameters, count), error))
    {
        return false;
    }
    char text[160];
    std::snprintf(text, sizeof(text),
                  "kernel32 UnhandledExceptionFilter: the guest left exception %08X at %08X unhandled "
                  "(flags %08X, parameters %08X %08X)",
                  record[0], record[3], record[1], parameters[0], parameters[1]);
    if (error != nullptr) *error = text;
    return false;
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

// Sleep(dwMilliseconds): the host waits that long, letting other guest
// threads run; Sleep(0) returns at once (another thread waiting for the
// guest lock runs as the next import starts). The last error is left alone.
// INFINITE would block the thread for good and stops.
bool Sleep(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kInfinite = 0xFFFFFFFFU;
    if (!CheckArgumentCount(call, result, 1, "kernel32 Sleep argument shape is invalid", error))
    {
        return false;
    }
    const std::uint32_t milliseconds = call.arguments[0];
    if (milliseconds == kInfinite)
    {
        if (error != nullptr) *error = "kernel32 Sleep(INFINITE) would block the guest thread for good";
        return false;
    }
    if (milliseconds != 0 && !call.services->WaitMilliseconds(milliseconds))
    {
        if (error != nullptr) *error = "kernel32 Sleep needs a host that can wait";
        return false;
    }
    return true;
}

// WaitForSingleObject(hHandle, dwMilliseconds) on an event or a thread. A
// signalled event returns WAIT_OBJECT_0 (an auto-reset one resets), as does
// a finished thread; otherwise a zero timeout returns WAIT_TIMEOUT at once.
// While another guest thread runs, which could signal the object, the wait
// goes on in 1 ms steps until it does or the timeout has passed. With none,
// nothing can signal it: a timeout is waited out, and INFINITE stops.
// A child's state from the host, while it has not yet been seen to end.
void PollChildProcess(const ImportCall& call, GuestProcess::ChildProcess* child)
{
    HostProcessLauncher* launcher = call.services == nullptr ? nullptr : call.services->ProcessLauncher();
    if (child->finished || launcher == nullptr)
    {
        return;
    }
    bool finished = false;
    std::uint32_t exit_code = 0;
    if (launcher->Poll(child->host_child, &finished, &exit_code) && finished)
    {
        child->finished = true;
        child->exit_code = exit_code;
    }
}

// The first token of a command line, as CreateProcessA takes the executable
// from it when lpApplicationName is NULL: up to the closing quote when it
// starts with one, else up to the first space or tab.
std::string CommandLineExecutable(const std::string& command_line)
{
    if (!command_line.empty() && command_line.front() == '"')
    {
        const std::size_t end = command_line.find('"', 1);
        return command_line.substr(1, end == std::string::npos ? std::string::npos : end - 1);
    }
    return command_line.substr(0, command_line.find_first_of(" \t"));
}

// CreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttributes,
// lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
// lpCurrentDirectory, lpStartupInfo, lpProcessInformation), as a launcher
// starts another executable of the same image. As measured on Windows 11
// (task 431): the executable is the command line's first token, resolved
// against the parent's current directory; the child's command line is the
// parent's, its current directory lpCurrentDirectory, and its STARTUPINFO
// carries the cbReserved2 bytes of lpReserved2. The host starts the child as
// a host process of its own. A missing executable is FALSE with
// ERROR_FILE_NOT_FOUND and a zeroed PROCESS_INFORMATION. An application name,
// an environment block, or creation flags are not modelled.
bool CreateProcessA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 10, "kernel32 CreateProcessA argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[0] != 0 || call.arguments[6] != 0 || call.arguments[5] != 0 || call.arguments[1] == 0 ||
        call.arguments[8] == 0 || call.arguments[9] == 0)
    {
        if (error != nullptr)
        {
            *error = "kernel32 CreateProcessA with an application name, an environment, creation flags, or no "
                     "command line, startup information or process information is not modelled";
        }
        return false;
    }
    GuestFiles* files = call.services->Files();
    HostProcessLauncher* launcher = call.services->ProcessLauncher();
    if (files == nullptr || launcher == nullptr)
    {
        if (error != nullptr) *error = "kernel32 CreateProcessA needs the guest files and a host that starts processes";
        return false;
    }
    ChildProcessRequest request;
    std::string read_error;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]), &request.command_line, &read_error))
    {
        if (error != nullptr) *error = "kernel32 CreateProcessA cannot read the command line: " + read_error;
        return false;
    }
    if (call.arguments[7] != 0)
    {
        if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[7]), &request.current_directory,
                                            &read_error))
        {
            if (error != nullptr) *error = "kernel32 CreateProcessA cannot read the current directory: " + read_error;
            return false;
        }
    }
    else
    {
        request.current_directory = files->CurrentDirectory();
    }
    std::array<std::uint8_t, 68> startup = {};
    std::string memory_error;
    if (!call.services->ReadGuestBytes(runtime::GuestAddress(call.arguments[8]), startup, &memory_error))
    {
        if (error != nullptr) *error = "kernel32 CreateProcessA cannot read the startup information";
        return false;
    }
    const std::uint32_t reserved_size =
        static_cast<std::uint32_t>(startup[0x32]) | (static_cast<std::uint32_t>(startup[0x33]) << 8);
    std::uint32_t reserved_address = 0;
    std::memcpy(&reserved_address, startup.data() + 0x34, sizeof(reserved_address));
    if (reserved_size != 0 && reserved_address != 0)
    {
        request.startup_reserved.resize(reserved_size);
        if (!call.services->ReadGuestBytes(runtime::GuestAddress(reserved_address), request.startup_reserved,
                                           &memory_error))
        {
            if (error != nullptr) *error = "kernel32 CreateProcessA cannot read lpReserved2";
            return false;
        }
    }
    std::string executable = CommandLineExecutable(request.command_line);
    const std::size_t name_start = executable.find_last_of("\\/");
    if (executable.find('.', name_start == std::string::npos ? 0 : name_start + 1) == std::string::npos)
    {
        executable += ".exe";
    }
    bool outside_root = false;
    const std::uint32_t found = files->ImagePath(executable, &request.image_path, &outside_root);
    if (outside_root)
    {
        if (error != nullptr)
        {
            *error = "kernel32 CreateProcessA of a path outside the guest root is not modelled: " + executable;
        }
        return false;
    }
    std::array<std::uint8_t, 16> information = {};
    if (found != kWin32ErrorSuccess)
    {
        if (!PutGuestBytes(call, call.arguments[9], information, error))
        {
            return false;
        }
        result->eax = 0;
        call.services->SetLastError(found);
        return true;
    }
    std::uint32_t host_child = 0;
    std::string start_error;
    if (!launcher->Start(request, &host_child, &start_error))
    {
        if (error != nullptr)
        {
            *error = "kernel32 CreateProcessA could not start " + request.image_path + ": " + start_error;
        }
        return false;
    }
    const GuestProcess::ChildProcess& child = process->AddChildProcess(host_child);
    StoreDword(information, 0, child.process_handle);
    StoreDword(information, 4, child.thread_handle);
    StoreDword(information, 8, child.process_id);
    StoreDword(information, 12, child.thread_id);
    if (!PutGuestBytes(call, call.arguments[9], information, error))
    {
        return false;
    }
    result->eax = 1;
    return true;
}

// GetExitCodeProcess(hProcess, lpExitCode): STILL_ACTIVE (259) while a child
// runs and then its exit code, as measured on Windows 11 (task 431); this
// process, by its pseudo-handle, is running. Any other handle is FALSE with
// ERROR_INVALID_HANDLE.
bool GetExitCodeProcess(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kStillActive = 259;
    if (!CheckArgumentCount(call, result, 2, "kernel32 GetExitCodeProcess argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    std::uint32_t code = 0;
    if (GuestProcess::ChildProcess* child = process->FindChildProcess(call.arguments[0]); child != nullptr)
    {
        PollChildProcess(call, child);
        code = child->finished ? child->exit_code : kStillActive;
    }
    else if (call.arguments[0] == GuestProcess::kCurrentProcessHandle)
    {
        code = kStillActive;
    }
    else
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    const std::uint32_t value[1] = {code};
    result->eax = 1;
    return WriteGuestWords(call, call.arguments[1], value, error);
}

// SetPriorityClass(hProcess, dwPriorityClass): TRUE for a child or this
// process, as Windows 11 answers for a child (task 431); the host's
// scheduling does not change. Any other handle is FALSE with
// ERROR_INVALID_HANDLE.
bool SetPriorityClass(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 2, "kernel32 SetPriorityClass argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    if (process->FindChildProcess(call.arguments[0]) == nullptr &&
        call.arguments[0] != GuestProcess::kCurrentProcessHandle)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    result->eax = 1;
    return true;
}

// WaitForSingleObject on a child's process handle: WAIT_OBJECT_0 once the
// host reports its end, WAIT_TIMEOUT when the timeout passes first, as
// measured on Windows 11 (task 431).
bool WaitForChildProcess(const ImportCall& call,
                         GuestProcess* process,
                         std::uint32_t handle,
                         std::uint32_t timeout,
                         ImportReturn* result,
                         std::string* error)
{
    constexpr std::uint32_t kWaitObject0 = 0x00000000U;
    constexpr std::uint32_t kWaitTimeout = 0x00000102U;
    constexpr std::uint32_t kWaitFailed = 0xFFFFFFFFU;
    constexpr std::uint32_t kInfinite = 0xFFFFFFFFU;
    constexpr std::uint32_t kPollMilliseconds = 10;
    std::uint32_t waited = 0;
    for (;;)
    {
        GuestProcess::ChildProcess* child = process->FindChildProcess(handle);
        if (child == nullptr)
        {
            result->eax = kWaitFailed;
            call.services->SetLastError(kWin32ErrorInvalidHandle);
            return true;
        }
        PollChildProcess(call, child);
        if (child->finished)
        {
            result->eax = kWaitObject0;
            return true;
        }
        if (timeout != kInfinite && waited >= timeout)
        {
            result->eax = kWaitTimeout;
            return true;
        }
        const std::uint32_t step =
            timeout == kInfinite ? kPollMilliseconds : (std::min)(kPollMilliseconds, timeout - waited);
        if (!call.services->WaitMilliseconds(step))
        {
            if (error != nullptr) *error = "kernel32 WaitForSingleObject needs a host that can wait";
            return false;
        }
        waited += step;
    }
}

bool WaitForSingleObject(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kWaitObject0 = 0x00000000U;
    constexpr std::uint32_t kWaitTimeout = 0x00000102U;
    constexpr std::uint32_t kWaitFailed = 0xFFFFFFFFU;
    constexpr std::uint32_t kInfinite = 0xFFFFFFFFU;
    if (!CheckArgumentCount(call, result, 2, "kernel32 WaitForSingleObject argument shape is invalid", error))
    {
        return false;
    }
    GuestProcess* process = RequireProcess(call, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t handle = call.arguments[0];
    const std::uint32_t timeout = call.arguments[1];
    if (process->FindChildProcess(handle) != nullptr)
    {
        return WaitForChildProcess(call, process, handle, timeout, result, error);
    }
    if (process->FindEvent(handle) == nullptr && process->FindThreadHandle(handle) == nullptr)
    {
        result->eax = kWaitFailed;
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return true;
    }
    std::uint32_t waited = 0;
    for (;;)
    {
        // Looked up again after every wait: another thread may have closed it.
        GuestEvent* event = process->FindEvent(handle);
        const GuestThread* thread = process->FindThreadHandle(handle);
        if (event != nullptr && event->signaled)
        {
            if (!event->manual_reset)
            {
                event->signaled = false;
            }
            result->eax = kWaitObject0;
            return true;
        }
        if (thread != nullptr && thread->finished)
        {
            result->eax = kWaitObject0;
            return true;
        }
        if (event == nullptr && thread == nullptr)
        {
            result->eax = kWaitFailed;
            call.services->SetLastError(kWin32ErrorInvalidHandle);
            return true;
        }
        if (timeout != kInfinite && waited >= timeout)
        {
            result->eax = kWaitTimeout;
            return true;
        }
        if (process->running_threads() == 0)
        {
            if (timeout == kInfinite)
            {
                if (error != nullptr) *error = "kernel32 WaitForSingleObject would block the only guest thread";
                return false;
            }
            if (!call.services->WaitMilliseconds(timeout - waited))
            {
                if (error != nullptr) *error = "kernel32 WaitForSingleObject needs a host that can wait";
                return false;
            }
            waited = timeout;
            continue;
        }
        if (!call.services->WaitMilliseconds(1))
        {
            if (error != nullptr) *error = "kernel32 WaitForSingleObject needs a host that can wait";
            return false;
        }
        ++waited;
    }
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

// QueryPerformanceFrequency(lpFrequency): TRUE with the 10 MHz counter
// frequency of Windows 11, as measured (task 434), the last error left
// alone. The 6th's 1st child asks for the frequency alone; the counter
// itself waits for a guest that reads it. A null pointer stops.
bool QueryPerformanceFrequency(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint64_t kFrequency = 10000000;
    if (!CheckArgumentCount(call, result, 1, "kernel32 QueryPerformanceFrequency argument shape is invalid", error))
    {
        return false;
    }
    if (call.arguments[0] == 0)
    {
        if (error != nullptr) *error = "kernel32 QueryPerformanceFrequency with a null pointer is not modelled";
        return false;
    }
    std::array<std::uint8_t, 8> bytes = {};
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(kFrequency >> (index * 8));
    }
    if (!PutGuestBytes(call, call.arguments[0], bytes, error))
    {
        return false;
    }
    result->eax = 1;
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
// Whether a handle names nothing the guest could transfer through: no open
// file or device, event, thread or process handle. INVALID_HANDLE_VALUE counts
// as nothing although it is also GetCurrentProcess's pseudo-handle, because
// the file and serial functions treat it that way.
bool NamesNoGuestObject(const ImportCall& call, std::uint32_t handle)
{
    GuestFiles* files = call.services->Files();
    GuestDeviceSet* devices = call.services->Devices();
    GuestProcess* process = call.services->Process();
    const bool is_object = (files != nullptr && files->IsOpen(handle)) ||
                           (devices != nullptr && devices->IsOpen(handle)) ||
                           (process != nullptr && handle != GuestProcess::kCurrentProcessHandle &&
                            (process->IsProcessHandle(handle) || process->FindEvent(handle) != nullptr ||
                             process->FindThreadHandle(handle) != nullptr));
    return !is_object;
}

// ReadFile or WriteFile with an OVERLAPPED on a handle that names nothing:
// FALSE with ERROR_INVALID_HANDLE, zero bytes through the count, and the
// OVERLAPPED's Internal set to STATUS_PENDING, as measured on Windows 11 for
// INVALID_HANDLE_VALUE and a stale handle (task 428).
bool FailOverlappedTransfer(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kStatusPending = 0x00000103U;
    std::array<std::uint8_t, 4> value = {};
    if (call.arguments[3] != 0 && !PutGuestBytes(call, call.arguments[3], value, error))
    {
        return false;
    }
    StoreDword(value, 0, kStatusPending);
    if (!PutGuestBytes(call, call.arguments[4], value, error))
    {
        return false;
    }
    result->eax = 0;
    call.services->SetLastError(kWin32ErrorInvalidHandle);
    return true;
}

bool ReadFile(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckArgumentCount(call, result, 5, "kernel32 ReadFile argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    if (call.arguments[4] != 0)
    {
        if (NamesNoGuestObject(call, call.arguments[0]))
        {
            return FailOverlappedTransfer(call, result, error);
        }
        if (error != nullptr) *error = "kernel32 ReadFile with OVERLAPPED on an open handle is not modelled";
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
        if (NamesNoGuestObject(call, call.arguments[0]))
        {
            return FailOverlappedTransfer(call, result, error);
        }
        if (error != nullptr) *error = "kernel32 WriteFile with OVERLAPPED on an open handle is not modelled";
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

// The serial port functions. No COM port opens: CreateFileA("COM1") fails with
// ERROR_FILE_NOT_FOUND, as on a Windows 11 machine without one. So a guest
// reaches these only with a handle that names nothing, as EZ2Dancer 2nd MOVE
// does when it goes on using its failed COM1 handle. As measured on Windows 11,
// each then returns FALSE with ERROR_INVALID_HANDLE and leaves its outputs
// alone (task 428).
bool FailSerialCall(const ImportCall& call,
                    ImportReturn* result,
                    std::size_t count,
                    const char* name,
                    std::string* error)
{
    if (!ReturnZero(call, result, error) || call.arguments.size() != count || call.services == nullptr)
    {
        if (error != nullptr && error->empty()) *error = std::string("kernel32 ") + name + " argument shape is invalid";
        return false;
    }
    if (!NamesNoGuestObject(call, call.arguments[0]))
    {
        if (error != nullptr) *error = std::string("kernel32 ") + name + " on an open handle is not modelled";
        return false;
    }
    call.services->SetLastError(kWin32ErrorInvalidHandle);
    return true;
}

bool SetCommState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 2, "SetCommState", error);
}

bool GetCommState(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 2, "GetCommState", error);
}

bool SetCommTimeouts(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 2, "SetCommTimeouts", error);
}

bool PurgeComm(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 2, "PurgeComm", error);
}

bool SetupComm(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 3, "SetupComm", error);
}

bool SetCommMask(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 2, "SetCommMask", error);
}

bool ClearCommError(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 3, "ClearCommError", error);
}

bool WaitCommEvent(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return FailSerialCall(call, result, 3, "WaitCommEvent", error);
}

// GetOverlappedResult(hFile, lpOverlapped, lpNumberOfBytesTransferred, bWait)
// for an operation still pending, asked without waiting: FALSE with
// ERROR_IO_INCOMPLETE and the count left alone, whatever the handle, as
// measured on Windows 11 (task 428). A failed ReadFile or WriteFile leaves
// its OVERLAPPED that way. Waiting, or a finished operation, is not modelled,
// since no transfer here completes asynchronously.
bool GetOverlappedResult(const ImportCall& call, ImportReturn* result, std::string* error)
{
    constexpr std::uint32_t kStatusPending = 0x00000103U;
    constexpr std::uint32_t kErrorIoIncomplete = 996U;
    if (!CheckArgumentCount(call, result, 4, "kernel32 GetOverlappedResult argument shape is invalid", error) ||
        call.services == nullptr)
    {
        return false;
    }
    std::uint32_t internal[1] = {};
    if (!ReadGuestWords(call, call.arguments[1], internal, error))
    {
        return false;
    }
    if (internal[0] != kStatusPending || call.arguments[3] != 0)
    {
        if (error != nullptr) *error = "kernel32 GetOverlappedResult on a finished or awaited operation is not modelled";
        return false;
    }
    call.services->SetLastError(kErrorIoIncomplete);
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
// signatures).
constexpr ResolveOnlyExport kKernel32ResolveOnly[] = {
    {"GetWindowsDirectoryA", 2},
    {"GlobalMemoryStatus", 1},
    {"TerminateThread", 2},
    {"SetEndOfFile", 1},
    {"SetConsoleCtrlHandler", 2},
    {"RaiseException", 4},
   
    {"TerminateProcess", 2},
   
    {"IsBadCodePtr", 1}, {"GetStringTypeA", 5},
    {"lstrcmpA", 2}, {"lstrlenA", 1}, {"SetEnvironmentVariableA", 2},
    {"CompareStringW", 6}, {"CompareStringA", 6}, {"FlushFileBuffers", 1},
    {"SetStdHandle", 2},
    // EZ2DJ 1st's protection resolves these too (Task 405).
    {"DebugBreak", 0}, {"OutputDebugStringA", 1},
    {"GetUserDefaultLCID", 0}, {"IsValidLocale", 2}, {"IsValidCodePage", 1}, {"EnumSystemLocalesA", 2},
    {"GetLocaleInfoA", 4}, {"GetLocaleInfoW", 4},
    {"FatalAppExitA", 2},
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
    descriptor.exports.push_back(MakeExport("HeapValidate", 3, &HeapValidate));
    descriptor.exports.push_back(MakeExport("GetStartupInfoA", 1, &GetStartupInfoA));
    descriptor.exports.push_back(MakeExport("GetFullPathNameA", 4, &GetFullPathNameA));
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
    descriptor.exports.push_back(MakeExport("UnhandledExceptionFilter", 1, &UnhandledExceptionFilter));
    descriptor.exports.push_back(MakeExport("CreateEventA", 4, &CreateEventA));
    descriptor.exports.push_back(MakeExport("SetEvent", 1, &SetEvent));
    descriptor.exports.push_back(MakeExport("ResetEvent", 1, &ResetEvent));
    descriptor.exports.push_back(MakeExport("WaitForSingleObject", 2, &WaitForSingleObject));
    // A launcher's child processes (task 431).
    descriptor.exports.push_back(MakeExport("CreateProcessA", 10, &CreateProcessA));
    descriptor.exports.push_back(MakeExport("GetExitCodeProcess", 2, &GetExitCodeProcess));
    descriptor.exports.push_back(MakeExport("SetPriorityClass", 2, &SetPriorityClass));
    descriptor.exports.push_back(MakeExport("GetTickCount", 0, &GetTickCount));
    descriptor.exports.push_back(MakeExport("QueryPerformanceFrequency", 1, &QueryPerformanceFrequency));
    descriptor.exports.push_back(MakeExport("GetSystemTime", 1, &GetSystemTime));
    descriptor.exports.push_back(MakeExport("GetLocalTime", 1, &GetLocalTime));
    descriptor.exports.push_back(MakeExport("SystemTimeToFileTime", 2, &SystemTimeToFileTime));
    descriptor.exports.push_back(
        MakeExport("GetTimeZoneInformation", 1, &GetTimeZoneInformation));
    descriptor.exports.push_back(MakeExport("ReadFile", 5, &ReadFile));
    descriptor.exports.push_back(MakeExport("WriteFile", 5, &WriteFile));
    // EZ2Dancer 2nd MOVE's serial port functions (tasks 427 and 428).
    descriptor.exports.push_back(MakeExport("SetCommState", 2, &SetCommState));
    descriptor.exports.push_back(MakeExport("GetCommState", 2, &GetCommState));
    descriptor.exports.push_back(MakeExport("SetCommTimeouts", 2, &SetCommTimeouts));
    descriptor.exports.push_back(MakeExport("PurgeComm", 2, &PurgeComm));
    descriptor.exports.push_back(MakeExport("SetupComm", 3, &SetupComm));
    descriptor.exports.push_back(MakeExport("SetCommMask", 2, &SetCommMask));
    descriptor.exports.push_back(MakeExport("ClearCommError", 3, &ClearCommError));
    descriptor.exports.push_back(MakeExport("GetOverlappedResult", 4, &GetOverlappedResult));
    descriptor.exports.push_back(MakeExport("WaitCommEvent", 3, &WaitCommEvent));
    descriptor.exports.push_back(MakeExport("SetFilePointer", 4, &SetFilePointer));
    descriptor.exports.push_back(MakeExport("GetFileSize", 2, &GetFileSize));
    descriptor.exports.push_back(MakeExport("GetCurrentDirectoryA", 2, &GetCurrentDirectoryA));
    descriptor.exports.push_back(MakeExport("SetCurrentDirectoryA", 1, &SetCurrentDirectoryA));
    descriptor.exports.push_back(MakeExport("FindFirstFileA", 2, &FindFirstFileA));
    descriptor.exports.push_back(MakeExport("FindNextFileA", 2, &FindNextFileA));
    descriptor.exports.push_back(MakeExport("FindClose", 1, &FindClose));
    descriptor.exports.push_back(MakeExport("GetFileAttributesA", 1, &GetFileAttributesA));
    descriptor.exports.push_back(MakeExport("DeleteFileA", 1, &DeleteFileA));
    descriptor.exports.push_back(MakeExport("RtlUnwind", 4, &RtlUnwind));
    descriptor.exports.push_back(MakeExport("GetPrivateProfileIntA", 4, &GetPrivateProfileIntA));
    descriptor.exports.push_back(MakeExport("GetCurrentThread", 0, &GetCurrentThread));
    descriptor.exports.push_back(MakeExport("CreateThread", 6, &CreateThread));
    descriptor.exports.push_back(MakeExport("SetThreadPriority", 2, &SetThreadPriority));
    descriptor.exports.push_back(MakeExport("GetThreadPriority", 1, &GetThreadPriority));
    descriptor.exports.push_back(MakeExport("InitializeCriticalSection", 1, &InitializeCriticalSection));
    descriptor.exports.push_back(MakeExport("EnterCriticalSection", 1, &EnterCriticalSection));
    descriptor.exports.push_back(MakeExport("LeaveCriticalSection", 1, &LeaveCriticalSection));
    descriptor.exports.push_back(MakeExport("DeleteCriticalSection", 1, &DeleteCriticalSection));
    descriptor.exports.push_back(MakeExport("InterlockedIncrement", 1, &InterlockedIncrement));
    descriptor.exports.push_back(MakeExport("InterlockedDecrement", 1, &InterlockedDecrement));
    descriptor.exports.push_back(MakeExport("TlsAlloc", 0, &TlsAlloc));
    descriptor.exports.push_back(MakeExport("TlsFree", 1, &TlsFree));
    descriptor.exports.push_back(MakeExport("TlsGetValue", 1, &TlsGetValue));
    descriptor.exports.push_back(MakeExport("TlsSetValue", 2, &TlsSetValue));
    descriptor.exports.push_back(MakeExport("IsBadReadPtr", 2, &IsBadReadPtr));
    descriptor.exports.push_back(MakeExport("IsBadWritePtr", 2, &IsBadWritePtr));
    descriptor.exports.push_back(MakeExport("Sleep", 1, &Sleep));
    descriptor.exports.push_back(MakeExport("GetPrivateProfileStringA", 6, &GetPrivateProfileStringA));
    descriptor.exports.push_back(MakeExport("GetPrivateProfileSectionNamesA", 3, &GetPrivateProfileSectionNamesA));
    descriptor.exports.push_back(MakeExport("WritePrivateProfileStringA", 4, &WritePrivateProfileStringA));
    AddResolveOnlyExports(&descriptor, kKernel32ResolveOnly);
    // The protection probes for DOS extenders (Phar Lap TNT, Borland 32-bit)
    // with these names; no Windows kernel32 exports them.
    descriptor.absent_exports = {"IsTNT", "Borland32"};
    return descriptor;
}

}  // namespace re2dj::hle::modules
