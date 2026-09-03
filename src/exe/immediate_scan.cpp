#include "re2dj/exe/immediate_scan.h"

namespace re2dj::exe {
namespace {

std::uint32_t ReadLittleEndian32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

}  // namespace

std::vector<ImmediateReference> ScanImmediateReferences(const std::uint8_t* bytes,
                                                        std::size_t size,
                                                        const std::uint32_t* values,
                                                        std::size_t value_count,
                                                        std::size_t max_matches,
                                                        bool* capped,
                                                        std::size_t* total_matches)
{
    std::vector<ImmediateReference> matches;
    std::size_t total = 0;
    if (capped != nullptr)
    {
        *capped = false;
    }
    if (total_matches != nullptr)
    {
        *total_matches = 0;
    }
    if (bytes == nullptr || values == nullptr || size < sizeof(std::uint32_t) ||
        value_count == 0)
    {
        return matches;
    }
    for (std::size_t index = 0; index + sizeof(std::uint32_t) <= size; ++index)
    {
        const std::uint32_t candidate = ReadLittleEndian32(bytes + index);
        bool wanted = false;
        for (std::size_t value_index = 0; value_index < value_count; ++value_index)
        {
            if (values[value_index] == candidate)
            {
                wanted = true;
                break;
            }
        }
        if (!wanted)
        {
            continue;
        }
        ++total;
        if (matches.size() >= max_matches)
        {
            if (capped != nullptr)
            {
                *capped = true;
            }
            continue;
        }
        ImmediateReference reference;
        reference.offset = static_cast<std::uint32_t>(index);
        reference.value = candidate;
        const std::size_t leading = index < ImmediateReference::kContextBytes
                                        ? index
                                        : ImmediateReference::kContextBytes;
        for (std::size_t leading_index = 0; leading_index < leading; ++leading_index)
        {
            reference.leading[leading_index] =
                bytes[index - leading + leading_index];
        }
        reference.leading_count = static_cast<std::uint8_t>(leading);
        const std::size_t after = index + sizeof(std::uint32_t);
        const std::size_t available = size - after;
        const std::size_t trailing = available < ImmediateReference::kContextBytes
                                         ? available
                                         : ImmediateReference::kContextBytes;
        for (std::size_t trailing_index = 0; trailing_index < trailing; ++trailing_index)
        {
            reference.trailing[trailing_index] = bytes[after + trailing_index];
        }
        reference.trailing_count = static_cast<std::uint8_t>(trailing);
        matches.push_back(reference);
    }
    if (total_matches != nullptr)
    {
        *total_matches = total;
    }
    return matches;
}

}  // namespace re2dj::exe
