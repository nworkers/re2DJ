#include "re2dj/runtime/address_space.h"

#include <cstdint>
#include <string>

#include "test_support.h"

void RunAddressSpaceTests(re2dj::test::Context& context)
{
    using re2dj::runtime::AddressSpace;
    using re2dj::runtime::GuestAddress;
    using re2dj::runtime::MemoryAccess;

    AddressSpace space;
    std::string error;
    RE2DJ_CHECK(context,
                space.Map(GuestAddress(0x1000),
                          0x100,
                          MemoryAccess::kRead | MemoryAccess::kWrite,
                          &error));
    RE2DJ_CHECK(context, space.Write32(GuestAddress(0x1010), 0x78563412));
    std::uint32_t value = 0;
    RE2DJ_CHECK(context, space.Read32(GuestAddress(0x1010), &value));
    RE2DJ_CHECK_EQ(context, value, std::uint32_t{0x78563412});
    std::uint8_t outside = 0;
    RE2DJ_CHECK(context, !space.Read8(GuestAddress(0x2000), &outside));
    RE2DJ_CHECK(context,
                !space.Map(GuestAddress(0x1080), 0x100, MemoryAccess::kRead, &error));
    RE2DJ_CHECK(context,
                !space.Map(GuestAddress(0xFFFFFFF0), 0x20, MemoryAccess::kRead, &error));

    AddressSpace top_page;
    RE2DJ_CHECK(context,
                top_page.Map(GuestAddress(0xFFFFF000),
                             re2dj::runtime::kGuestPageSize,
                             MemoryAccess::kRead | MemoryAccess::kWrite,
                             &error));
    RE2DJ_CHECK(context, !top_page.Write32(GuestAddress(0xFFFFFFFE), 0x12345678));

    AddressSpace read_only;
    RE2DJ_CHECK(context,
                read_only.Map(GuestAddress(0x2000), 0x20, MemoryAccess::kRead, &error));
    RE2DJ_CHECK(context, !read_only.Write8(GuestAddress(0x2000), 1));
}
