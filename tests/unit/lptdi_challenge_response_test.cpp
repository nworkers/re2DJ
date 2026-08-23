#include "re2dj/device/lptdi_challenge_response.h"

#include <array>
#include <string>

#include "test_support.h"

void RunLptdiChallengeResponseTests(re2dj::test::Context& context)
{
    using re2dj::device::LptdiTargetState;

    RE2DJ_CHECK_EQ(context,
                   re2dj::device::AdvanceLptdiChallenge(0x75ea31f1u),
                   std::uint32_t{0x446dc4e6u});
    RE2DJ_CHECK_EQ(context,
                   re2dj::device::AdvanceLptdiChallenge(0x446dc4e6u),
                   std::uint32_t{0xbb93d79fu});
    RE2DJ_CHECK_EQ(context,
                   re2dj::device::AdvanceLptdiChallenge(0x2656754cu),
                   std::uint32_t{0xd05b70bdu});
    RE2DJ_CHECK_EQ(context,
                   re2dj::device::AdvanceLptdiChallenge(0xd05b70bdu),
                   std::uint32_t{0x5eb9ed22u});

    const LptdiTargetState expected_mask = {
        0xe6, 0xc4, 0x6d, 0x44, 0x9f, 0xd7, 0x93, 0xbb,
    };
    RE2DJ_CHECK_EQ(context,
                   re2dj::device::ComputeLptdiChallengeMask(0x75ea31f1u),
                   expected_mask);

    const LptdiTargetState target = {
        1, 2, 3, 4, 5, 6, 7, 8,
    };
    const LptdiTargetState expected_response = {
        0xe7, 0xc6, 0x6e, 0x40, 0x9a, 0xd1, 0x94, 0xb3,
    };
    RE2DJ_CHECK_EQ(context,
                   re2dj::device::EncodeLptdiTargetState(0x75ea31f1u, target),
                   expected_response);

    LptdiTargetState parsed = {};
    std::string error;
    RE2DJ_CHECK(context,
                re2dj::device::ParseLptdiTargetState(
                    "0123456789abcdef", &parsed, &error));
    const LptdiTargetState expected_parsed = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    };
    RE2DJ_CHECK_EQ(context, parsed, expected_parsed);
    RE2DJ_CHECK(context,
                !re2dj::device::ParseLptdiTargetState(
                    "0123456789abcde", &parsed, &error));
    RE2DJ_CHECK(context,
                !re2dj::device::ParseLptdiTargetState(
                    "0123456789abcdeg", &parsed, &error));
}
