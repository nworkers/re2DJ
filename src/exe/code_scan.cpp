#include "re2dj/exe/code_scan.h"

#include <algorithm>

namespace re2dj::exe {

PrologueSearchResult FindPrologueBefore(const std::uint8_t* bytes,
                                        std::size_t size,
                                        std::size_t anchor,
                                        std::size_t max_scan_back)
{
    PrologueSearchResult result;
    constexpr std::size_t kPrologueLength = 3;
    if (bytes == nullptr || size < kPrologueLength || anchor == 0 || anchor > size)
    {
        return result;
    }
    const std::size_t limit = anchor > max_scan_back ? anchor - max_scan_back : 0;
    // `anchor - 1` would start the window on the anchor's own first byte, so the
    // search begins one byte earlier and walks down to the limit.
    std::size_t index = anchor;
    while (index > limit)
    {
        --index;
        if (index + kPrologueLength > size)
        {
            continue;
        }
        if (bytes[index] == 0x55 && bytes[index + 1] == 0x8b && bytes[index + 2] == 0xec)
        {
            result.found = true;
            result.offset = index;
            result.distance = anchor - index;
            return result;
        }
    }
    return result;
}

std::vector<NearBranch> ListNearBranches(const std::uint8_t* bytes,
                                         std::size_t size,
                                         std::uint32_t base_address,
                                         std::size_t start,
                                         std::size_t length,
                                         std::size_t max_branches,
                                         bool* capped,
                                         std::size_t* total_branches)
{
    std::vector<NearBranch> branches;
    std::size_t total = 0;
    if (capped != nullptr)
    {
        *capped = false;
    }
    if (total_branches != nullptr)
    {
        *total_branches = 0;
    }
    if (bytes == nullptr || start >= size)
    {
        return branches;
    }
    const std::size_t end = (std::min)(size, start + length);
    for (std::size_t index = start; index < end; ++index)
    {
        const std::uint8_t opcode = bytes[index];
        std::size_t instruction_length = 0;
        std::uint8_t recorded_opcode = opcode;
        bool near_form = false;
        std::uint32_t relative = 0;
        if (opcode == 0xe8 || opcode == 0xe9)
        {
            instruction_length = 5;
            near_form = true;
        }
        else if (opcode == 0xeb || (opcode >= 0x70 && opcode <= 0x7f))
        {
            instruction_length = 2;
        }
        else if (opcode == 0x0f && index + 1 < end && bytes[index + 1] >= 0x80 &&
                 bytes[index + 1] <= 0x8f)
        {
            instruction_length = 6;
            recorded_opcode = bytes[index + 1];
            near_form = true;
        }
        else
        {
            continue;
        }
        if (index + instruction_length > size)
        {
            continue;
        }
        if (instruction_length == 2)
        {
            // A short displacement is signed, so sign-extend before adding.
            relative = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int8_t>(bytes[index + 1])));
        }
        else
        {
            const std::size_t displacement = index + instruction_length - 4;
            relative = static_cast<std::uint32_t>(bytes[displacement]) |
                       (static_cast<std::uint32_t>(bytes[displacement + 1]) << 8) |
                       (static_cast<std::uint32_t>(bytes[displacement + 2]) << 16) |
                       (static_cast<std::uint32_t>(bytes[displacement + 3]) << 24);
        }
        ++total;
        if (branches.size() >= max_branches)
        {
            if (capped != nullptr)
            {
                *capped = true;
            }
            continue;
        }
        NearBranch branch;
        branch.offset = static_cast<std::uint32_t>(index);
        branch.opcode = recorded_opcode;
        branch.near_form = near_form;
        branch.target = base_address + static_cast<std::uint32_t>(index) +
                        static_cast<std::uint32_t>(instruction_length) + relative;
        branches.push_back(branch);
    }
    if (total_branches != nullptr)
    {
        *total_branches = total;
    }
    return branches;
}

std::vector<RelativeBranchSite> ScanRelativeBranches(const std::uint8_t* bytes,
                                                     std::size_t size,
                                                     std::uint32_t base_address,
                                                     std::uint32_t target_address,
                                                     std::size_t max_sites,
                                                     bool* capped,
                                                     std::size_t* total_sites)
{
    std::vector<RelativeBranchSite> sites;
    std::size_t total = 0;
    if (capped != nullptr)
    {
        *capped = false;
    }
    if (total_sites != nullptr)
    {
        *total_sites = 0;
    }
    constexpr std::size_t kBranchLength = 5;
    if (bytes == nullptr || size < kBranchLength)
    {
        return sites;
    }
    for (std::size_t index = 0; index + kBranchLength <= size; ++index)
    {
        const std::uint8_t opcode = bytes[index];
        if (opcode != 0xe8 && opcode != 0xe9)
        {
            continue;
        }
        const std::uint32_t relative =
            static_cast<std::uint32_t>(bytes[index + 1]) |
            (static_cast<std::uint32_t>(bytes[index + 2]) << 8) |
            (static_cast<std::uint32_t>(bytes[index + 3]) << 16) |
            (static_cast<std::uint32_t>(bytes[index + 4]) << 24);
        // x86 relative branches are taken from the address after the
        // instruction, and the sum wraps within 32 bits.
        const std::uint32_t destination =
            base_address + static_cast<std::uint32_t>(index) +
            static_cast<std::uint32_t>(kBranchLength) + relative;
        if (destination != target_address)
        {
            continue;
        }
        ++total;
        if (sites.size() >= max_sites)
        {
            if (capped != nullptr)
            {
                *capped = true;
            }
            continue;
        }
        RelativeBranchSite site;
        site.offset = static_cast<std::uint32_t>(index);
        site.opcode = opcode;
        sites.push_back(site);
    }
    if (total_sites != nullptr)
    {
        *total_sites = total;
    }
    return sites;
}

}  // namespace re2dj::exe
