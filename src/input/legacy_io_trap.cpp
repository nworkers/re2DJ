#include "re2dj/input/legacy_io_trap.h"

namespace re2dj::input
{

std::optional<LegacyIoAccess> DecodeLegacyIoAccess(const LegacyIoTrapPolicy& policy,
                                                   std::uint32_t address,
                                                   std::uint8_t first,
                                                   std::uint8_t second)
{
    if (!policy.enabled || policy.image_base == 0)
    {
        return std::nullopt;
    }
    // A word-wide port instruction in 32-bit code carries a 0x66 operand-size
    // prefix, so the faulting address is that prefix and the opcode follows
    // it. Unprefixed 0xED and 0xEF are 32-bit accesses, which no supported
    // product has been observed using.
    const bool word_prefixed = first == 0x66;
    if (word_prefixed != policy.word_width)
    {
        return std::nullopt;
    }
    const std::uint8_t opcode = word_prefixed ? second : first;
    const std::uint8_t read_opcode = policy.word_width ? 0xED : 0xEC;
    const std::uint8_t write_opcode = policy.word_width ? 0xEF : 0xEE;

    const bool configured_read = policy.in_rva != 0 && address == policy.image_base + policy.in_rva;
    const bool configured_write = policy.out_rva != 0 && address == policy.image_base + policy.out_rva;
    // Bring-up reaches one direction before the other, and the width is
    // pinned by the profile, so an unknown helper's opcode is unambiguous.
    const bool read_by_opcode = policy.in_rva == 0 && opcode == read_opcode;
    const bool write_by_opcode = policy.out_rva == 0 && opcode == write_opcode;
    const bool read = configured_read || (!configured_write && read_by_opcode);
    const bool write = configured_write || (!configured_read && write_by_opcode);
    if ((!read && !write) || opcode != (read ? read_opcode : write_opcode))
    {
        return std::nullopt;
    }
    LegacyIoAccess access;
    access.read = read;
    access.word = policy.word_width;
    access.length = word_prefixed ? 2U : 1U;
    return access;
}

std::uint32_t MergeLegacyIoRead(const LegacyIoAccess& access, std::uint32_t eax, std::uint16_t value)
{
    return access.word ? ((eax & 0xFFFF0000U) | value) : ((eax & 0xFFFFFF00U) | (value & 0xFFU));
}

}  // namespace re2dj::input
