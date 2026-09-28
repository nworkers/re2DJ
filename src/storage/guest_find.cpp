#include "re2dj/storage/guest_find.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace re2dj::storage
{
namespace
{

bool Match(const char* pattern, const char* pattern_end, const char* text, const char* text_end)
{
    while (pattern != pattern_end)
    {
        if (*pattern == '*')
        {
            ++pattern;
            if (pattern == pattern_end)
            {
                return true;
            }
            for (; text != text_end; ++text)
            {
                if (Match(pattern, pattern_end, text, text_end))
                {
                    return true;
                }
            }
            return false;
        }
        if (text == text_end)
        {
            return false;
        }
        if (*pattern != '?' &&
            std::tolower(static_cast<unsigned char>(*pattern)) != std::tolower(static_cast<unsigned char>(*text)))
        {
            return false;
        }
        ++pattern;
        ++text;
    }
    return text == text_end;
}

bool IsLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

void SplitFileTime(std::optional<std::uint64_t> value, std::uint32_t* out)
{
    if (value.has_value())
    {
        out[0] = static_cast<std::uint32_t>(*value);
        out[1] = static_cast<std::uint32_t>(*value >> 32);
    }
}

}  // namespace

bool MatchesFindPattern(std::string_view pattern, std::string_view name)
{
    if (pattern == "*.*" || pattern == "*")
    {
        return true;
    }
    return Match(pattern.data(), pattern.data() + pattern.size(), name.data(), name.data() + name.size());
}

std::optional<std::uint64_t> DosDateTimeToFileTime(std::uint16_t date, std::uint16_t time)
{
    const int day = date & 0x1F;
    const int month = (date >> 5) & 0x0F;
    const int year = 1980 + (date >> 9);
    const int second = (time & 0x1F) * 2;
    const int minute = (time >> 5) & 0x3F;
    const int hour = time >> 11;
    constexpr int kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12 || day < 1 || hour > 23 || minute > 59 || second > 59)
    {
        return std::nullopt;
    }
    const int month_days = kDaysInMonth[month - 1] + (month == 2 && IsLeapYear(year) ? 1 : 0);
    if (day > month_days)
    {
        return std::nullopt;
    }
    // Days from 1601-01-01 to the start of the year, then into it.
    std::int64_t days = 0;
    for (int y = 1601; y < year; ++y)
    {
        days += IsLeapYear(y) ? 366 : 365;
    }
    for (int m = 1; m < month; ++m)
    {
        days += kDaysInMonth[m - 1] + (m == 2 && IsLeapYear(year) ? 1 : 0);
    }
    days += day - 1;
    const std::int64_t seconds = days * 86400 + hour * 3600 + minute * 60 + second;
    return static_cast<std::uint64_t>(seconds) * 10000000ULL;
}

GuestFindData DescribeFindEntry(const Fat32Entry& entry)
{
    GuestFindData data;
    data.attributes = EntryAttributes(entry.directory);
    data.size_low = entry.size;
    if (entry.creation_date != 0)
    {
        SplitFileTime(DosDateTimeToFileTime(entry.creation_date, entry.creation_time), data.creation_time);
    }
    if (entry.last_access_date != 0)
    {
        SplitFileTime(DosDateTimeToFileTime(entry.last_access_date, 0), data.last_access_time);
    }
    if (entry.write_date != 0)
    {
        SplitFileTime(DosDateTimeToFileTime(entry.write_date, entry.write_time), data.last_write_time);
    }
    const std::size_t length = std::min(entry.name.size(), sizeof(data.file_name) - 1);
    std::memcpy(data.file_name, entry.name.data(), length);
    return data;
}

std::uint32_t EntryAttributes(bool directory)
{
    return directory ? kFileAttributeDirectory : kFileAttributeNormal;
}

GuestFileAttributes DescribeGuestFileAttributes(const std::vector<std::string>& below_root,
                                                bool trailing_separator,
                                                const std::function<GuestEntryKind(const std::string&)>& lookup)
{
    // Win32 error codes, as the hle and Windows layers name them.
    constexpr std::uint32_t kErrorFileNotFound = 2;
    constexpr std::uint32_t kErrorPathNotFound = 3;
    constexpr std::uint32_t kErrorDirectory = 267;
    GuestFileAttributes result;
    if (below_root.empty())
    {
        result.attributes = EntryAttributes(true);
        return result;
    }
    std::string relative;
    for (std::size_t index = 0; index < below_root.size(); ++index)
    {
        relative += (index == 0 ? "" : "/") + below_root[index];
        const bool last = index + 1 == below_root.size();
        switch (lookup(relative))
        {
        case GuestEntryKind::kMissing:
            result.error = last ? kErrorFileNotFound : kErrorPathNotFound;
            return result;
        case GuestEntryKind::kFile:
            if (!last)
            {
                result.error = kErrorPathNotFound;
                return result;
            }
            if (trailing_separator)
            {
                result.error = kErrorDirectory;
                return result;
            }
            result.attributes = EntryAttributes(false);
            return result;
        case GuestEntryKind::kDirectory:
            break;
        }
    }
    result.attributes = EntryAttributes(true);
    return result;
}

}  // namespace re2dj::storage
