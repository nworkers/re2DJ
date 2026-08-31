#include "re2dj/device/hardlock_450_response.h"

#include "test_support.h"

void RunHardlock450ResponseTests(re2dj::test::Context& context)
{
    re2dj::device::Hardlock450Response response = {};
    std::string error;
    RE2DJ_CHECK(context,
                 re2dj::device::ParseHardlock450Response(
                     "0100FaFa0010", &response, &error));
    RE2DJ_CHECK_EQ(context, response[0], std::uint8_t{0x01});
    RE2DJ_CHECK_EQ(context, response[2], std::uint8_t{0xfa});
    RE2DJ_CHECK_EQ(context, response[5], std::uint8_t{0x10});
    RE2DJ_CHECK(context,
                 !re2dj::device::ParseHardlock450Response(
                     "0100fafa00", &response, &error));
    RE2DJ_CHECK(context,
                 !re2dj::device::ParseHardlock450Response(
                     "0100fafa00xz", &response, &error));
}
