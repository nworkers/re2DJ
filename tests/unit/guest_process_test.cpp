#include "re2dj/hle/guest_process.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>
#include <string>

#include "re2dj/hle/modules/advapi32_module.h"
#include "re2dj/hle/modules/resolve_only_modules.h"
#include "re2dj/hle/modules/wtsapi32_module.h"

#include "memory_services.h"
#include "test_support.h"

namespace
{

void CheckHeap(re2dj::test::Context& context)
{
    using re2dj::hle::GuestProcess;

    GuestProcess process;
    // No region, no blocks.
    RE2DJ_CHECK_EQ(context, process.Allocate(4), 0U);

    process.SetHeapRegion(0x1000, 0x40);
    const std::uint32_t first = process.Allocate(4);
    const std::uint32_t second = process.Allocate(0);
    RE2DJ_CHECK_EQ(context, first, 0x1000U);
    // Blocks round up to eight bytes; a zero-byte request still gets a block.
    RE2DJ_CHECK_EQ(context, second, 0x1008U);
    RE2DJ_CHECK(context, process.BlockContains(first, 8));
    RE2DJ_CHECK(context, !process.BlockContains(first, 9));
    RE2DJ_CHECK(context, !process.BlockContains(0x0FFF, 1));
    RE2DJ_CHECK_EQ(context, process.live_blocks(), std::size_t{2});

    // First fit reuses a freed gap; freeing twice or a non-block start fails.
    RE2DJ_CHECK(context, process.Free(first));
    RE2DJ_CHECK(context, !process.Free(first));
    RE2DJ_CHECK(context, !process.Free(second + 4));
    RE2DJ_CHECK(context, !process.BlockContains(first, 1));
    RE2DJ_CHECK_EQ(context, process.Allocate(8), first);
    // 0x40 bytes hold 0x10 more after the two blocks, then nothing.
    RE2DJ_CHECK_EQ(context, process.Allocate(0x30), 0x1010U);
    RE2DJ_CHECK_EQ(context, process.Allocate(1), 0U);

    // SetErrorMode's exchange.
    RE2DJ_CHECK_EQ(context, process.ExchangeErrorMode(0x8001), 0U);
    RE2DJ_CHECK_EQ(context, process.ExchangeErrorMode(0), 0x8001U);
}

// The Windows loader's protections for 4th's sections, and the image region
// they describe.
void CheckImageRegion(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    using hle::GuestProcess;

    RE2DJ_CHECK_EQ(context, GuestProcess::SectionProtection(0x60000020U), hle::kPageExecuteRead);
    RE2DJ_CHECK_EQ(context, GuestProcess::SectionProtection(0x40000040U), hle::kPageReadOnly);
    RE2DJ_CHECK_EQ(context, GuestProcess::SectionProtection(0xC0000040U), hle::kPageWriteCopy);
    RE2DJ_CHECK_EQ(context, GuestProcess::SectionProtection(0xE0000020U), hle::kPageExecuteWriteCopy);
    RE2DJ_CHECK_EQ(context, GuestProcess::SectionProtection(0x20000020U), hle::kPageExecute);
    RE2DJ_CHECK_EQ(context, GuestProcess::SectionProtection(0x00000080U), hle::kPageNoAccess);

    GuestProcess process;
    const std::array<hle::GuestImageSection, 2> sections = {{
        {0x1000, 0x1022, 0x60000020U},
        {0x3000, 0x0766, 0xC0000040U},
    }};
    process.AddImageRegion(0x400000, 0x4000, sections);
    RE2DJ_CHECK_EQ(context, process.PageProtection(0x400000), hle::kPageReadOnly);
    RE2DJ_CHECK_EQ(context, process.PageProtection(0x402010), hle::kPageExecuteRead);
    RE2DJ_CHECK_EQ(context, process.PageProtection(0x403000), hle::kPageWriteCopy);
    RE2DJ_CHECK_EQ(context, process.PageProtection(0x404000), 0U);

    // The protection loop 4th runs over its image: open for writing, restore.
    std::uint32_t old = 0;
    RE2DJ_CHECK(context, process.VirtualProtect(0x402000, 0x22, hle::kPageReadWrite, &old) ==
                             hle::GuestMemoryResult::kOk);
    RE2DJ_CHECK_EQ(context, old, hle::kPageExecuteRead);
    RE2DJ_CHECK(context, process.VirtualProtect(0x402000, 0x22, old, &old) ==
                             hle::GuestMemoryResult::kOk);
    RE2DJ_CHECK_EQ(context, old, hle::kPageReadWrite);
    RE2DJ_CHECK_EQ(context, process.PageProtection(0x402000), hle::kPageExecuteRead);
    // Image pages can be made copy-on-write again, but not freed.
    RE2DJ_CHECK(context, process.VirtualProtect(0x403000, 1, hle::kPageWriteCopy, &old) ==
                             hle::GuestMemoryResult::kOk);
    RE2DJ_CHECK(context, process.VirtualFree(0x400000, 0, hle::kMemRelease) ==
                             hle::GuestMemoryResult::kInvalidParameter);
    RE2DJ_CHECK(context, !process.PrivateCommitted(0x400000, 1));
}

void CheckVirtualAllocShapes(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    hle::GuestProcess process;
    process.SetPrivateArena(0x00123000, 0x40000);
    std::uint32_t block = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;

    // A reservation starts on 64 KiB and commits nothing until asked.
    RE2DJ_CHECK(context, process.VirtualAlloc(0, 0x3000, hle::kMemReserve, hle::kPageReadWrite,
                                              &block, &committed) == hle::GuestMemoryResult::kOk);
    RE2DJ_CHECK_EQ(context, block, 0x00130000U);
    RE2DJ_CHECK(context, committed.empty());
    RE2DJ_CHECK(context, !process.PrivateCommitted(block, 1));
    std::uint32_t committed_at = 0;
    RE2DJ_CHECK(context, process.VirtualAlloc(block + 0x1010, 0x10, hle::kMemCommit,
                                              hle::kPageReadWrite, &committed_at,
                                              &committed) == hle::GuestMemoryResult::kOk);
    RE2DJ_CHECK_EQ(context, committed_at, block + 0x1000);
    RE2DJ_CHECK_EQ(context, committed.size(), std::size_t{1});
    RE2DJ_CHECK(context, process.PrivateCommitted(block + 0x1000, 0x1000));
    RE2DJ_CHECK(context, !process.PrivateCommitted(block + 0x1000, 0x1001));

    // Unmodelled and invalid shapes.
    RE2DJ_CHECK(context, process.VirtualAlloc(0, 0x1000, hle::kMemCommit | 0x00100000U,
                                              hle::kPageReadWrite, &block, &committed) ==
                             hle::GuestMemoryResult::kUnsupported);
    RE2DJ_CHECK(context, process.VirtualAlloc(0x00200000, 0x1000, hle::kMemReserve,
                                              hle::kPageReadWrite, &block, &committed) ==
                             hle::GuestMemoryResult::kUnsupported);
    RE2DJ_CHECK(context, process.VirtualAlloc(0, 0, hle::kMemCommit, hle::kPageReadWrite,
                                              &block, &committed) ==
                             hle::GuestMemoryResult::kInvalidParameter);
    RE2DJ_CHECK(context, process.VirtualAlloc(0, 0x1000, hle::kMemCommit, hle::kPageWriteCopy,
                                              &block, &committed) ==
                             hle::GuestMemoryResult::kInvalidParameter);
    RE2DJ_CHECK(context, process.VirtualAlloc(0x00300000, 0x1000, hle::kMemCommit,
                                              hle::kPageReadWrite, &block, &committed) ==
                             hle::GuestMemoryResult::kInvalidAddress);
    // The arena ends at 0x163000: two more 64 KiB regions fit, a third does not.
    for (int index = 0; index < 2; ++index)
    {
        RE2DJ_CHECK(context, process.VirtualAlloc(0, 0x10000, hle::kMemCommit, hle::kPageReadWrite,
                                                  &block, &committed) == hle::GuestMemoryResult::kOk);
    }
    RE2DJ_CHECK(context, process.VirtualAlloc(0, 0x10000, hle::kMemCommit, hle::kPageReadWrite,
                                              &block, &committed) ==
                             hle::GuestMemoryResult::kInvalidParameter);
}

// The DLLs the original import table names with no export implemented yet;
// ws2_32 is found by ordinal.
void CheckResolveOnlyModules(re2dj::test::Context& context)
{
    namespace modules = re2dj::hle::modules;
    const auto descriptors = modules::MakeResolveOnlyModuleDescriptors();
    RE2DJ_CHECK_EQ(context, descriptors.size(), std::size_t{3});
    std::size_t exports = 0;
    for (const auto& descriptor : descriptors)
    {
        std::string error;
        RE2DJ_CHECK(context, modules::ValidateGuestModuleDescriptor(descriptor, &error));
        for (const auto& export_descriptor : descriptor.exports)
        {
            RE2DJ_CHECK(context, export_descriptor.handler == &modules::UnimplementedExport);
            ++exports;
        }
    }
    RE2DJ_CHECK_EQ(context, exports, std::size_t{15});
    if (descriptors.size() == 3)
    {
        RE2DJ_CHECK_EQ(context, descriptors[0].name, std::string("dinput.dll"));
        RE2DJ_CHECK_EQ(context, descriptors[2].name, std::string("ws2_32.dll"));
        RE2DJ_CHECK(context, descriptors[2].exports.back().ordinal == std::uint16_t{116});
    }
}

// Accessible covers image and private pages alike, but not decommitted or
// PAGE_NOACCESS ones.
void CheckAccessible(re2dj::test::Context& context)
{
    namespace hle = re2dj::hle;
    hle::GuestProcess process;
    const std::array<hle::GuestImageSection, 1> sections = {{{0x1000, 0x1000, 0x60000020U}}};
    process.AddImageRegion(0x6f000000U, 0x3000, sections);
    RE2DJ_CHECK(context, process.Accessible(0x6f00204cU, 5));
    RE2DJ_CHECK(context, !process.Accessible(0x6f002ffeU, 0x10));
    std::uint32_t old = 0;
    process.VirtualProtect(0x6f002000U, 1, hle::kPageNoAccess, &old);
    RE2DJ_CHECK(context, !process.Accessible(0x6f002000U, 1));
    process.SetPrivateArena(0x00100000U, 0x20000);
    std::uint32_t block = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
    process.VirtualAlloc(0, 0x2000, hle::kMemCommit, hle::kPageReadWrite, &block, &committed);
    RE2DJ_CHECK(context, process.Accessible(block, 0x2000));
    process.VirtualFree(block + 0x1000, 0x1000, hle::kMemDecommit);
    RE2DJ_CHECK(context, !process.Accessible(block, 0x2000));
}

void CheckWtsSessionQuery(re2dj::test::Context& context)
{
    namespace modules = re2dj::hle::modules;
    using re2dj::test::MemoryServices;

    const auto descriptor = modules::MakeWtsapi32ModuleDescriptor();
    std::string error;
    RE2DJ_CHECK(context, modules::ValidateGuestModuleDescriptor(descriptor, &error));
    RE2DJ_CHECK_EQ(context, descriptor.name, std::string("wtsapi32.dll"));

    MemoryServices services;
    constexpr std::uint32_t kBufferSlot = MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kBytesSlot = MemoryServices::kBase + 0x14;

    // The current session's ID is 0, returned in a four-byte heap block.
    auto result = re2dj::test::CallModuleExport(
        context, services, descriptor, "WTSQuerySessionInformationA",
        {modules::kWtsCurrentServerHandle, modules::kWtsCurrentSession, modules::kWtsSessionId,
         kBufferSlot, kBytesSlot});
    RE2DJ_CHECK_EQ(context, result.eax, 1U);
    const std::uint32_t block = services.U32(kBufferSlot);
    RE2DJ_CHECK_EQ(context, block, MemoryServices::kHeapBase);
    RE2DJ_CHECK_EQ(context, services.U32(kBytesSlot), 4U);
    RE2DJ_CHECK_EQ(context, services.U32(block), modules::kWtsGuestSessionId);
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), std::size_t{1});

    // WTSFreeMemory releases it once.
    re2dj::test::CallModuleExport(context, services, descriptor, "WTSFreeMemory", {block});
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), std::size_t{0});
    bool handled = true;
    re2dj::test::CallModuleExport(
        context, services, descriptor, "WTSFreeMemory", {block}, &handled, &error);
    RE2DJ_CHECK(context, !handled);

    // Any other class, server, or session is not answered with a guess.
    handled = true;
    re2dj::test::CallModuleExport(
        context, services, descriptor, "WTSQuerySessionInformationA",
        {modules::kWtsCurrentServerHandle, modules::kWtsCurrentSession, 8, kBufferSlot, kBytesSlot},
        &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, !error.empty());
    handled = true;
    re2dj::test::CallModuleExport(
        context, services, descriptor, "WTSQuerySessionInformationA",
        {modules::kWtsCurrentServerHandle, 3, modules::kWtsSessionId, kBufferSlot, kBytesSlot},
        &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), std::size_t{0});
}

void CheckAdvapi32ResolveOnly(re2dj::test::Context& context)
{
    namespace modules = re2dj::hle::modules;

    const auto descriptor = modules::MakeAdvapi32ModuleDescriptor();
    std::string error;
    RE2DJ_CHECK(context, modules::ValidateGuestModuleDescriptor(descriptor, &error));
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{4});
    re2dj::test::MemoryServices services;
    bool handled = true;
    re2dj::test::CallModuleExport(
        context, services, descriptor, "RegOpenKeyA", {0x80000002U, 0, 0}, &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("advapi32.dll!RegOpenKeyA") != std::string::npos);
}

}  // namespace

void RunGuestProcessTests(re2dj::test::Context& context)
{
    CheckHeap(context);
    CheckImageRegion(context);
    CheckAccessible(context);
    CheckResolveOnlyModules(context);
    CheckVirtualAllocShapes(context);
    CheckWtsSessionQuery(context);
    CheckAdvapi32ResolveOnly(context);
}
