#include "re2dj/input/legacy_io_trap.h"

#include <cstdint>

#include "test_support.h"

namespace
{

namespace input = re2dj::input;

input::LegacyIoTrapPolicy FourthPolicy()
{
    input::LegacyIoTrapPolicy policy;
    policy.enabled = true;
    policy.image_base = 0x00400000U;
    policy.in_rva = 0x000c3817U;
    policy.out_rva = 0x000c384bU;
    return policy;
}

// The 4th's byte-wide helpers: `in al, dx` and `out dx, al` at their RVAs.
void CheckByteHelpers(re2dj::test::Context& context)
{
    const input::LegacyIoTrapPolicy policy = FourthPolicy();
    const auto read = input::DecodeLegacyIoAccess(policy, 0x004c3817U, 0xEC, 0);
    RE2DJ_CHECK(context, read.has_value() && read->read && !read->word && read->length == 1);
    const auto write = input::DecodeLegacyIoAccess(policy, 0x004c384bU, 0xEE, 0);
    RE2DJ_CHECK(context, write.has_value() && !write->read);
    // A helper's address takes that helper's direction only.
    RE2DJ_CHECK(context, !input::DecodeLegacyIoAccess(policy, 0x004c3817U, 0xEE, 0).has_value());
    // Elsewhere, with both helpers known, nothing is the board's.
    RE2DJ_CHECK(context, !input::DecodeLegacyIoAccess(policy, 0x00401000U, 0xEC, 0).has_value());
    // A word access on a byte board, and a disabled policy, are faults.
    RE2DJ_CHECK(context, !input::DecodeLegacyIoAccess(policy, 0x004c3817U, 0x66, 0xED).has_value());
    input::LegacyIoTrapPolicy disabled = policy;
    disabled.enabled = false;
    RE2DJ_CHECK(context, !input::DecodeLegacyIoAccess(disabled, 0x004c3817U, 0xEC, 0).has_value());
}

// An unknown helper is judged by opcode; a word board's instructions carry
// the 0x66 prefix, which the length includes.
void CheckOpcodeFallbackAndWords(re2dj::test::Context& context)
{
    input::LegacyIoTrapPolicy policy = FourthPolicy();
    policy.out_rva = 0;
    RE2DJ_CHECK(context, input::DecodeLegacyIoAccess(policy, 0x00401000U, 0xEE, 0).has_value());
    RE2DJ_CHECK(context, !input::DecodeLegacyIoAccess(policy, 0x00401000U, 0xEC, 0).has_value());

    input::LegacyIoTrapPolicy word;
    word.enabled = true;
    word.word_width = true;
    word.image_base = 0x00400000U;
    const auto access = input::DecodeLegacyIoAccess(word, 0x00401000U, 0x66, 0xED);
    RE2DJ_CHECK(context, access.has_value() && access->read && access->word && access->length == 2);
    RE2DJ_CHECK(context, !input::DecodeLegacyIoAccess(word, 0x00401000U, 0xED, 0).has_value());
}

// A read replaces only the operand's width of EAX.
void CheckMerge(re2dj::test::Context& context)
{
    input::LegacyIoAccess byte_read;
    byte_read.read = true;
    RE2DJ_CHECK_EQ(context, input::MergeLegacyIoRead(byte_read, 0x12345678U, 0x00AB), 0x123456ABU);
    input::LegacyIoAccess word_read = byte_read;
    word_read.word = true;
    RE2DJ_CHECK_EQ(context, input::MergeLegacyIoRead(word_read, 0x12345678U, 0xBEEF), 0x1234BEEFU);
}

}  // namespace

void RunLegacyIoTrapTests(re2dj::test::Context& context)
{
    CheckByteHelpers(context);
    CheckOpcodeFallbackAndWords(context);
    CheckMerge(context);
}
