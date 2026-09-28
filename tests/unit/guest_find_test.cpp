#include "re2dj/storage/guest_find.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

#include "test_support.h"

namespace
{

namespace storage = re2dj::storage;

// The Windows product's pattern rules.
void CheckPatterns(re2dj::test::Context& context)
{
    RE2DJ_CHECK(context, storage::MatchesFindPattern("*.*", "anything"));
    RE2DJ_CHECK(context, storage::MatchesFindPattern("*", "noext"));
    RE2DJ_CHECK(context, storage::MatchesFindPattern("*.ez", "SONG.EZ"));
    RE2DJ_CHECK(context, !storage::MatchesFindPattern("*.ez", "song.ezw"));
    RE2DJ_CHECK(context, storage::MatchesFindPattern("s?ng*", "Sung01.ez"));
    RE2DJ_CHECK(context, !storage::MatchesFindPattern("s?ng", "sng"));
    RE2DJ_CHECK(context, storage::MatchesFindPattern("Songs", "SONGS"));
    RE2DJ_CHECK(context, !storage::MatchesFindPattern("a*b", "acd"));
}

// DOS stamps as Win32 converts them: no time zone, invalid ones refused.
void CheckTimes(re2dj::test::Context& context)
{
    // 2025-01-01 12:00:00.
    const auto noon = storage::DosDateTimeToFileTime(0x5A21, 0x6000);
    RE2DJ_CHECK(context, noon.has_value() && *noon == 133802064000000000ULL);
    RE2DJ_CHECK(context, !storage::DosDateTimeToFileTime(0x5A20, 0).has_value());  // day 0
    RE2DJ_CHECK(context, !storage::DosDateTimeToFileTime(0x5A01, 0).has_value());  // month 0
    RE2DJ_CHECK(context, !storage::DosDateTimeToFileTime(0x5A21, 0xC000).has_value());  // hour 24
#if defined(_WIN32)
    // Every date, at a few times, against Windows' own conversion.
    const std::uint16_t times[] = {0x0000, 0x6000, 0xBF7D, 0xBF7E, 0x001E};
    std::uint32_t mismatches = 0;
    for (std::uint32_t date = 0; date <= 0xFFFF; ++date)
    {
        for (const std::uint16_t time : times)
        {
            FILETIME windows = {};
            const bool converted =
                DosDateTimeToFileTime(static_cast<WORD>(date), time, &windows) != FALSE;
            const auto ours = storage::DosDateTimeToFileTime(static_cast<std::uint16_t>(date), time);
            const std::uint64_t value =
                (static_cast<std::uint64_t>(windows.dwHighDateTime) << 32) | windows.dwLowDateTime;
            if (converted != ours.has_value() || (converted && value != *ours))
            {
                ++mismatches;
            }
        }
    }
    RE2DJ_CHECK_EQ(context, mismatches, 0U);
#endif
}

// An entry as WIN32_FIND_DATAA.
void CheckEntries(re2dj::test::Context& context)
{
    storage::Fat32Entry file;
    file.name = "SONG.EZ";
    file.size = 1234;
    file.write_date = 0x5A21;
    file.write_time = 0x6000;
    const storage::GuestFindData data = storage::DescribeFindEntry(file);
    RE2DJ_CHECK_EQ(context, data.attributes, storage::kFileAttributeNormal);
    RE2DJ_CHECK_EQ(context, data.size_low, 1234U);
    RE2DJ_CHECK_EQ(context, std::string(data.file_name), std::string("SONG.EZ"));
    RE2DJ_CHECK_EQ(context, data.last_write_time[0], static_cast<std::uint32_t>(133802064000000000ULL));
    RE2DJ_CHECK_EQ(context, data.last_write_time[1], static_cast<std::uint32_t>(133802064000000000ULL >> 32));
    RE2DJ_CHECK_EQ(context, data.creation_time[0] | data.creation_time[1], 0U);
    storage::Fat32Entry directory;
    directory.name = std::string(300, 'D');
    directory.directory = true;
    const storage::GuestFindData folder = storage::DescribeFindEntry(directory);
    RE2DJ_CHECK_EQ(context, folder.attributes, storage::kFileAttributeDirectory);
    RE2DJ_CHECK_EQ(context, std::strlen(folder.file_name), std::size_t{259});
}

// GetFileAttributesA's walk over a host's lookup, with the errors Windows 11
// gives: the lookup sees each prefix in turn and the walk stops at the first
// that is not a directory.
void CheckFileAttributesWalk(re2dj::test::Context& context)
{
    std::vector<std::string> seen;
    const auto lookup = [&](const std::string& relative) {
        seen.push_back(relative);
        if (relative == "A" || relative == "A/B")
        {
            return storage::GuestEntryKind::kDirectory;
        }
        if (relative == "A/f.bin")
        {
            return storage::GuestEntryKind::kFile;
        }
        return storage::GuestEntryKind::kMissing;
    };
    storage::GuestFileAttributes result = storage::DescribeGuestFileAttributes({"A", "f.bin"}, false, lookup);
    RE2DJ_CHECK_EQ(context, result.attributes, storage::kFileAttributeNormal);
    RE2DJ_CHECK_EQ(context, result.error, 0U);
    RE2DJ_CHECK_EQ(context, seen.size(), std::size_t{2});
    RE2DJ_CHECK_EQ(context, seen[1], std::string("A/f.bin"));
    result = storage::DescribeGuestFileAttributes({"A", "B"}, true, lookup);
    RE2DJ_CHECK_EQ(context, result.attributes, storage::kFileAttributeDirectory);
    // The root itself asks nothing.
    seen.clear();
    result = storage::DescribeGuestFileAttributes({}, false, lookup);
    RE2DJ_CHECK_EQ(context, result.attributes, storage::kFileAttributeDirectory);
    RE2DJ_CHECK(context, seen.empty());

    const auto error = [&](std::vector<std::string> below, bool trailing) {
        const storage::GuestFileAttributes failed = storage::DescribeGuestFileAttributes(below, trailing, lookup);
        RE2DJ_CHECK_EQ(context, failed.attributes, storage::kInvalidFileAttributes);
        return failed.error;
    };
    RE2DJ_CHECK_EQ(context, error({"A", "none"}, false), 2U);
    RE2DJ_CHECK_EQ(context, error({"none", "f.bin"}, false), 3U);
    RE2DJ_CHECK_EQ(context, error({"A", "f.bin", "x"}, false), 3U);
    RE2DJ_CHECK_EQ(context, error({"A", "f.bin"}, true), 267U);
}

}  // namespace

void RunGuestFindTests(re2dj::test::Context& context)
{
    CheckPatterns(context);
    CheckTimes(context);
    CheckEntries(context);
    CheckFileAttributesWalk(context);
}
