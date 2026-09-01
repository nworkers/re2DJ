#include "re2dj/device/hardlock_protocol.h"

#include <limits>

#include "re2dj/device/hardlock_api_descriptor.h"

namespace re2dj::device
{
namespace
{

HardlockRequestKind Classify(std::uint32_t control_code)
{
    switch (control_code)
    {
    case kHardlockIoctlInitialize:
        return HardlockRequestKind::kInitialize;
    case kHardlockIoctlHandshake:
        return HardlockRequestKind::kHandshake;
    case kHardlockIoctlDescriptor:
        return HardlockRequestKind::kDescriptor;
    case kHardlockIoctlTransform:
        return HardlockRequestKind::kTransform;
    default:
        return HardlockRequestKind::kUnknown;
    }
}

}  // namespace

HardlockRequestObservation HardlockProtocolTracker::Observe(
    std::uint32_t control_code,
    std::span<const std::uint8_t> input,
    std::size_t output_size,
    std::uint16_t configured_module_address)
{
    HardlockRequestObservation observation;
    observation.kind = Classify(control_code);

    switch (observation.kind)
    {
    case HardlockRequestKind::kInitialize:
        observation.shape_valid = input.empty() && output_size == 0;
        observation.sequence_valid = stage_ == Stage::kStart;
        if (observation.shape_valid && observation.sequence_valid)
        {
            stage_ = Stage::kInitialized;
        }
        break;
    case HardlockRequestKind::kHandshake:
        observation.shape_valid = input.size() == 6 && output_size == 6;
        observation.sequence_valid = stage_ == Stage::kInitialized ||
                                     stage_ == Stage::kHandshake;
        if (observation.shape_valid && observation.sequence_valid)
        {
            stage_ = Stage::kHandshake;
        }
        break;
    case HardlockRequestKind::kDescriptor:
    case HardlockRequestKind::kTransform:
    {
        observation.descriptor_present = input.size() >= kHardlockApiDescriptorSize;
        HardlockApiDescriptorHeader header;
        observation.descriptor_valid = observation.descriptor_present &&
                                       ParseHardlockApiDescriptorHeader(input, &header);
        if (observation.descriptor_valid)
        {
            observation.function = header.function;
            observation.block_count = header.block_count;
            observation.module_address_matches =
                header.module_address == configured_module_address;
        }

        if (observation.kind == HardlockRequestKind::kDescriptor)
        {
            observation.shape_valid = input.size() == kHardlockApiDescriptorSize &&
                                      output_size == kHardlockApiDescriptorSize &&
                                      observation.descriptor_valid;
            observation.sequence_valid = stage_ == Stage::kHandshake ||
                                         stage_ == Stage::kDescriptor ||
                                         stage_ == Stage::kTransform;
            if (observation.shape_valid && observation.sequence_valid)
            {
                stage_ = Stage::kDescriptor;
            }
            break;
        }

        const std::size_t block_count = observation.block_count;
        const bool size_safe =
            block_count <= (std::numeric_limits<std::size_t>::max() -
                            kHardlockApiDescriptorSize) /
                               8;
        const std::size_t expected_size = size_safe
                                              ? kHardlockApiDescriptorSize + block_count * 8
                                              : 0;
        observation.shape_valid = observation.descriptor_valid && size_safe &&
                                  observation.function == 0x0e && block_count != 0 &&
                                  input.size() == expected_size &&
                                  output_size == expected_size;
        observation.sequence_valid = stage_ == Stage::kDescriptor ||
                                     stage_ == Stage::kTransform;
        if (observation.shape_valid && observation.sequence_valid)
        {
            stage_ = Stage::kTransform;
        }
        break;
    }
    case HardlockRequestKind::kUnknown:
        break;
    }
    return observation;
}

const char* HardlockRequestKindName(HardlockRequestKind kind)
{
    switch (kind)
    {
    case HardlockRequestKind::kInitialize:
        return "initialize";
    case HardlockRequestKind::kHandshake:
        return "handshake";
    case HardlockRequestKind::kDescriptor:
        return "descriptor";
    case HardlockRequestKind::kTransform:
        return "transform";
    case HardlockRequestKind::kUnknown:
        return "unknown";
    }
    return "unknown";
}

}  // namespace re2dj::device
