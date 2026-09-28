#ifndef RE2DJ_INPUT_LEGACY_IO_TRAP_H_
#define RE2DJ_INPUT_LEGACY_IO_TRAP_H_

#include <cstdint>
#include <optional>

// Which trapped guest instructions are the I/O board's port accesses. A user
// mode `in` or `out` faults, and the host answers it through the board's port
// bus instead. These rules decide which faults qualify, on either host.
namespace re2dj::input
{

// A profile's port-access contract, placed at the loaded main image.
struct LegacyIoTrapPolicy
{
    bool enabled = false;
    // The EZ2Dancer board is word-wide; its instructions carry a 0x66
    // operand-size prefix. The EZ2DJ board is byte-wide.
    bool word_width = false;
    std::uint32_t image_base = 0;
    // Main-image RVAs of the confirmed input and output helpers' first bytes;
    // zero when a direction's helper is unknown.
    std::uint32_t in_rva = 0;
    std::uint32_t out_rva = 0;
};

// One port access the policy claims.
struct LegacyIoAccess
{
    bool read = false;
    bool word = false;
    // Bytes to step EIP past the instruction: the opcode, plus the prefix of a
    // word access.
    std::uint32_t length = 1;
};

// Decides whether the instruction at address, whose first two bytes are given,
// is a board access:
// - its width must be the profile's (a 0x66 prefix exactly for a word board);
// - an access at a configured helper's address takes that helper's direction;
// - a direction whose helper is unknown is judged by opcode alone (IN DX: 0xEC
//   byte, 0xED word; OUT DX: 0xEE byte, 0xEF word);
// - the opcode must then match the direction and width.
// Anything else is not the board's (nullopt), and stays a fault.
std::optional<LegacyIoAccess> DecodeLegacyIoAccess(const LegacyIoTrapPolicy& policy,
                                                   std::uint32_t address,
                                                   std::uint8_t first,
                                                   std::uint8_t second);

// EAX after a read: only the operand's own width is replaced; the rest belongs
// to the guest.
std::uint32_t MergeLegacyIoRead(const LegacyIoAccess& access, std::uint32_t eax, std::uint16_t value);

}  // namespace re2dj::input

#endif  // RE2DJ_INPUT_LEGACY_IO_TRAP_H_
