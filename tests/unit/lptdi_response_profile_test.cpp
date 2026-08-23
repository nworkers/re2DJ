#include "re2dj/device/lptdi_response_profile.h"

#include <string>

#include "temporary_tree.h"
#include "test_support.h"

void RunLptdiResponseProfileTests(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    re2dj::device::LptdiResponseProfile profile;
    std::string error;

    tree.WriteText("valid.txt",
                   "# synthetic branch-separation values\n"
                   "re2dj-lptdi-response-v1\n"
                   " 0x9c406410 = 0100000000000000 \n");
    RE2DJ_CHECK(context,
                 re2dj::device::ReadLptdiResponseProfile(
                     tree.root() / "valid.txt", &profile, &error));
    const re2dj::device::LptdiResponseEntry* response =
        re2dj::device::FindLptdiResponse(profile, re2dj::device::kLptdiIoctlCode410);
    RE2DJ_CHECK(context, response != nullptr);
    if (response != nullptr)
    {
        RE2DJ_CHECK_EQ(context, response->bytes.size(), std::size_t{8});
        RE2DJ_CHECK_EQ(context, response->bytes[0], std::uint8_t{1});
    }

    tree.WriteText("second.txt",
                   std::string("re2dj-lptdi-response-v1\n0x9c406414=") +
                       std::string(re2dj::device::kLptdiIoctlResponseSize414 * 2, '0') +
                       "\n");
    RE2DJ_CHECK(context,
                 re2dj::device::ReadLptdiResponseProfile(
                     tree.root() / "second.txt", &profile, &error));
    response = re2dj::device::FindLptdiResponse(
        profile, re2dj::device::kLptdiIoctlCode414);
    RE2DJ_CHECK(context, response != nullptr);
    if (response != nullptr)
    {
        RE2DJ_CHECK_EQ(context,
                       response->bytes.size(),
                       re2dj::device::kLptdiIoctlResponseSize414);
    }

    tree.WriteText("duplicate.txt",
                   "re2dj-lptdi-response-v1\n"
                   "0x9c406410=0000000000000000\n"
                   "0x9c406410=0100000000000000\n");
    RE2DJ_CHECK(context,
                 !re2dj::device::ReadLptdiResponseProfile(
                     tree.root() / "duplicate.txt", &profile, &error));

    tree.WriteText("bad_hex.txt",
                   "re2dj-lptdi-response-v1\n"
                   "0x9c406410=not-hex\n");
    RE2DJ_CHECK(context,
                 !re2dj::device::ReadLptdiResponseProfile(
                     tree.root() / "bad_hex.txt", &profile, &error));

    tree.WriteText("bad_size.txt",
                   "re2dj-lptdi-response-v1\n"
                   "0x9c406410=00000000\n");
    RE2DJ_CHECK(context,
                 !re2dj::device::ReadLptdiResponseProfile(
                     tree.root() / "bad_size.txt", &profile, &error));

    tree.WriteText("unknown.txt",
                   "re2dj-lptdi-response-v1\n"
                   "0x9c406418=0000000000000000\n");
    RE2DJ_CHECK(context,
                 !re2dj::device::ReadLptdiResponseProfile(
                     tree.root() / "unknown.txt", &profile, &error));
}
