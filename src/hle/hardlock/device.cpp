#include "re2dj/hle/hardlock/device.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "re2dj/hle/hardlock/api_descriptor.h"

namespace re2dj::hle::hardlock
{
namespace
{

constexpr std::size_t kHandshakeSize = 6;
constexpr std::size_t kDescriptorStatusOffset = 0x1a;

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
                      kHardlockTransformBlockSize;
    return size_safe &&
           size == kHardlockApiDescriptorSize + blocks * kHardlockTransformBlockSize;
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
        // Diagnostic: refuse a chosen function so a retry, or its absence,
        // shows whether the protection inspects that answer. The request is
        // left rejected exactly as a framing violation would be.
        if (options_.reject_function.has_value() &&
            header.function == *options_.reject_function)
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
        // The dongle-internal algorithms are unknown here, so a payload is
        // only ever replaced from the externally computed response map. A
        // request row is more specific than a block row, so it goes first. A
        // block no row covers passes through untouched rather than being
        // guessed.
        const std::span<std::uint8_t> payload = output.subspan(kHardlockApiDescriptorSize);
        if (!descriptor && options_.seeds.has_value())
        {
            HardlockEngine engine(*options_.seeds);
            if (header.function == 0x0009 ||
                (header.block_count == 7 && header.function == 0x0011 && payload.size() == 56))
            {
                std::span<const std::uint8_t, 8> in_block(payload.data() + 40, 8);
                const auto code_resp = engine.CodePayload(in_block);
                // Blocks 0, 1, 2 from code response
                std::copy_n(code_resp.begin(), 24, payload.begin());
                // Block 3: add two 32-bit little-endian DWORDs from code response
                std::uint32_t base0 = 0, add0 = 0;
                std::uint32_t base1 = 0, add1 = 0;
                std::memcpy(&base0, payload.data() + 24, 4);
                std::memcpy(&add0, code_resp.data() + 24, 4);
                std::memcpy(&base1, payload.data() + 28, 4);
                std::memcpy(&add1, code_resp.data() + 28, 4);
                base0 += add0;
                base1 += add1;
                std::memcpy(payload.data() + 24, &base0, 4);
                std::memcpy(payload.data() + 28, &base1, 4);
                // Blocks 4, 5 from code response
                std::copy_n(code_resp.begin() + 32, 16, payload.begin() + 32);
                result.transform_dynamically_computed = true;
                result.transform_payload_mapped = true;
            }
            else
            {
                for (std::size_t offset = kHardlockApiDescriptorSize; offset < output.size();
                     offset += kHardlockTransformBlockSize)
                {
                    std::span<std::uint8_t, 8> block(output.data() + offset, 8);
                    engine.EncryptBlock(block);
                    ++result.transform_blocks_mapped;
                }
                result.transform_dynamically_computed = true;
            }
        }
        else
        {
            const HardlockPayloadResponseEntry* const request_row =
                descriptor ? nullptr
                           : FindHardlockPayloadResponse(options_.payload_responses, payload);
            if (request_row != nullptr)
            {
                ApplyHardlockPayloadResponse(*request_row, payload);
                result.transform_payload_mapped = true;
            }
            else if (!descriptor && !options_.transform_responses.empty())
            {
                for (std::size_t offset = kHardlockApiDescriptorSize; offset < output.size();
                     offset += kHardlockTransformBlockSize)
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
