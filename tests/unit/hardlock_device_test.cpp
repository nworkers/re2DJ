#include "re2dj/hle/hardlock/device.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "re2dj/hle/hardlock/api_descriptor.h"
#include "test_support.h"

namespace
{

// A descriptor with a module address, block count, and function value. The
// bytes are arbitrary test scaffolding, not observed dongle material.
std::vector<std::uint8_t> MakeDescriptor(std::uint16_t function,
                                         std::uint16_t block_count,
                                         std::size_t total_size)
{
    std::vector<std::uint8_t> bytes(total_size, 0);
    bytes[0x08] = 0x34;
    bytes[0x09] = 0x12;
    bytes[0x16] = static_cast<std::uint8_t>(block_count & 0xff);
    bytes[0x17] = static_cast<std::uint8_t>((block_count >> 8) & 0xff);
    bytes[0x18] = static_cast<std::uint8_t>(function & 0xff);
    bytes[0x19] = static_cast<std::uint8_t>((function >> 8) & 0xff);
    // A nonzero status so clearing it is observable.
    bytes[0x1a] = 0x7f;
    bytes[0x1b] = 0x7f;
    return bytes;
}

}  // namespace

void RunHardlockDeviceTests(re2dj::test::Context& context)
{
    using re2dj::hle::hardlock::HardlockRequestKind;
    using re2dj::hle::hardlock::HardlockDevice;
    using re2dj::hle::hardlock::HardlockDeviceOptions;
    using re2dj::hle::hardlock::HardlockOutcome;
    using re2dj::hle::hardlock::HardlockDeviceResult;

    const std::span<const std::uint8_t> no_input;
    const std::span<std::uint8_t> no_output;

    HardlockDevice stub;

    // An unrelated control code stays with the caller.
    HardlockDeviceResult result = stub.Complete(0x9c406410, no_input, no_output);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kNotHandled);
    RE2DJ_CHECK(context, result.kind == HardlockRequestKind::kUnknown);

    // Initialize takes and returns nothing.
    result = stub.Complete(
        re2dj::hle::hardlock::kHardlockIoctlInitialize, no_input, no_output);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, result.kind == HardlockRequestKind::kInitialize);
    RE2DJ_CHECK_EQ(context, result.bytes_written, std::size_t{0});

    std::array<std::uint8_t, 6> handshake_output = {};
    result = stub.Complete(re2dj::hle::hardlock::kHardlockIoctlInitialize,
                           no_input,
                           handshake_output);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // Without a configured replay the handshake preserves the request.
    std::array<std::uint8_t, 6> handshake = {1, 2, 3, 4, 5, 6};
    result = stub.Complete(
        re2dj::hle::hardlock::kHardlockIoctlHandshake, handshake, handshake);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, !result.handshake_answered);
    RE2DJ_CHECK_EQ(context, result.bytes_written, std::size_t{6});
    RE2DJ_CHECK_EQ(context, handshake[0], std::uint8_t{1});
    RE2DJ_CHECK_EQ(context, handshake[5], std::uint8_t{6});

    // Separate buffers copy the request across.
    handshake_output = {};
    result = stub.Complete(
        re2dj::hle::hardlock::kHardlockIoctlHandshake, handshake, handshake_output);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK_EQ(context, handshake_output[5], std::uint8_t{6});

    // Wrong sizes are rejected rather than forced to succeed.
    std::array<std::uint8_t, 5> short_handshake = {};
    result = stub.Complete(
        re2dj::hle::hardlock::kHardlockIoctlHandshake, short_handshake, handshake_output);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // Partial overlap would make the copy order-dependent.
    std::array<std::uint8_t, 12> overlapping = {};
    result = stub.Complete(re2dj::hle::hardlock::kHardlockIoctlHandshake,
                           std::span<const std::uint8_t>(overlapping.data(), 6),
                           std::span<std::uint8_t>(overlapping.data() + 3, 6));
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // A configured replay wins over buffer preservation.
    HardlockDeviceOptions options;
    options.handshake_response = re2dj::hle::hardlock::HardlockHandshakeResponse{9, 9, 9, 9, 9, 9};
    options.descriptor_tail_word = std::uint16_t{0xbeef};
    HardlockDevice configured(options);
    handshake_output = {};
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlHandshake, handshake, handshake_output);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, result.handshake_answered);
    RE2DJ_CHECK_EQ(context, handshake_output[0], std::uint8_t{9});

    // A Function 0 descriptor clears status and takes the configured tail.
    std::vector<std::uint8_t> descriptor =
        MakeDescriptor(0, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize);
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlDescriptor, descriptor, descriptor);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, result.descriptor_status_cleared);
    RE2DJ_CHECK(context, result.descriptor_tail_written);
    RE2DJ_CHECK_EQ(context,
                   result.bytes_written,
                   re2dj::hle::hardlock::kHardlockApiDescriptorSize);
    RE2DJ_CHECK_EQ(context, descriptor[0x1a], std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, descriptor[0x1b], std::uint8_t{0});
    RE2DJ_CHECK_EQ(context,
                   descriptor[re2dj::hle::hardlock::kHardlockApiTailWordOffset],
                   std::uint8_t{0xef});
    RE2DJ_CHECK_EQ(context,
                   descriptor[re2dj::hle::hardlock::kHardlockApiTailWordOffset + 1],
                   std::uint8_t{0xbe});

    // The tail experiment stays scoped to Function 0.
    std::vector<std::uint8_t> other_function =
        MakeDescriptor(6, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize);
    const std::uint8_t original_tail =
        other_function[re2dj::hle::hardlock::kHardlockApiTailWordOffset];
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlDescriptor, other_function, other_function);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, !result.descriptor_tail_written);
    RE2DJ_CHECK_EQ(context,
                   other_function[re2dj::hle::hardlock::kHardlockApiTailWordOffset],
                   original_tail);

    // A descriptor call must be exactly the descriptor size.
    std::vector<std::uint8_t> oversized =
        MakeDescriptor(0, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlDescriptor, oversized, oversized);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // The transform packet is descriptor + block_count * 8 and its payload is
    // left untouched, because the dongle algorithm is unknown.
    std::vector<std::uint8_t> packet =
        MakeDescriptor(0x0e, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    packet[re2dj::hle::hardlock::kHardlockApiDescriptorSize] = 0x5a;
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, packet, packet);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, result.descriptor_status_cleared);
    RE2DJ_CHECK(context, !result.descriptor_tail_written);
    RE2DJ_CHECK_EQ(context,
                   result.bytes_written,
                   re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    RE2DJ_CHECK_EQ(context,
                   packet[re2dj::hle::hardlock::kHardlockApiDescriptorSize],
                   std::uint8_t{0x5a});

    // A block count that does not match the packet size is rejected.
    std::vector<std::uint8_t> mismatched =
        MakeDescriptor(0x0e, 2, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, mismatched, mismatched);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // Zero blocks carry no payload and are outside the observed contract.
    std::vector<std::uint8_t> empty_packet =
        MakeDescriptor(0x0e, 0, re2dj::hle::hardlock::kHardlockApiDescriptorSize);
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, empty_packet, empty_packet);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // A large block count must not wrap the size arithmetic.
    std::vector<std::uint8_t> huge_blocks =
        MakeDescriptor(0x0e, 0xffff, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, huge_blocks, huge_blocks);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);

    // A transform shorter than the descriptor cannot be parsed.
    std::vector<std::uint8_t> truncated(re2dj::hle::hardlock::kHardlockApiDescriptorSize - 1, 0);
    result = configured.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, truncated, truncated);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kRejectedShape);


    // A response map replaces a covered challenge block and leaves an
    // uncovered one untouched, so a partial map is visible rather than silent.
    HardlockDeviceOptions mapped_options;
    re2dj::hle::hardlock::HardlockTransformResponseEntry entry;
    entry.input = {0x5a, 0, 0, 0, 0, 0, 0, 0};
    entry.output = {0xc0, 0xff, 0xee, 0x01, 0x02, 0x03, 0x04, 0x05};
    mapped_options.transform_responses.push_back(entry);
    HardlockDevice mapping(mapped_options);

    std::vector<std::uint8_t> covered =
        MakeDescriptor(0x0e, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    covered[re2dj::hle::hardlock::kHardlockApiDescriptorSize] = 0x5a;
    result = mapping.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, covered, covered);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK_EQ(context, result.transform_blocks_mapped, std::size_t{1});
    RE2DJ_CHECK_EQ(context, result.transform_blocks_unmapped, std::size_t{0});
    RE2DJ_CHECK_EQ(context,
                   covered[re2dj::hle::hardlock::kHardlockApiDescriptorSize],
                   std::uint8_t{0xc0});
    RE2DJ_CHECK_EQ(context,
                   covered[re2dj::hle::hardlock::kHardlockApiDescriptorSize + 7],
                   std::uint8_t{0x05});

    std::vector<std::uint8_t> uncovered =
        MakeDescriptor(0x0e, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    uncovered[re2dj::hle::hardlock::kHardlockApiDescriptorSize] = 0x99;
    result = mapping.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, uncovered, uncovered);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK_EQ(context, result.transform_blocks_mapped, std::size_t{0});
    RE2DJ_CHECK_EQ(context, result.transform_blocks_unmapped, std::size_t{1});
    RE2DJ_CHECK_EQ(context,
                   uncovered[re2dj::hle::hardlock::kHardlockApiDescriptorSize],
                   std::uint8_t{0x99});

    // The same challenge maps identically however often it appears, so call
    // order cannot change the answer.
    std::vector<std::uint8_t> repeated =
        MakeDescriptor(0x0e, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8);
    repeated[re2dj::hle::hardlock::kHardlockApiDescriptorSize] = 0x5a;
    result = mapping.Complete(
        re2dj::hle::hardlock::kHardlockIoctlTransform, repeated, repeated);
    RE2DJ_CHECK_EQ(context, result.transform_blocks_mapped, std::size_t{1});
    RE2DJ_CHECK_EQ(context,
                   repeated[re2dj::hle::hardlock::kHardlockApiDescriptorSize],
                   std::uint8_t{0xc0});

    // A descriptor call is never touched by the map.
    std::vector<std::uint8_t> mapped_descriptor =
        MakeDescriptor(0, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize);
    result = mapping.Complete(
        re2dj::hle::hardlock::kHardlockIoctlDescriptor, mapped_descriptor, mapped_descriptor);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK_EQ(context, result.transform_blocks_mapped, std::size_t{0});

    // Status clearing can be turned off so the request passes through as-is.
    HardlockDeviceOptions preserve_status;
    preserve_status.clear_descriptor_status = false;
    HardlockDevice preserving(preserve_status);
    std::vector<std::uint8_t> preserved =
        MakeDescriptor(0, 1, re2dj::hle::hardlock::kHardlockApiDescriptorSize);
    result = preserving.Complete(
        re2dj::hle::hardlock::kHardlockIoctlDescriptor, preserved, preserved);
    RE2DJ_CHECK(context, result.outcome == HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, !result.descriptor_status_cleared);
    RE2DJ_CHECK_EQ(context, preserved[0x1a], std::uint8_t{0x7f});
}
