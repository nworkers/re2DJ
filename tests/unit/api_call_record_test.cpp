#include "re2dj/hle/api_call_record.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "memory_services.h"
#include "test_support.h"

namespace
{

using re2dj::hle::ApiCallRecord;
using re2dj::hle::RecordingImportCallServices;
using re2dj::runtime::GuestAddress;
using re2dj::test::MemoryServices;

void CheckRecording(re2dj::test::Context& context)
{
    MemoryServices inner;
    ApiCallRecord record;
    const RecordingImportCallServices services(inner, &record);
    constexpr std::uint32_t kText = MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kData = MemoryServices::kBase + 0x40;
    inner.Put(kText, "HL_SEARCH");

    // Every service reaches the inner one; reads, writes, and last errors are noted.
    std::string text;
    std::string error;
    RE2DJ_CHECK(context, services.ReadGuestString(GuestAddress(kText), &text, &error));
    RE2DJ_CHECK_EQ(context, text, std::string("HL_SEARCH"));
    std::array<std::uint8_t, 4> bytes = {1, 2, 3, 4};
    RE2DJ_CHECK(context, services.WriteGuestBytes(GuestAddress(kData), bytes, &error));
    RE2DJ_CHECK_EQ(context, inner.U32(kData), 0x04030201U);
    std::array<std::uint8_t, 2> read = {};
    RE2DJ_CHECK(context, services.ReadGuestBytes(GuestAddress(kData), read, &error));
    RE2DJ_CHECK(context, !services.ReadGuestBytes(GuestAddress(0x10), read, &error));
    const std::array<std::uint8_t, 6> path = {'D', ':', '\\', 'a', 'b', 0};
    services.WriteGuestBytes(GuestAddress(kData), path, &error);
    const std::vector<std::uint8_t> large(100, 0xAB);
    services.WriteGuestBytes(GuestAddress(kData), large, &error);
    services.SetLastError(203);
    RE2DJ_CHECK_EQ(context, inner.LastError(), 203U);
    RE2DJ_CHECK(context, services.Process() == inner.Process());

    const std::vector<std::string> lines = re2dj::hle::FormatApiCallEvents(record, false);
    RE2DJ_CHECK_EQ(context, lines.size(), std::size_t{7});
    if (lines.size() == 7)
    {
        RE2DJ_CHECK_EQ(context, lines[0], std::string("read  00010010 \"HL_SEARCH\""));
        RE2DJ_CHECK_EQ(context, lines[1], std::string("write 00010040 [4] 01020304"));
        RE2DJ_CHECK_EQ(context, lines[2], std::string("read  00010040 [2] 0102"));
        RE2DJ_CHECK_EQ(context, lines[3], std::string("read  00000010 [2] <failed>"));
        RE2DJ_CHECK_EQ(context, lines[4], std::string("write 00010040 [6] \"D:\\\\ab\" + NUL"));
        // Only the first 64 bytes are kept; the rest is counted.
        RE2DJ_CHECK(context, lines[5].find("[100] abababab") != std::string::npos);
        RE2DJ_CHECK(context, lines[5].find("... (+36)") != std::string::npos);
        RE2DJ_CHECK_EQ(context, lines[6], std::string("last_error <- 203"));
    }

    // Secret-derived buffers keep only their lengths.
    const std::vector<std::string> withheld = re2dj::hle::FormatApiCallEvents(record, true);
    if (withheld.size() == 7)
    {
        RE2DJ_CHECK_EQ(context, withheld[1], std::string("write 00010040 [4] [withheld]"));
        RE2DJ_CHECK_EQ(context, withheld[0], std::string("read  00010010 \"HL_SEARCH\""));
    }
}

// A guest call is noted with its function, its arguments (the data argument
// by its length), and the guest's answer.
void CheckGuestCall(re2dj::test::Context& context)
{
    MemoryServices inner;
    inner.guest_function = [](const std::vector<std::uint32_t>& arguments) { return arguments[1] + 1; };
    ApiCallRecord record;
    const RecordingImportCallServices services(inner, &record);
    re2dj::hle::GuestCall call;
    call.function = 0x00406BAAU;
    call.arguments = {0x10014, 0x81, 0, 0};
    call.data.assign(48, 0);
    call.data_argument = 3;
    std::uint32_t result = 0;
    std::string error;
    RE2DJ_CHECK(context, services.CallGuest(&call, &result, &error));
    RE2DJ_CHECK_EQ(context, result, 0x82U);
    RE2DJ_CHECK_EQ(context, inner.guest_calls.size(), std::size_t{1});
    const std::vector<std::string> lines = re2dj::hle::FormatApiCallEvents(record, false);
    RE2DJ_CHECK_EQ(context, lines.size(), std::size_t{1});
    if (lines.size() == 1)
    {
        RE2DJ_CHECK_EQ(context, lines[0],
                       std::string("call  00406baa(00010014, 00000081, 00000000, &data[48]) -> 00000082"));
    }
}

}  // namespace

void RunApiCallRecordTests(re2dj::test::Context& context)
{
    CheckRecording(context);
    CheckGuestCall(context);
}
