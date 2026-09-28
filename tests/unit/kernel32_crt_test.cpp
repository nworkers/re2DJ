#include "re2dj/hle/modules/kernel32_module.h"

#include <array>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/guest_heap.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/win32_errors.h"
#include "re2dj/hle/win32_time.h"
#include "re2dj/target/target_profile.h"

#include "memory_services.h"
#include "test_support.h"

namespace
{

using re2dj::test::MemoryServices;

re2dj::hle::ImportReturn Call(re2dj::test::Context& context,
                              const MemoryServices& services,
                              std::string_view name,
                              std::initializer_list<std::uint32_t> arguments,
                              bool* handled = nullptr)
{
    return re2dj::test::CallModuleExport(context,
                                         services,
                                         re2dj::hle::modules::MakeKernel32ModuleDescriptor(),
                                         name,
                                         arguments,
                                         handled);
}

std::string ReadText(MemoryServices& services, std::uint32_t address)
{
    std::string text;
    for (std::uint32_t at = address; services.Byte(at) != 0; ++at)
    {
        text.push_back(static_cast<char>(services.Byte(at)));
    }
    return text;
}

void CheckGuestHeap(re2dj::test::Context& context)
{
    re2dj::hle::GuestHeap heap(0x1000, 0x40);
    const std::uint32_t first = heap.Allocate(3);
    const std::uint32_t second = heap.Allocate(8);
    RE2DJ_CHECK_EQ(context, first, 0x1000U);
    RE2DJ_CHECK_EQ(context, second, 0x1008U);
    // HeapSize reports the requested size, not the rounded one.
    RE2DJ_CHECK(context, heap.BlockSize(first) == std::uint32_t{3});
    // A block grows in place up to its neighbour, and the last block to the end.
    RE2DJ_CHECK(context, heap.ResizeInPlace(first, 8));
    RE2DJ_CHECK(context, !heap.ResizeInPlace(first, 9));
    RE2DJ_CHECK(context, heap.ResizeInPlace(second, 0x38));
    RE2DJ_CHECK(context, !heap.ResizeInPlace(second, 0x39));
    RE2DJ_CHECK(context, !heap.ResizeInPlace(0x1004, 4));
}

void CheckHeapExports(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    MemoryServices services;

    // A growable heap reserves 128 MiB, more than the test arena holds.
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapCreate", {1, 0x1000, 0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorNotEnoughMemory);
    const std::uint32_t heap = Call(context, services, "HeapCreate", {1, 0x1000, 0x10000}).eax;
    RE2DJ_CHECK_EQ(context, heap, MemoryServices::kArenaBase);

    services.Byte(heap) = 0xAA;
    const std::uint32_t zeroed = Call(context, services, "HeapAlloc", {heap, 8, 0x10}).eax;
    RE2DJ_CHECK_EQ(context, zeroed, heap);
    RE2DJ_CHECK_EQ(context, services.Byte(zeroed), std::uint8_t{0});
    const std::uint32_t next = Call(context, services, "HeapAlloc", {heap, 0, 4}).eax;
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapSize", {heap, 0, next}).eax, 4U);

    // Growing a block boxed in by its neighbour moves it and keeps its bytes.
    services.PutU32(zeroed, 0x11223344U);
    const std::uint32_t moved = Call(context, services, "HeapReAlloc", {heap, 8, zeroed, 0x20}).eax;
    RE2DJ_CHECK(context, moved != zeroed && moved != 0);
    RE2DJ_CHECK_EQ(context, services.U32(moved), 0x11223344U);
    RE2DJ_CHECK_EQ(context, services.U32(moved + 0x10), 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapSize", {heap, 0, zeroed}).eax, 0xFFFFFFFFU);
    // In-place-only fails when the block cannot grow where it is.
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapReAlloc", {heap, 0x10, next, 0x40}).eax, 0U);

    // HeapValidate, as measured: the whole heap or a block's start is valid;
    // inside a block, a freed block, or another heap's block is not; the
    // flags are not checked and the last error stays.
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapValidate", {heap, 0, 0}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapValidate", {heap, 0x10000, moved}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapValidate", {heap, 0, moved + 8}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapValidate", {heap, 0, zeroed}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    const std::uint32_t process_block = Call(context, services, "HeapAlloc",
                                             {services.Process()->process_heap(), 0, 8}).eax;
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapValidate", {heap, 0, process_block}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapValidate",
                                 {services.Process()->process_heap(), 0, process_block}).eax, 1U);
    bool unknown_handled = true;
    Call(context, services, "HeapValidate", {0x1234, 0, 0}, &unknown_handled);
    RE2DJ_CHECK(context, !unknown_handled);

    // The process heap is reachable by its base; freeing NULL succeeds.
    const std::uint32_t process_heap = services.Process()->process_heap();
    RE2DJ_CHECK(context, Call(context, services, "HeapAlloc", {process_heap, 0, 8}).eax != 0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapFree", {heap, 0, 0}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapFree", {heap, 0, next}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapFree", {heap, 0, next}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapDestroy", {heap}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "HeapDestroy", {process_heap}).eax, 0U);

    // HEAP_GENERATE_EXCEPTIONS is not modelled.
    bool handled = true;
    Call(context, services, "HeapAlloc", {process_heap, 4, 8}, &handled);
    RE2DJ_CHECK(context, !handled);
}

void CheckStartupExports(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    MemoryServices services;
    services.Process()->SetMainImage(0x00400000U, "D:\\ez2dj\\EZ2DJ.EXE");
    constexpr std::uint32_t kBuffer = MemoryServices::kBase + 0x100;

    for (std::uint32_t offset = 0; offset < 68; ++offset)
    {
        services.Byte(kBuffer + offset) = 0xCC;
    }
    Call(context, services, "GetStartupInfoA", {kBuffer});
    RE2DJ_CHECK_EQ(context, services.U32(kBuffer), 68U);
    RE2DJ_CHECK_EQ(context, services.U32(kBuffer + 44), 0U);

    // A GUI process has no standard handles, and they have no file type.
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetStdHandle", {0xFFFFFFF5U}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetStdHandle", {7}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetFileType", {0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetHandleCount", {32}).eax, 32U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetCurrentThreadId", {}).eax,
                   hle::GuestProcess::kThreadId);

    // The command line quotes the module path and is placed once.
    const std::uint32_t command = Call(context, services, "GetCommandLineA", {}).eax;
    RE2DJ_CHECK_EQ(context, ReadText(services, command), std::string("\"D:\\ez2dj\\EZ2DJ.EXE\""));
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetCommandLineA", {}).eax, command);

    // The environment block is empty: only its terminator.
    const std::uint32_t wide = Call(context, services, "GetEnvironmentStringsW", {}).eax;
    RE2DJ_CHECK_EQ(context, services.U32(wide), 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "FreeEnvironmentStringsW", {wide}).eax, 1U);
    const std::uint32_t narrow = Call(context, services, "GetEnvironmentStrings", {}).eax;
    RE2DJ_CHECK_EQ(context, services.Byte(narrow + 1), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, Call(context, services, "FreeEnvironmentStringsA", {narrow}).eax, 1U);

    // GetModuleHandleA(NULL) and GetModuleFileNameA name the main image; a
    // facade module sits in system32; a short buffer truncates.
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetModuleHandleA", {0}).eax, 0x00400000U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetModuleFileNameA", {0, kBuffer, 260}).eax, 18U);
    RE2DJ_CHECK_EQ(context, ReadText(services, kBuffer), std::string("D:\\ez2dj\\EZ2DJ.EXE"));
    Call(context, services, "GetModuleFileNameA", {MemoryServices::kModule, kBuffer, 260});
    RE2DJ_CHECK_EQ(context, ReadText(services, kBuffer), std::string("C:\\WINDOWS\\system32\\advapi32.dll"));
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetModuleFileNameA", {0, kBuffer, 4}).eax, 4U);
    RE2DJ_CHECK_EQ(context, ReadText(services, kBuffer), std::string("D:\\"));
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInsufficientBuffer);

    RE2DJ_CHECK_EQ(context, Call(context, services, "IsProcessorFeaturePresent", {0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "IsProcessorFeaturePresent", {10}).eax, 1U);
    bool handled = true;
    Call(context, services, "IsProcessorFeaturePresent", {23}, &handled);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetUnhandledExceptionFilter", {0x004cb603U}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetUnhandledExceptionFilter", {0}).eax, 0x004cb603U);
}

// Code page 949 as measured on a Korean Windows 11 host.
void CheckCodePage(re2dj::test::Context& context)
{
    MemoryServices services;
    constexpr std::uint32_t kBytes = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kWide = MemoryServices::kBase + 0x200;
    constexpr std::uint32_t kInfo = MemoryServices::kBase + 0x300;

    RE2DJ_CHECK_EQ(context, Call(context, services, "GetACP", {}).eax, 949U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetOEMCP", {}).eax, 949U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetCPInfo", {949, kInfo}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kInfo), 2U);
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 4), std::uint8_t{'?'});
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 6), std::uint8_t{0x81});
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 7), std::uint8_t{0xFE});
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetCPInfo", {1252, kInfo}).eax, 0U);

    // Single bytes: ASCII, 0x80 -> U+0080, 0xFF -> U+F8F7, and back.
    services.Byte(kBytes) = 'A';
    services.Byte(kBytes + 1) = 0x80;
    services.Byte(kBytes + 2) = 0xFF;
    RE2DJ_CHECK_EQ(context, Call(context, services, "MultiByteToWideChar", {949, 1, kBytes, 3, 0, 0}).eax, 3U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "MultiByteToWideChar", {949, 1, kBytes, 3, kWide, 3}).eax, 3U);
    RE2DJ_CHECK_EQ(context, services.U32(kWide), 0x00800041U);
    RE2DJ_CHECK_EQ(context, services.U32(kWide + 4) & 0xFFFFU, 0xF8F7U);
    services.PutU32(kBytes, 0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WideCharToMultiByte", {0, 0, kWide, 3, kBytes, 3, 0, 0}).eax, 3U);
    RE2DJ_CHECK_EQ(context, services.U32(kBytes) & 0xFFFFFFU, 0xFF8041U);
    // A lead byte starts double-byte text, which is not modelled.
    services.Byte(kBytes) = 0xB0;
    bool handled = true;
    Call(context, services, "MultiByteToWideChar", {949, 1, kBytes, 1, kWide, 1}, &handled);
    RE2DJ_CHECK(context, !handled);

    // CT_CTYPE1 for ' ', 'A', 'g', '0', NUL, TAB, U+0080, U+F8F7 as measured.
    const std::array<std::uint16_t, 8> characters = {' ', 'A', 'g', '0', 0, '\t', 0x0080, 0xF8F7};
    const std::array<std::uint16_t, 8> expected = {0x248, 0x381, 0x302, 0x284, 0x220, 0x268, 0x220, 0x200};
    for (std::size_t index = 0; index < characters.size(); ++index)
    {
        services.Byte(kWide + index * 2) = static_cast<std::uint8_t>(characters[index]);
        services.Byte(kWide + index * 2 + 1) = static_cast<std::uint8_t>(characters[index] >> 8);
    }
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetStringTypeW", {1, kWide, 8, kInfo}).eax, 1U);
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        const std::uint16_t type = static_cast<std::uint16_t>(
            services.Byte(kInfo + index * 2) | (services.Byte(kInfo + index * 2 + 1) << 8));
        RE2DJ_CHECK_EQ(context, type, expected[index]);
    }

    // Case mapping changes only ASCII letters.
    RE2DJ_CHECK_EQ(context, Call(context, services, "LCMapStringW", {0x412, 0x200, kWide, 8, kInfo, 8}).eax, 8U);
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 4), std::uint8_t{'G'});
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 14), std::uint8_t{0xF7});
    services.Byte(kBytes) = 'q';
    services.Byte(kBytes + 1) = 0xFF;
    RE2DJ_CHECK_EQ(context, Call(context, services, "LCMapStringA", {0, 0x200, kBytes, 2, kInfo, 2}).eax, 2U);
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo), std::uint8_t{'Q'});
    RE2DJ_CHECK_EQ(context, services.Byte(kInfo + 1), std::uint8_t{0xFF});
}

void CheckEvents(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    constexpr std::uint32_t kWaitObject0 = 0;
    constexpr std::uint32_t kWaitTimeout = 0x102;
    MemoryServices services;

    // An auto-reset event created signalled satisfies one wait, then times out.
    const std::uint32_t automatic = Call(context, services, "CreateEventA", {0, 0, 1, 0}).eax;
    RE2DJ_CHECK_EQ(context, automatic, hle::GuestHandleAllocator::kFirstHandle);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {automatic, 1000}).eax, kWaitObject0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {automatic, 0}).eax, kWaitTimeout);
    // A manual-reset event stays signalled until reset.
    const std::uint32_t manual = Call(context, services, "CreateEventA", {0, 1, 0, 0}).eax;
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetEvent", {manual}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {manual, 0}).eax, kWaitObject0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {manual, 0}).eax, kWaitObject0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "ResetEvent", {manual}).eax, 1U);
    // Blocking the only guest thread is not answered.
    bool handled = true;
    Call(context, services, "WaitForSingleObject", {manual, 0xFFFFFFFFU}, &handled);
    RE2DJ_CHECK(context, !handled);
    // CloseHandle closes events; waits and signals on closed handles fail.
    RE2DJ_CHECK_EQ(context, Call(context, services, "CloseHandle", {manual}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetEvent", {manual}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {manual, 0}).eax, 0xFFFFFFFFU);
    // Named events are not modelled.
    handled = true;
    Call(context, services, "CreateEventA", {0, 0, 0, MemoryServices::kBase}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// CreateThread, thread priorities, waits on threads, and a critical section
// two threads share, as measured on Windows 11 (design 417).
void CheckThreads(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    constexpr std::uint32_t kWaitObject0 = 0;
    constexpr std::uint32_t kWaitTimeout = 0x102;
    constexpr std::uint32_t kIdOut = MemoryServices::kBase + 0x3000;
    constexpr std::uint32_t kSection = MemoryServices::kBase + 0x3100;
    constexpr std::uint32_t kFirst = hle::GuestProcess::kThreadId + 4;
    constexpr std::uint32_t kSecond = hle::GuestProcess::kThreadId + 8;
    MemoryServices services;
    services.SetLastError(1234);

    // A handle and the next thread ID; the last error stays.
    const std::uint32_t first = Call(context, services, "CreateThread", {0, 0, 0x401000, 0x77, 0, kIdOut}).eax;
    RE2DJ_CHECK_EQ(context, first, hle::GuestHandleAllocator::kFirstHandle);
    RE2DJ_CHECK_EQ(context, services.U32(kIdOut), kFirst);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, services.started_threads.size(), std::size_t{1});
    if (services.started_threads.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, services.started_threads[0].start, 0x401000U);
        RE2DJ_CHECK_EQ(context, services.started_threads[0].parameter, 0x77U);
        RE2DJ_CHECK_EQ(context, services.started_threads[0].thread_id, kFirst);
    }
    const std::uint32_t second = Call(context, services, "CreateThread", {0, 0, 0x401000, 5, 0, 0}).eax;
    RE2DJ_CHECK_EQ(context, second, first + 4);
    // No ThreadProc, a suspended start, or a host without threads stop.
    bool handled = true;
    Call(context, services, "CreateThread", {0, 0, 0, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    handled = true;
    Call(context, services, "CreateThread", {0, 0, 0x401000, 0, 4, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.refuse_threads = true;
    handled = true;
    Call(context, services, "CreateThread", {0, 0, 0x401000, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.refuse_threads = false;

    // Priorities: THREAD_PRIORITY_* only, on a thread handle or the
    // pseudo-handle (here the main thread's).
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {first, 1}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetThreadPriority", {first}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {first, 15}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {first, 0xFFFFFFF1U}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {first, 3}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {first, 99}).eax, 0U);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {0x1234, 1}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetThreadPriority", {0x1234}).eax, 0x7FFFFFFFU);
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {0xFFFFFFFEU, 2}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetThreadPriority", {0xFFFFFFFEU}).eax, 2U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetThreadPriority", {first}).eax, 0xFFFFFFF1U);

    // A running thread times out at once; a wait goes on in 1 ms steps until
    // it finishes, or until the timeout has passed.
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {first, 0}).eax, kWaitTimeout);
    services.on_wait = [&]() {
        if (services.waits.size() == 3)
        {
            services.Process()->FinishThread(kFirst, 119);
        }
    };
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {first, 5000}).eax, kWaitObject0);
    RE2DJ_CHECK(context, services.waits == (std::vector<std::uint32_t>{1, 1, 1}));
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {first, 0}).eax, kWaitObject0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {second, 2}).eax, kWaitTimeout);
    RE2DJ_CHECK_EQ(context, services.waits.size(), std::size_t{5});
    // A finished thread's handle still takes a priority.
    RE2DJ_CHECK_EQ(context, Call(context, services, "SetThreadPriority", {first, 1}).eax, 1U);

    // A section the second thread owns is waited for until it leaves.
    Call(context, services, "InitializeCriticalSection", {kSection});
    services.thread_id = kSecond;
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetCurrentThreadId", {}).eax, kSecond);
    Call(context, services, "EnterCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 12), kSecond);
    services.thread_id = hle::GuestProcess::kThreadId;
    handled = true;
    Call(context, services, "LeaveCriticalSection", {kSection}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.on_wait = [&]() {
        services.PutU32(kSection + 4, 0xFFFFFFFFU);
        services.PutU32(kSection + 8, 0);
        services.PutU32(kSection + 12, 0);
    };
    Call(context, services, "EnterCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 12), hle::GuestProcess::kThreadId);
    RE2DJ_CHECK_EQ(context, services.waits.size(), std::size_t{6});

    // Closing a handle leaves the thread; a closed handle is invalid.
    RE2DJ_CHECK_EQ(context, Call(context, services, "CloseHandle", {first}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "CloseHandle", {first}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {first, 0}).eax, 0xFFFFFFFFU);

    // With no other thread running nothing can signal: a timeout is waited
    // out in one wait, and a section another thread still owns stops.
    services.Process()->FinishThread(kSecond, 0);
    services.on_wait = nullptr;
    services.waits.clear();
    const std::uint32_t event = Call(context, services, "CreateEventA", {0, 1, 0, 0}).eax;
    RE2DJ_CHECK_EQ(context, Call(context, services, "WaitForSingleObject", {event, 30}).eax, kWaitTimeout);
    RE2DJ_CHECK(context, services.waits == (std::vector<std::uint32_t>{30}));
    services.PutU32(kSection + 4, 0xFFFFFFFEU);
    services.PutU32(kSection + 12, kSecond);
    handled = true;
    Call(context, services, "EnterCriticalSection", {kSection}, &handled);
    RE2DJ_CHECK(context, !handled);
}

void CheckTimeConversion(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    // 1601-01-01 (a Monday) is FILETIME 0; 1970-01-01 (a Thursday) the Unix epoch.
    const hle::Win32SystemTime origin = hle::FileTimeToSystemTime(0);
    RE2DJ_CHECK_EQ(context, origin.year, std::uint16_t{1601});
    RE2DJ_CHECK_EQ(context, origin.day_of_week, std::uint16_t{1});
    const hle::Win32SystemTime epoch = hle::FileTimeToSystemTime(hle::kFileTimeUnixEpoch);
    RE2DJ_CHECK_EQ(context, epoch.year, std::uint16_t{1970});
    RE2DJ_CHECK_EQ(context, epoch.month, std::uint16_t{1});
    RE2DJ_CHECK_EQ(context, epoch.day, std::uint16_t{1});
    RE2DJ_CHECK_EQ(context, epoch.day_of_week, std::uint16_t{4});
    // 2024-02-29 13:45:06.789 (a Thursday) round-trips.
    const hle::Win32SystemTime leap = {2024, 2, 4, 29, 13, 45, 6, 789};
    const std::optional<std::uint64_t> file_time = hle::SystemTimeToFileTime(leap);
    RE2DJ_CHECK(context, file_time.has_value());
    if (file_time.has_value())
    {
        const hle::Win32SystemTime back = hle::FileTimeToSystemTime(*file_time);
        RE2DJ_CHECK_EQ(context, back.day, std::uint16_t{29});
        RE2DJ_CHECK_EQ(context, back.day_of_week, std::uint16_t{4});
        RE2DJ_CHECK_EQ(context, back.millisecond, std::uint16_t{789});
        RE2DJ_CHECK_EQ(context, (*file_time - hle::kFileTimeUnixEpoch) / hle::kFileTimeTicksPerSecond,
                       std::uint64_t{1709214306});
    }
    RE2DJ_CHECK(context, !hle::SystemTimeToFileTime({2023, 2, 0, 29, 0, 0, 0, 0}).has_value());
    RE2DJ_CHECK(context, !hle::SystemTimeToFileTime({2023, 13, 0, 1, 0, 0, 0, 0}).has_value());
}

void CheckClockExports(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    MemoryServices services;
    constexpr std::uint32_t kTime = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kFileTime = MemoryServices::kBase + 0x140;
    constexpr std::uint32_t kZone = MemoryServices::kBase + 0x200;

    // Without a clock the time exports stop.
    bool handled = true;
    Call(context, services, "GetTickCount", {}, &handled);
    RE2DJ_CHECK(context, !handled);

    // 2026-09-24 23:30:00 UTC is 2026-09-25 08:30:00 in Korea (+540).
    const std::uint64_t utc = hle::kFileTimeUnixEpoch + 1790292600ULL * hle::kFileTimeTicksPerSecond;
    services.SetClock({utc, 540, 123456});
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetTickCount", {}).eax, 123456U);
    Call(context, services, "GetSystemTime", {kTime});
    RE2DJ_CHECK_EQ(context, services.U32(kTime), 0x00090000U | 2026U);
    RE2DJ_CHECK_EQ(context, services.Byte(kTime + 6), std::uint8_t{24});
    RE2DJ_CHECK_EQ(context, services.Byte(kTime + 8), std::uint8_t{23});
    Call(context, services, "GetLocalTime", {kTime});
    RE2DJ_CHECK_EQ(context, services.Byte(kTime + 6), std::uint8_t{25});
    RE2DJ_CHECK_EQ(context, services.Byte(kTime + 8), std::uint8_t{8});
    // A Friday.
    RE2DJ_CHECK_EQ(context, services.Byte(kTime + 4), std::uint8_t{5});
    RE2DJ_CHECK_EQ(context, Call(context, services, "SystemTimeToFileTime", {kTime, kFileTime}).eax, 1U);
    const std::uint64_t local = services.U32(kFileTime) |
                                (static_cast<std::uint64_t>(services.U32(kFileTime + 4)) << 32);
    RE2DJ_CHECK_EQ(context, local - utc, 540ULL * 60ULL * hle::kFileTimeTicksPerSecond);
    services.Byte(kTime + 2) = 13;
    RE2DJ_CHECK_EQ(context, Call(context, services, "SystemTimeToFileTime", {kTime, kFileTime}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);

    // The zone: bias -540 (UTC = local - 9 h), no daylight rule, no names.
    for (std::uint32_t offset = 0; offset < 172; ++offset)
    {
        services.Byte(kZone + offset) = 0xCC;
    }
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetTimeZoneInformation", {kZone}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kZone), static_cast<std::uint32_t>(-540));
    RE2DJ_CHECK_EQ(context, services.U32(kZone + 4), 0U);
    RE2DJ_CHECK_EQ(context, services.Byte(kZone + 171), std::uint8_t{0});
}

void CheckGuestPaths(re2dj::test::Context& context)
{
    re2dj::target::TargetProfile profile;
    profile.executable_relative_path = "EZ2DJ/EZ2DJ.EXE";
    RE2DJ_CHECK_EQ(context, re2dj::target::GuestRootPath(profile), std::string("D:\\ez2dj"));
    RE2DJ_CHECK_EQ(context, re2dj::target::GuestExecutablePath(profile),
                   std::string("D:\\ez2dj\\EZ2DJ.EXE"));
    profile.guest_drive_letter = 'C';
    profile.guest_directory = "\\game";
    RE2DJ_CHECK_EQ(context, re2dj::target::GuestExecutablePath(profile),
                   std::string("C:\\game\\EZ2DJ.EXE"));
}

// RtlUnwind as measured on Windows 11: the frames above the target are
// handed to their handlers innermost first with the record flagged
// EXCEPTION_UNWINDING (STATUS_UNWIND at the return address when none is
// given), then unlinked; ReturnValue comes back in eax.
void CheckRtlUnwind(re2dj::test::Context& context)
{
    const auto descriptor = re2dj::hle::modules::MakeKernel32ModuleDescriptor();
    MemoryServices services;
    constexpr std::uint32_t kTeb = MemoryServices::kBase + 0x2000;
    constexpr std::uint32_t kFrame1 = MemoryServices::kBase + 0x3000;
    constexpr std::uint32_t kFrame2 = kFrame1 + 0x40;
    constexpr std::uint32_t kFrame3 = kFrame1 + 0x80;
    constexpr std::uint32_t kRecord = MemoryServices::kBase + 0x3100;
    const auto link = [&]() {
        services.PutU32(kTeb, kFrame1);
        services.PutU32(kFrame1, kFrame2);
        services.PutU32(kFrame1 + 4, 0x00401000);
        services.PutU32(kFrame2, kFrame3);
        services.PutU32(kFrame2 + 4, 0x00402000);
        services.PutU32(kFrame3, 0xFFFFFFFFU);
        services.PutU32(kFrame3 + 4, 0x00403000);
    };
    services.teb = kTeb;
    services.return_address = 0x00405678;
    std::vector<std::uint32_t> seen_codes;
    std::vector<std::uint32_t> seen_flags;
    std::vector<std::uint32_t> seen_addresses;
    services.guest_function = [&](const std::vector<std::uint32_t>& arguments) {
        seen_codes.push_back(services.U32(arguments[0]));
        seen_flags.push_back(services.U32(arguments[0] + 4));
        seen_addresses.push_back(services.U32(arguments[0] + 12));
        return 1U;  // ExceptionContinueSearch
    };

    link();
    const auto call = [&](std::initializer_list<std::uint32_t> arguments, bool* handled = nullptr) {
        return re2dj::test::CallModuleExport(context, services, descriptor, "RtlUnwind", arguments, handled).eax;
    };
    RE2DJ_CHECK_EQ(context, call({kFrame3, 0x00409999, 0, 0x1234}), 0x1234U);
    RE2DJ_CHECK_EQ(context, services.U32(kTeb), kFrame3);
    RE2DJ_CHECK_EQ(context, services.guest_calls.size(), std::size_t{2});
    if (services.guest_calls.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, services.guest_calls[0][1], kFrame1);
        RE2DJ_CHECK_EQ(context, services.guest_calls[1][1], kFrame2);
        RE2DJ_CHECK_EQ(context, seen_codes[0], 0xC0000027U);
        RE2DJ_CHECK_EQ(context, seen_flags[0], 2U);
        RE2DJ_CHECK_EQ(context, seen_addresses[1], 0x00405678U);
    }
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), std::size_t{0});

    // A record given is flagged in place and handed on.
    link();
    services.guest_calls.clear();
    services.PutU32(kRecord, 0xC0000005U);
    services.PutU32(kRecord + 4, 0);
    call({kFrame3, 0, kRecord, 0});
    RE2DJ_CHECK_EQ(context, services.U32(kRecord + 4), 2U);
    RE2DJ_CHECK_EQ(context, services.guest_calls.size(), std::size_t{2});
    if (!services.guest_calls.empty())
    {
        RE2DJ_CHECK_EQ(context, services.guest_calls[0][0], kRecord);
    }

    // Not modelled: an exit unwind, and a target not on the list.
    link();
    bool handled = true;
    call({0, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
    link();
    handled = true;
    call({kRecord, 0, 0, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// Critical sections, TLS, Interlocked, GetCurrentThread, and IsBad*Ptr as
// measured on Windows 11 (design 406).
void CheckCrtStartupExports(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    MemoryServices services;
    constexpr std::uint32_t kTeb = MemoryServices::kBase + 0x2000;
    constexpr std::uint32_t kSection = MemoryServices::kBase + 0x3000;
    services.teb = kTeb;
    services.SetLastError(1234);

    Call(context, services, "InitializeCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 4), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 20), 0x020007D0U);
    Call(context, services, "EnterCriticalSection", {kSection});
    Call(context, services, "EnterCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 4), 0xFFFFFFFEU);
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 8), 2U);
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 12), hle::GuestProcess::kThreadId);
    Call(context, services, "LeaveCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 8), 1U);
    Call(context, services, "LeaveCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 4), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 12), 0U);
    bool handled = true;
    Call(context, services, "LeaveCriticalSection", {kSection}, &handled);
    RE2DJ_CHECK(context, !handled);
    Call(context, services, "DeleteCriticalSection", {kSection});
    RE2DJ_CHECK_EQ(context, services.U32(kSection + 20), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);

    // TLS: index 1 first, slots in the TEB, freed indices reused.
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsAlloc", {}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsAlloc", {}).eax, 2U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsSetValue", {1, 0x1234}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kTeb + 0xE10 + 4), 0x1234U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsGetValue", {1}).eax, 0x1234U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsGetValue", {9999}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsFree", {2}).eax, 1U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsFree", {2}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "TlsAlloc", {}).eax, 2U);
    handled = true;
    Call(context, services, "TlsGetValue", {100}, &handled);
    RE2DJ_CHECK(context, !handled);

    // Interlocked: the new value.
    services.PutU32(kSection, 5);
    RE2DJ_CHECK_EQ(context, Call(context, services, "InterlockedIncrement", {kSection}).eax, 6U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "InterlockedDecrement", {kSection}).eax, 5U);
    RE2DJ_CHECK_EQ(context, services.U32(kSection), 5U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "GetCurrentThread", {}).eax, 0xFFFFFFFEU);

    // Sleep waits on the host (Sleep(0) not at all); INFINITE stops.
    Call(context, services, "Sleep", {0});
    Call(context, services, "Sleep", {3000});
    RE2DJ_CHECK(context, services.waits == std::vector<std::uint32_t>{3000});
    handled = true;
    Call(context, services, "Sleep", {0xFFFFFFFFU}, &handled);
    RE2DJ_CHECK(context, !handled);

    // IsBad*Ptr: memory the services reach, a zero size, and a read-only page.
    const std::uint32_t page = Call(context, services, "VirtualAlloc", {0, 0x1000, 0x3000, 0x02}).eax;
    RE2DJ_CHECK(context, page != 0);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, Call(context, services, "IsBadReadPtr", {kSection, 32}).eax, 0U);
    RE2DJ_CHECK(context, Call(context, services, "IsBadReadPtr", {0, 32}).eax != 0);
    RE2DJ_CHECK_EQ(context, Call(context, services, "IsBadReadPtr", {0, 0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "IsBadWritePtr", {kSection, 32}).eax, 0U);
    RE2DJ_CHECK_EQ(context, Call(context, services, "IsBadReadPtr", {page, 16}).eax, 0U);
    RE2DJ_CHECK(context, Call(context, services, "IsBadWritePtr", {page, 16}).eax != 0);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
}

}  // namespace

void RunKernel32CrtTests(re2dj::test::Context& context)
{
    CheckCrtStartupExports(context);
    CheckRtlUnwind(context);
    CheckThreads(context);
    CheckGuestHeap(context);
    CheckHeapExports(context);
    CheckStartupExports(context);
    CheckCodePage(context);
    CheckGuestPaths(context);
    CheckEvents(context);
    CheckTimeConversion(context);
    CheckClockExports(context);
}
