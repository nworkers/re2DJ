#include "re2dj/hle/win32_time.h"

#include <cstdint>

namespace re2dj::hle
{
namespace
{

constexpr std::int64_t kSecondsPerDay = 86400;
// Days from 1601-01-01 to 1970-01-01.
constexpr std::int64_t kDaysFrom1601To1970 = 134774;

// Days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's
// days_from_civil).
std::int64_t DaysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day)
{
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t year_of_era = year - era * 400;
    const std::int64_t day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + day_of_era - 719468;
}

// The inverse (civil_from_days).
void CivilFromDays(std::int64_t days, std::int64_t* year, std::int64_t* month, std::int64_t* day)
{
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t day_of_era = days - era * 146097;
    const std::int64_t year_of_era =
        (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    const std::int64_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const std::int64_t month_index = (5 * day_of_year + 2) / 153;
    *day = day_of_year - (153 * month_index + 2) / 5 + 1;
    *month = month_index + (month_index < 10 ? 3 : -9);
    *year = year_of_era + era * 400 + (*month <= 2 ? 1 : 0);
}

bool IsLeapYear(std::int64_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

std::int64_t DaysInMonth(std::int64_t year, std::int64_t month)
{
    constexpr std::int64_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && IsLeapYear(year) ? 29 : kDays[month - 1];
}

}  // namespace

Win32SystemTime FileTimeToSystemTime(std::uint64_t file_time)
{
    const auto total_seconds = static_cast<std::int64_t>(file_time / kFileTimeTicksPerSecond);
    const std::int64_t days_since_1601 = total_seconds / kSecondsPerDay;
    const std::int64_t second_of_day = total_seconds % kSecondsPerDay;
    std::int64_t year = 0;
    std::int64_t month = 0;
    std::int64_t day = 0;
    CivilFromDays(days_since_1601 - kDaysFrom1601To1970, &year, &month, &day);
    Win32SystemTime time;
    time.year = static_cast<std::uint16_t>(year);
    time.month = static_cast<std::uint16_t>(month);
    time.day = static_cast<std::uint16_t>(day);
    // 1601-01-01 was a Monday.
    time.day_of_week = static_cast<std::uint16_t>((days_since_1601 + 1) % 7);
    time.hour = static_cast<std::uint16_t>(second_of_day / 3600);
    time.minute = static_cast<std::uint16_t>(second_of_day / 60 % 60);
    time.second = static_cast<std::uint16_t>(second_of_day % 60);
    time.millisecond = static_cast<std::uint16_t>(file_time % kFileTimeTicksPerSecond / 10000);
    return time;
}

std::optional<std::uint64_t> SystemTimeToFileTime(const Win32SystemTime& time)
{
    if (time.year < 1601 || time.year > 30827 || time.month < 1 || time.month > 12 ||
        time.day < 1 || time.day > DaysInMonth(time.year, time.month) || time.hour > 23 ||
        time.minute > 59 || time.second > 59 || time.millisecond > 999)
    {
        return std::nullopt;
    }
    const std::int64_t days = DaysFromCivil(time.year, time.month, time.day) + kDaysFrom1601To1970;
    const std::int64_t seconds =
        days * kSecondsPerDay + time.hour * 3600 + time.minute * 60 + time.second;
    return static_cast<std::uint64_t>(seconds) * kFileTimeTicksPerSecond +
           static_cast<std::uint64_t>(time.millisecond) * 10000;
}

std::array<std::uint8_t, 16> EncodeSystemTime(const Win32SystemTime& time)
{
    const std::uint16_t fields[8] = {time.year, time.month, time.day_of_week, time.day,
                                     time.hour, time.minute, time.second, time.millisecond};
    std::array<std::uint8_t, 16> bytes = {};
    for (std::size_t index = 0; index < 8; ++index)
    {
        bytes[index * 2] = static_cast<std::uint8_t>(fields[index]);
        bytes[index * 2 + 1] = static_cast<std::uint8_t>(fields[index] >> 8);
    }
    return bytes;
}

Win32SystemTime DecodeSystemTime(const std::array<std::uint8_t, 16>& bytes)
{
    const auto field = [&bytes](std::size_t index)
    {
        return static_cast<std::uint16_t>(bytes[index * 2] | (bytes[index * 2 + 1] << 8));
    };
    return {field(0), field(1), field(2), field(3), field(4), field(5), field(6), field(7)};
}

}  // namespace re2dj::hle
