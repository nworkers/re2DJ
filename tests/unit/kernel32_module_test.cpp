#include "re2dj/hle/modules/kernel32_module.h"

#include <array>
#include <cstring>
#include <initializer_list>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "re2dj/hle/guest_devices.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/hardlock/protocol.h"
#include "re2dj/hle/win32_errors.h"

#include "memory_services.h"
#include "test_support.h"

namespace
{

void CheckDescriptor(re2dj::test::Context& context)
{
    using re2dj::hle::CallingConvention;
    using re2dj::hle::ImportCall;
    using re2dj::hle::ImportReturn;
    using re2dj::runtime::GuestAddress;
    using re2dj::runtime::ImportGate;

    const auto descriptor = re2dj::hle::modules::MakeKernel32ModuleDescriptor();
    std::string error = "stale";
    RE2DJ_CHECK(context,
                re2dj::hle::modules::ValidateGuestModuleDescriptor(descriptor, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK_EQ(context, descriptor.name, std::string("kernel32.dll"));
    RE2DJ_CHECK_EQ(context, descriptor.aliases.size(), std::size_t{1});
    if (descriptor.aliases.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, descriptor.aliases[0], std::string("kernel32"));
    }
    // 107 implemented exports, then 27 the guest only resolves; the first 24
    // are checked here and the rest in kernel32_crt_test.cpp.
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{134});
    if (descriptor.exports.size() != 134)
    {
        return;
    }

    const std::array<std::string, 24> names = {
        "GetModuleHandleA", "GetProcAddress", "GetVersion", "CreateFileA", "ExitProcess",
        "DeviceIoControl", "CloseHandle", "GetLastError", "SetLastError",
        "GetCurrentProcess", "GetCurrentProcessId", "GetEnvironmentVariableA",
        "SetErrorMode", "LoadLibraryA", "FreeLibrary", "GetVersionExA",
        "OpenProcess", "VirtualAlloc", "VirtualFree", "VirtualProtect", "LocalAlloc",
        "LocalFree", "ReadProcessMemory", "WriteProcessMemory"};
    const std::array<std::uint32_t, 24> argument_counts = {1, 2, 0, 7, 1, 8, 1, 0, 1,
                                                           0, 0, 3, 1, 1, 1, 1,
                                                           3, 4, 3, 4, 2, 1, 5, 5};
    for (std::size_t index = 107; index < descriptor.exports.size(); ++index)
    {
        RE2DJ_CHECK(context,
                    descriptor.exports[index].handler == &re2dj::hle::modules::UnimplementedExport);
    }
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        const auto& export_descriptor = descriptor.exports[index];
        RE2DJ_CHECK_EQ(context, export_descriptor.name, names[index]);
        RE2DJ_CHECK(context, !export_descriptor.ordinal.has_value());
        RE2DJ_CHECK(context,
                    export_descriptor.calling_convention == CallingConvention::kStdcall);
        RE2DJ_CHECK_EQ(context, export_descriptor.argument_count, argument_counts[index]);
        RE2DJ_CHECK(context, export_descriptor.handler != nullptr);
    }
    // Names the protection probes that no Windows kernel32 exports.
    RE2DJ_CHECK(context,
                descriptor.absent_exports == std::vector<std::string>({"IsTNT", "Borland32"}));

    ImportGate gate;
    gate.module = descriptor.name;
    gate.address = GuestAddress(0xF1000000U);
    const std::array<std::uint32_t, 8> arguments = {};
    ImportReturn result;

    // The first nine exports need no services; the later ones are checked with
    // services in CheckProcessExports.
    for (std::size_t index = 0; index < 9; ++index)
    {
        gate.name = descriptor.exports[index].name;
        const ImportCall call{gate,
                              std::span<const std::uint32_t>(
                                  arguments.data(), descriptor.exports[index].argument_count)};
        result = {0x12345678U, 0x87654321U};
        RE2DJ_CHECK(context,
                    descriptor.exports[index].handler(call, &result, &error));
        RE2DJ_CHECK(context, error.empty());
        const std::uint32_t expected =
            index == 3   ? (std::numeric_limits<std::uint32_t>::max)()
            : index == 2 ? re2dj::hle::modules::kKernel32GuestVersion
                         : 0U;
        RE2DJ_CHECK_EQ(context, result.eax, expected);
        RE2DJ_CHECK_EQ(context, result.edx, std::uint32_t{0});
        // Only ExitProcess ends the guest process.
        RE2DJ_CHECK_EQ(context, result.exit_process, index == 4);
    }
}

void CheckExitProcess(re2dj::test::Context& context)
{
    using re2dj::hle::ImportCall;
    using re2dj::hle::ImportReturn;

    const auto descriptor = re2dj::hle::modules::MakeKernel32ModuleDescriptor();
    const re2dj::hle::modules::GuestExportDescriptor* exit_process = nullptr;
    for (const auto& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.name == "ExitProcess")
        {
            exit_process = &export_descriptor;
        }
    }
    RE2DJ_CHECK(context, exit_process != nullptr);
    if (exit_process == nullptr)
    {
        return;
    }

    re2dj::runtime::ImportGate gate;
    gate.module = descriptor.name;
    gate.name = exit_process->name;
    const std::array<std::uint32_t, 1> code = {7};
    ImportReturn result;
    std::string error = "stale";
    RE2DJ_CHECK(context,
                exit_process->handler(ImportCall{gate, code}, &result, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK(context, result.exit_process);
    RE2DJ_CHECK_EQ(context, result.exit_code, std::uint32_t{7});

    // The exit code is the only argument; any other shape is rejected.
    const std::array<std::uint32_t, 2> too_many = {7, 8};
    RE2DJ_CHECK(context,
                !exit_process->handler(ImportCall{gate, too_many}, &result, &error));
    RE2DJ_CHECK(context, !error.empty());
}

// Calls a kernel32 export by name with the given arguments.
re2dj::hle::ImportReturn CallExport(re2dj::test::Context& context,
                                    const re2dj::test::MemoryServices& services,
                                    std::string_view name,
                                    std::initializer_list<std::uint32_t> arguments,
                                    bool* handled = nullptr,
                                    std::string* error = nullptr)
{
    return re2dj::test::CallModuleExport(context,
                                         services,
                                         re2dj::hle::modules::MakeKernel32ModuleDescriptor(),
                                         name,
                                         arguments,
                                         handled,
                                         error);
}

void CheckDeviceExports(re2dj::test::Context& context)
{
    using re2dj::hle::modules::kKernel32InvalidHandle;
    namespace hle = re2dj::hle;
    namespace hardlock = re2dj::hle::hardlock;

    hle::GuestDeviceConfig config;
    config.device_path_prefix = "\\\\.\\FEnteDev";
    re2dj::test::MemoryServices services(config);
    constexpr std::uint32_t kName = re2dj::test::MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kBuffer = re2dj::test::MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kReturned = re2dj::test::MemoryServices::kBase + 0x200;

    // A device name the profile provides opens; others fail with the Win32
    // error the Windows host showed for "\\.\NTICE", or not-found for files.
    services.Put(kName, "\\\\.\\NTICE");
    auto result = CallExport(context, services, "CreateFileA", {kName, 0, 0, 0, 3, 0, 0});
    RE2DJ_CHECK_EQ(context, result.eax, kKernel32InvalidHandle);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidName);
    services.Put(kName, "C:\\EZ2DJ\\DATA.BIN");
    result = CallExport(context, services, "CreateFileA", {kName, 0, 0, 0, 3, 0, 0});
    RE2DJ_CHECK_EQ(context, result.eax, kKernel32InvalidHandle);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    services.Put(kName, "\\\\.\\FEnteDev");
    result = CallExport(context, services, "CreateFileA", {kName, 0, 0, 0, 3, 0, 0});
    const std::uint32_t handle = result.eax;
    RE2DJ_CHECK_EQ(context, handle, hle::GuestDeviceSet::kFirstHandle);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorSuccess);

    // The zero-sized initialize succeeds and reports zero bytes.
    services.Byte(kReturned) = 0xAA;
    result = CallExport(context, services, "DeviceIoControl",
                        {handle, hardlock::kHardlockIoctlInitialize, 0, 0, 0, 0, kReturned, 0});
    RE2DJ_CHECK_EQ(context, result.eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kReturned), 0U);

    // An in-place six-byte handshake without replay material keeps the buffer.
    for (std::uint32_t index = 0; index < 6; ++index)
    {
        services.Byte(kBuffer + index) = static_cast<std::uint8_t>(0x10 + index);
    }
    result = CallExport(context, services, "DeviceIoControl",
                        {handle, hardlock::kHardlockIoctlHandshake, kBuffer, 6, kBuffer, 6,
                         kReturned, 0});
    RE2DJ_CHECK_EQ(context, result.eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kReturned), 6U);
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 5), std::uint8_t{0x15});

    // The wrong buffer shape fails with ERROR_INVALID_DATA and zero bytes.
    result = CallExport(context, services, "DeviceIoControl",
                        {handle, hardlock::kHardlockIoctlHandshake, kBuffer, 4, kBuffer, 4,
                         kReturned, 0});
    RE2DJ_CHECK_EQ(context, result.eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidData);
    RE2DJ_CHECK_EQ(context, services.U32(kReturned), 0U);

    // A code outside the device contract, and a handle never opened.
    result = CallExport(context, services, "DeviceIoControl",
                        {handle, 0x00220000U, 0, 0, 0, 0, kReturned, 0});
    RE2DJ_CHECK_EQ(context, result.eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidFunction);
    result = CallExport(context, services, "DeviceIoControl",
                        {0x1234U, hardlock::kHardlockIoctlInitialize, 0, 0, 0, 0, 0, 0});
    RE2DJ_CHECK_EQ(context, result.eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    RE2DJ_CHECK_EQ(context, services.Devices()->activity().total, 3U);

    // Closing twice fails the second time.
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "CloseHandle", {handle}).eax, 1U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "CloseHandle", {handle}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);

    // SetLastError and GetLastError share the services' value.
    CallExport(context, services, "SetLastError", {42});
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "GetLastError", {}).eax, 42U);
}

void CheckProcessExports(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    using re2dj::test::MemoryServices;

    MemoryServices services;
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kInfo = MemoryServices::kBase + 0x100;

    RE2DJ_CHECK_EQ(context, CallExport(context, services, "GetCurrentProcess", {}).eax,
                   re2dj::hle::modules::kKernel32CurrentProcess);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "GetCurrentProcessId", {}).eax,
                   hle::GuestProcess::kProcessId);

    // The guest environment is empty.
    services.Put(kName, "HL_SEARCH");
    auto result = CallExport(context, services, "GetEnvironmentVariableA", {kName, kInfo, 88});
    RE2DJ_CHECK_EQ(context, result.eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorEnvironmentVariableNotFound);

    // SetErrorMode hands back the previous mode.
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "SetErrorMode", {0x8000}).eax, 0U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "SetErrorMode", {0}).eax, 0x8000U);

    // LoadLibraryA finds facade modules only; FreeLibrary accepts only those.
    services.Put(kName, MemoryServices::kKnownModule);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "LoadLibraryA", {kName}).eax,
                   MemoryServices::kModule);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorSuccess);
    services.Put(kName, "wfapi.dll");
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "LoadLibraryA", {kName}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorModuleNotFound);
    RE2DJ_CHECK_EQ(context,
                   CallExport(context, services, "FreeLibrary", {MemoryServices::kModule}).eax,
                   1U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "FreeLibrary", {0x1234}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);

    // GetVersionExA fills both structure sizes with GetVersion's 6.2.9200 NT.
    for (const std::uint32_t size : {re2dj::hle::modules::kKernel32OsVersionInfoSize,
                                     re2dj::hle::modules::kKernel32OsVersionInfoExSize})
    {
        for (std::uint32_t offset = 0; offset < size; ++offset)
        {
            services.Byte(kInfo + offset) = 0xCC;
        }
        services.PutU32(kInfo, size);
        RE2DJ_CHECK_EQ(context, CallExport(context, services, "GetVersionExA", {kInfo}).eax, 1U);
        RE2DJ_CHECK_EQ(context, services.U32(kInfo), size);
        RE2DJ_CHECK_EQ(context, services.U32(kInfo + 4), 6U);
        RE2DJ_CHECK_EQ(context, services.U32(kInfo + 8), 2U);
        RE2DJ_CHECK_EQ(context, services.U32(kInfo + 12), 9200U);
        RE2DJ_CHECK_EQ(context, services.U32(kInfo + 16), 2U);
        RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 20), std::uint8_t{0});
        if (size == re2dj::hle::modules::kKernel32OsVersionInfoExSize)
        {
            RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 154), std::uint8_t{1});
        }
    }
    services.PutU32(kInfo, 100);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "GetVersionExA", {kInfo}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInsufficientBuffer);

    // TerminateThread is only resolved so far: a call fails naming the export.
    bool handled = true;
    std::string error;
    CallExport(context, services, "TerminateThread", {0, 0}, &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("kernel32.dll!TerminateThread") != std::string::npos);
}

void CheckMemoryExports(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    using re2dj::test::MemoryServices;

    hle::GuestDeviceConfig config;
    config.device_path_prefix = "\\\\.\\FEnteDev";
    MemoryServices services(config);
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kOld = MemoryServices::kBase + 0x40;

    // Device and process handles share one space.
    services.Put(kName, "\\\\.\\FEnteDev");
    const std::uint32_t device = CallExport(context, services, "CreateFileA", {kName, 0, 0, 0, 3, 0, 0}).eax;
    const std::uint32_t process =
        CallExport(context, services, "OpenProcess", {0x38, 0, hle::GuestProcess::kProcessId}).eax;
    RE2DJ_CHECK_EQ(context, device, hle::GuestHandleAllocator::kFirstHandle);
    RE2DJ_CHECK_EQ(context, process, hle::GuestHandleAllocator::kFirstHandle + 4);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "OpenProcess", {0x38, 0, 0x1234}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "CloseHandle", {process}).eax, 1U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "CloseHandle", {process}).eax, 0U);

    // VirtualAlloc commits zeroed pages on the allocation granularity.
    services.Byte(MemoryServices::kArenaBase) = 0xAA;
    const std::uint32_t block = CallExport(context, services, "VirtualAlloc",
                                           {0, 0x1800, hle::kMemCommit, hle::kPageReadWrite}).eax;
    RE2DJ_CHECK_EQ(context, block, MemoryServices::kArenaBase);
    RE2DJ_CHECK_EQ(context, services.Byte(block), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualAlloc",
                                       {0, 0x1000, hle::kMemCommit, hle::kPageReadWrite}).eax,
                   MemoryServices::kArenaBase + hle::GuestProcess::kAllocationGranularity);

    // VirtualProtect reports the previous protection and records the new one.
    auto result = CallExport(context, services, "VirtualProtect",
                             {block + 0x10, 0x20, hle::kPageExecuteReadWrite, kOld});
    RE2DJ_CHECK_EQ(context, result.eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kOld), hle::kPageReadWrite);
    CallExport(context, services, "VirtualProtect", {block, 0x2000, hle::kPageReadOnly, kOld});
    RE2DJ_CHECK_EQ(context, services.U32(kOld), hle::kPageExecuteReadWrite);
    // Past the region, copy-on-write on private memory, and a null old slot.
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualProtect",
                                       {block, 0x3000, hle::kPageReadOnly, kOld}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidAddress);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualProtect",
                                       {block, 0x1000, hle::kPageWriteCopy, kOld}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualProtect",
                                       {block, 0x1000, hle::kPageReadOnly, 0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorNoAccess);
    // PAGE_GUARD is not modelled, so the call stops instead of guessing.
    bool handled = true;
    std::string error;
    CallExport(context, services, "VirtualProtect",
               {block, 0x1000, hle::kPageReadWrite | 0x100U, kOld}, &handled, &error);
    RE2DJ_CHECK(context, !handled);

    // Decommitted pages cannot be protected; release needs size 0 at the base.
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualFree",
                                       {block, 0x2000, hle::kMemDecommit}).eax, 1U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualProtect",
                                       {block, 0x1000, hle::kPageReadOnly, kOld}).eax, 0U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualFree",
                                       {block, 0x1000, hle::kMemRelease}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualFree",
                                       {block, 0, hle::kMemRelease}).eax, 1U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "VirtualFree",
                                       {block, 0, hle::kMemRelease}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidAddress);

    // Read/WriteProcessMemory copy within the own process; a write reaches a
    // page recorded read-only, as kernel32 lifts the protection for it.
    const std::uint32_t copy_block = CallExport(context, services, "VirtualAlloc",
                                                {0, 0x1000, hle::kMemCommit, hle::kPageReadOnly}).eax;
    constexpr std::uint32_t kSource = MemoryServices::kBase + 0x80;
    constexpr std::uint32_t kCount = MemoryServices::kBase + 0x90;
    services.PutU32(kSource, 0x11223344U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "WriteProcessMemory",
                                       {hle::GuestProcess::kCurrentProcessHandle, copy_block, kSource, 4, kCount}).eax,
                   1U);
    RE2DJ_CHECK_EQ(context, services.U32(copy_block), 0x11223344U);
    RE2DJ_CHECK_EQ(context, services.U32(kCount), 4U);
    services.PutU32(kSource, 0);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "ReadProcessMemory",
                                       {hle::GuestProcess::kCurrentProcessHandle, copy_block, kSource, 4, 0}).eax,
                   1U);
    RE2DJ_CHECK_EQ(context, services.U32(kSource), 0x11223344U);
    // Memory outside every region copies nothing; an unknown handle fails.
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "ReadProcessMemory",
                                       {hle::GuestProcess::kCurrentProcessHandle, kSource, copy_block, 4, kCount}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPartialCopy);
    RE2DJ_CHECK_EQ(context, services.U32(kCount), 0U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "ReadProcessMemory",
                                       {0x2468, copy_block, kSource, 4, 0}).eax,
                   0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);

    // LocalAlloc hands out heap blocks, zeroed for LMEM_ZEROINIT.
    services.Byte(MemoryServices::kHeapBase) = 0xAA;
    const std::uint32_t local = CallExport(context, services, "LocalAlloc", {0x40, 0x108}).eax;
    RE2DJ_CHECK_EQ(context, local, MemoryServices::kHeapBase);
    RE2DJ_CHECK_EQ(context, services.Byte(local), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "LocalFree", {local}).eax, 0U);
    RE2DJ_CHECK_EQ(context, CallExport(context, services, "LocalFree", {local}).eax, local);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    handled = true;
    CallExport(context, services, "LocalAlloc", {0x2, 0x10}, &handled, &error);
    RE2DJ_CHECK(context, !handled);
}

void CheckGuestVersionEncoding(re2dj::test::Context& context)
{
    // GetVersion packs major/minor in the low word and the build number in the
    // high word, with bit 31 clear on the NT platform.
    constexpr std::uint32_t version = re2dj::hle::modules::kKernel32GuestVersion;
    RE2DJ_CHECK_EQ(context, version & 0xFFU, std::uint32_t{6});
    RE2DJ_CHECK_EQ(context, (version >> 8) & 0xFFU, std::uint32_t{2});
    RE2DJ_CHECK_EQ(context, version >> 16, std::uint32_t{9200});
    RE2DJ_CHECK_EQ(context, version & 0x80000000U, std::uint32_t{0});
}

}  // namespace

// lstrcpynA copies at most n - 1 bytes and a NUL and returns the
// destination; 0 writes nothing, -1 is unbounded, a NULL source gives NULL,
// and the last error never changes (measured on Windows 11).
void CheckLstrcpyn(re2dj::test::Context& context)
{
    using re2dj::test::CallModuleExport;
    using re2dj::test::MemoryServices;
    const auto descriptor = re2dj::hle::modules::MakeKernel32ModuleDescriptor();
    MemoryServices services;
    constexpr std::uint32_t kSource = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kDestination = MemoryServices::kBase + 0x60;
    services.Put(kSource, "ABCDEF");
    services.SetLastError(0x1234);
    const auto fill = [&services]() {
        for (std::uint32_t index = 0; index < 10; ++index)
        {
            services.Byte(kDestination + index) = 0x5A;
        }
    };
    fill();
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "lstrcpynA",
                                             {kDestination, kSource, 0}).eax,
                   kDestination);
    RE2DJ_CHECK_EQ(context, services.Byte(kDestination), std::uint8_t{0x5A});
    CallModuleExport(context, services, descriptor, "lstrcpynA", {kDestination, kSource, 4});
    RE2DJ_CHECK_EQ(context, services.U32(kDestination), 0x00434241U);
    RE2DJ_CHECK_EQ(context, services.Byte(kDestination + 4), std::uint8_t{0x5A});
    fill();
    CallModuleExport(context, services, descriptor, "lstrcpynA", {kDestination, kSource, 0xFFFFFFFFU});
    RE2DJ_CHECK_EQ(context, services.Byte(kDestination + 5), std::uint8_t{'F'});
    RE2DJ_CHECK_EQ(context, services.Byte(kDestination + 6), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, services.Byte(kDestination + 7), std::uint8_t{0x5A});
    RE2DJ_CHECK_EQ(context, CallModuleExport(context, services, descriptor, "lstrcpynA",
                                             {kDestination, 0, 5}).eax,
                   0U);
    // An empty source gives an empty destination.
    services.Put(kSource, "");
    CallModuleExport(context, services, descriptor, "lstrcpynA", {kDestination, kSource, 5});
    RE2DJ_CHECK_EQ(context, services.Byte(kDestination), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, services.LastError(), 0x1234U);
}

void RunKernel32ModuleTests(re2dj::test::Context& context)
{
    CheckLstrcpyn(context);
    CheckDescriptor(context);
    CheckGuestVersionEncoding(context);
    CheckExitProcess(context);
    CheckDeviceExports(context);
    CheckProcessExports(context);
    CheckMemoryExports(context);
}
