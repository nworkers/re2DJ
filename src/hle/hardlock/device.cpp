#include "re2dj/hle/hardlock/device.h"

#include <algorithm>
#include <limits>

#include "re2dj/hle/hardlock/api_descriptor.h"

namespace re2dj::hle::hardlock
{
namespace
{

constexpr std::size_t kHandshakeSize = 6;
constexpr std::size_t kDescriptorStatusOffset = 0x1a;
constexpr std::size_t kTransformBlockSize = 8;

// Partial overlap would make an in-place copy order-dependent, so only exact
// aliasing (the shape the original actually uses) and fully separate buffers
// are accepted.
bool BuffersUsable(std::span<const std::uint8_t> input,
                   std::span<const std::uint8_t> output)
{
    if (input.data() == output.data())
    {
        return input.size() == output.size();
    }
    return input.data() + input.size() <= output.data() ||
           output.data() + output.size() <= input.data();
}

void CopyRequest(std::span<const std::uint8_t> input, std::span<std::uint8_t> output)
{
    if (input.data() == output.data())
    {
        return;
    }
    std::copy_n(input.begin(), input.size(), output.begin());
}

void WriteU16(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

bool TransformSizeMatches(std::uint16_t block_count, std::size_t size)
{
    if (block_count == 0)
    {
        return false;
    }
    const std::size_t blocks = block_count;
    const bool size_safe =
        blocks <= (std::numeric_limits<std::size_t>::max() - kHardlockApiDescriptorSize) /
                      kTransformBlockSize;
    return size_safe &&
           size == kHardlockApiDescriptorSize + blocks * kTransformBlockSize;
}

}  // namespace

HardlockDevice::HardlockDevice(const HardlockDeviceOptions& options)
    : options_(options)
{
}

HardlockDeviceResult HardlockDevice::Complete(std::uint32_t control_code,
                                                std::span<const std::uint8_t> input,
                                                std::span<std::uint8_t> output)
{
    HardlockDeviceResult result;
    result.kind = ClassifyHardlockRequest(control_code);
    if (result.kind == HardlockRequestKind::kUnknown)
    {
        return result;
    }
    result.outcome = HardlockOutcome::kRejectedShape;

    switch (result.kind)
    {
    case HardlockRequestKind::kInitialize:
        if (!input.empty() || !output.empty())
        {
            return result;
        }
        result.outcome = HardlockOutcome::kCompleted;
        return result;

    case HardlockRequestKind::kHandshake:
        if (input.size() != kHandshakeSize || output.size() != kHandshakeSize ||
            !BuffersUsable(input, output))
        {
            return result;
        }
        if (options_.handshake_response.has_value())
        {
            std::copy_n(options_.handshake_response->begin(),
                        kHandshakeSize,
                        output.begin());
            result.handshake_answered = true;
        }
        else
        {
            CopyRequest(input, output);
        }
        result.bytes_written = kHandshakeSize;
        result.outcome = HardlockOutcome::kCompleted;
        return result;

    case HardlockRequestKind::kDescriptor:
    case HardlockRequestKind::kTransform:
    {
        const bool descriptor = result.kind == HardlockRequestKind::kDescriptor;
        if (input.size() != output.size() || !BuffersUsable(input, output))
        {
            return result;
        }
        HardlockApiDescriptorHeader header;
        if (input.size() < kHardlockApiDescriptorSize ||
            !ParseHardlockApiDescriptorHeader(input, &header))
        {
            return result;
        }
        if (descriptor ? input.size() != kHardlockApiDescriptorSize
                       : !TransformSizeMatches(header.block_count, input.size()))
        {
            return result;
        }
        CopyRequest(input, output);
        if (options_.clear_descriptor_status)
        {
            WriteU16(output, kDescriptorStatusOffset, 0);
            result.descriptor_status_cleared = true;
        }
        // The tail word belongs to the Function 0 descriptor call only;
        // widening it would answer calls the original never asked this way.
        if (descriptor && header.function == 0 &&
            options_.descriptor_tail_word.has_value())
        {
            WriteU16(output, kHardlockApiTailWordOffset, *options_.descriptor_tail_word);
            result.descriptor_tail_written = true;
        }
        // The dongle-internal Function 0x0e algorithm is unknown here, so a
        // payload is only ever replaced from the externally computed response
        // map. A block the map does not cover passes through untouched rather
        // than being guessed.
        if (!descriptor && !options_.transform_responses.empty())
        {
            for (std::size_t offset = kHardlockApiDescriptorSize; offset < output.size();
                 offset += kTransformBlockSize)
            {
                HardlockTransformBlock block = {};
                std::copy_n(output.begin() + static_cast<std::ptrdiff_t>(offset),
                            block.size(),
                            block.begin());
                const HardlockTransformBlock* const mapped =
                    FindHardlockTransformResponse(options_.transform_responses, block);
                if (mapped == nullptr)
                {
                    ++result.transform_blocks_unmapped;
                    continue;
                }
                std::copy_n(mapped->begin(),
                            mapped->size(),
                            output.begin() + static_cast<std::ptrdiff_t>(offset));
                ++result.transform_blocks_mapped;
            }
        }
        result.bytes_written = output.size();
        result.outcome = HardlockOutcome::kCompleted;
        return result;
    }

    case HardlockRequestKind::kUnknown:
        break;
    }
    return result;
}

const char* HardlockOutcomeName(HardlockOutcome outcome)
{
    switch (outcome)
    {
    case HardlockOutcome::kNotHandled:
        return "not-handled";
    case HardlockOutcome::kRejectedShape:
        return "rejected-shape";
    case HardlockOutcome::kCompleted:
        return "completed";
    }
    return "not-handled";
}

}  // namespace re2dj::hle::hardlock
