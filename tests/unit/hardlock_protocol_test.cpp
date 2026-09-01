#include "re2dj/device/hardlock_protocol.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "re2dj/device/hardlock_api_descriptor.h"
#include "test_support.h"

void RunHardlockProtocolTests(re2dj::test::Context& context)
{
    re2dj::device::HardlockProtocolTracker tracker;
    const std::span<const std::uint8_t> empty;
    auto observation = tracker.Observe(
        re2dj::device::kHardlockIoctlInitialize, empty, 0, 0x1234);
    RE2DJ_CHECK(context, observation.shape_valid);
    RE2DJ_CHECK(context, observation.sequence_valid);

    std::array<std::uint8_t, 6> handshake = {};
    observation = tracker.Observe(
        re2dj::device::kHardlockIoctlHandshake, handshake, handshake.size(), 0x1234);
    RE2DJ_CHECK(context, observation.shape_valid);
    RE2DJ_CHECK(context, observation.sequence_valid);

    std::array<std::uint8_t, re2dj::device::kHardlockApiDescriptorSize> descriptor = {};
    descriptor[0x08] = 0x34;
    descriptor[0x09] = 0x12;
    descriptor[0x16] = 1;
    descriptor[0x18] = 0x0e;
    observation = tracker.Observe(
        re2dj::device::kHardlockIoctlDescriptor,
        descriptor,
        descriptor.size(),
        0x1234);
    RE2DJ_CHECK(context, observation.shape_valid);
    RE2DJ_CHECK(context, observation.sequence_valid);
    RE2DJ_CHECK(context, observation.descriptor_valid);
    RE2DJ_CHECK(context, observation.module_address_matches);
    RE2DJ_CHECK_EQ(context, observation.function, std::uint16_t{0x0e});

    std::array<std::uint8_t, re2dj::device::kHardlockApiDescriptorSize + 8> packet = {};
    std::copy(descriptor.begin(), descriptor.end(), packet.begin());
    observation = tracker.Observe(
        re2dj::device::kHardlockIoctlTransform, packet, packet.size(), 0x1234);
    RE2DJ_CHECK(context, observation.shape_valid);
    RE2DJ_CHECK(context, observation.sequence_valid);
    RE2DJ_CHECK(context, observation.module_address_matches);
    RE2DJ_CHECK_EQ(context, observation.block_count, std::uint16_t{1});

    re2dj::device::HardlockProtocolTracker invalid_tracker;
    observation = invalid_tracker.Observe(
        re2dj::device::kHardlockIoctlHandshake, handshake, handshake.size(), 0x1234);
    RE2DJ_CHECK(context, observation.shape_valid);
    RE2DJ_CHECK(context, !observation.sequence_valid);
    observation = invalid_tracker.Observe(
        re2dj::device::kHardlockIoctlInitialize, empty, 1, 0x1234);
    RE2DJ_CHECK(context, !observation.shape_valid);
    RE2DJ_CHECK(context, observation.sequence_valid);

    descriptor[0x08] = 0x35;
    observation = tracker.Observe(
        re2dj::device::kHardlockIoctlDescriptor,
        descriptor,
        descriptor.size(),
        0x1234);
    RE2DJ_CHECK(context, !observation.module_address_matches);

    RE2DJ_CHECK_EQ(context,
                   std::string(re2dj::device::HardlockRequestKindName(
                       re2dj::device::HardlockRequestKind::kTransform)),
                   std::string("transform"));
}
