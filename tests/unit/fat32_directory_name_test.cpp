#include "re2dj/storage/fat32_directory_name.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <string>
#include <vector>

#include "test_support.h"

namespace
{

using re2dj::storage::DecodeFatShortName;
using re2dj::storage::FatLongNameAssembler;
using re2dj::storage::FatShortNameChecksum;
using re2dj::storage::kFatDirectoryEntryBytes;
using re2dj::storage::kFatLongNameAttribute;
using re2dj::storage::kFatLongNameSlotCharacters;

using Entry = std::array<std::uint8_t, kFatDirectoryEntryBytes>;

// A short entry carrying the eleven 8.3 name bytes exactly as FAT stores them,
// space padded and without the dot.
Entry MakeShortEntry(const char* padded_name)
{
    Entry entry = {};
    std::memcpy(entry.data(), padded_name, 11);
    entry[11] = 0x20;
    return entry;
}

// One long-name slot: `sequence` counts from 1, `last` marks the slot holding
// the end of the name, and `characters` are this slot's thirteen UTF-16 units.
Entry MakeLongNameSlot(std::uint8_t sequence,
                       bool last,
                       std::uint8_t checksum,
                       const std::array<std::uint16_t, kFatLongNameSlotCharacters>& characters)
{
    Entry entry = {};
    entry[0] = static_cast<std::uint8_t>(sequence | (last ? 0x40 : 0x00));
    entry[11] = kFatLongNameAttribute;
    entry[13] = checksum;
    constexpr std::array<std::pair<std::size_t, std::size_t>, 3> ranges = {
        {{1, 5}, {14, 6}, {28, 2}}};
    std::size_t index = 0;
    for (const auto [offset, count] : ranges)
    {
        for (std::size_t step = 0; step < count; ++step)
        {
            const std::uint16_t value = characters[index++];
            entry[offset + step * 2] = static_cast<std::uint8_t>(value & 0xff);
            entry[offset + step * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
        }
    }
    return entry;
}

// Splits `name` into slot-sized groups, padding a partial final group with the
// terminator and 0xffff exactly as a formatter does. A name whose length is a
// multiple of thirteen therefore has no terminator at all, which is the case
// that exposed the assembly defect.
std::vector<std::array<std::uint16_t, kFatLongNameSlotCharacters>> SplitIntoSlots(
    const std::string& name)
{
    std::vector<std::array<std::uint16_t, kFatLongNameSlotCharacters>> slots;
    for (std::size_t start = 0; start < name.size(); start += kFatLongNameSlotCharacters)
    {
        std::array<std::uint16_t, kFatLongNameSlotCharacters> slot = {};
        for (std::size_t index = 0; index < kFatLongNameSlotCharacters; ++index)
        {
            const std::size_t position = start + index;
            slot[index] = position < name.size()
                              ? static_cast<std::uint16_t>(name[position])
                              : (position == name.size() ? 0x0000 : 0xffff);
        }
        slots.push_back(slot);
    }
    return slots;
}

// Feeds the slots for `name` in on-disk order, which is last slot first.
void AddLongName(FatLongNameAssembler* assembler,
                 const std::string& name,
                 std::uint8_t checksum)
{
    const auto slots = SplitIntoSlots(name);
    for (std::size_t index = slots.size(); index != 0; --index)
    {
        const Entry slot = MakeLongNameSlot(static_cast<std::uint8_t>(index),
                                            index == slots.size(),
                                            checksum,
                                            slots[index - 1]);
        assembler->Add(slot.data());
    }
}

std::string DecodeName(const std::string& long_name, const char* padded_short_name)
{
    const Entry short_entry = MakeShortEntry(padded_short_name);
    FatLongNameAssembler assembler;
    AddLongName(&assembler, long_name, FatShortNameChecksum(short_entry.data()));
    std::string decoded;
    if (!assembler.Decode(short_entry.data(), &decoded))
    {
        return DecodeFatShortName(short_entry.data());
    }
    return decoded;
}

}  // namespace

void RunFat32DirectoryNameTests(re2dj::test::Context& context)
{
    // Short names keep their 8.3 shape, with the dot only where an extension
    // exists.
    const Entry credits = MakeShortEntry("CREDITS ABM");
    RE2DJ_CHECK(context, DecodeFatShortName(credits.data()) == "CREDITS.ABM");
    const Entry dotted = MakeShortEntry("TITLE   STR");
    RE2DJ_CHECK(context, DecodeFatShortName(dotted.data()) == "TITLE.STR");
    const Entry bare = MakeShortEntry("SYSTEM     ");
    RE2DJ_CHECK(context, DecodeFatShortName(bare.data()) == "SYSTEM");

    // A name shorter than one slot ends with the terminator inside that slot.
    RE2DJ_CHECK(context, DecodeName("Credits.abm", "CREDITS ABM") == "Credits.abm");

    // Exactly thirteen characters fill one slot with no terminator left. The
    // assembler must stop at the slot boundary rather than reading the short
    // entry that follows it.
    RE2DJ_CHECK(context, DecodeName("Credits_0.abm", "CREDIT~1ABM") == "Credits_0.abm");

    // Two slots, the second holding a single character and the terminator.
    RE2DJ_CHECK(context, DecodeName("INSERTCOIN.str", "INSERT~1STR") == "INSERTCOIN.str");

    // Twenty-one characters across two slots, the case the guest asks for.
    RE2DJ_CHECK(context,
                DecodeName("1PLAYERInsertCoin.str", "1PLAYE~1STR") ==
                    "1PLAYERInsertCoin.str");

    // Twenty-six characters is an exact two-slot fit, again with no terminator.
    RE2DJ_CHECK(context,
                DecodeName("Channel_Eyecatch_mask.abmx", "CHANNE~1ABM") ==
                    "Channel_Eyecatch_mask.abmx");

    // A slot whose checksum disagrees with the short entry is not this entry's
    // name, so the short name stands.
    {
        const Entry short_entry = MakeShortEntry("INSERT~1STR");
        FatLongNameAssembler assembler;
        AddLongName(&assembler,
                    "INSERTCOIN.str",
                    static_cast<std::uint8_t>(FatShortNameChecksum(short_entry.data()) ^ 0xff));
        std::string decoded;
        RE2DJ_CHECK(context, !assembler.Decode(short_entry.data(), &decoded));
    }

    // A missing middle slot leaves a hole in the sequence, which must not
    // silently produce a truncated name.
    {
        const Entry short_entry = MakeShortEntry("1PLAYE~1STR");
        const std::uint8_t checksum = FatShortNameChecksum(short_entry.data());
        const auto slots = SplitIntoSlots("1PLAYERInsertCoin.str");
        FatLongNameAssembler assembler;
        const Entry last = MakeLongNameSlot(static_cast<std::uint8_t>(slots.size()),
                                            true,
                                            checksum,
                                            slots[slots.size() - 1]);
        assembler.Add(last.data());
        std::string decoded;
        RE2DJ_CHECK(context, !assembler.Decode(short_entry.data(), &decoded));
    }

    // Slots collected without the last-slot marker are an incomplete set.
    {
        const Entry short_entry = MakeShortEntry("CREDIT~1ABM");
        const std::uint8_t checksum = FatShortNameChecksum(short_entry.data());
        const auto slots = SplitIntoSlots("Credits_0.abm");
        const Entry slot = MakeLongNameSlot(1, false, checksum, slots[0]);
        FatLongNameAssembler assembler;
        assembler.Add(slot.data());
        std::string decoded;
        RE2DJ_CHECK(context, !assembler.Decode(short_entry.data(), &decoded));
    }

    // Clearing drops everything collected so far.
    {
        const Entry short_entry = MakeShortEntry("CREDIT~1ABM");
        FatLongNameAssembler assembler;
        AddLongName(&assembler, "Credits_0.abm", FatShortNameChecksum(short_entry.data()));
        assembler.Clear();
        std::string decoded;
        RE2DJ_CHECK(context, !assembler.Decode(short_entry.data(), &decoded));
    }
}
