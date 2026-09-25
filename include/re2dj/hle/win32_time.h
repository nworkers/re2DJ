#ifndef RE2DJ_HLE_WIN32_TIME_H_
#define RE2DJ_HLE_WIN32_TIME_H_

#include <array>
#include <cstdint>
#include <optional>

namespace re2dj::hle
{

// A SYSTEMTIME (minwinbase.h): year, month, day of week (0 = Sunday), day,
// hour, minute, second, millisecond.
struct Win32SystemTime
{
    std::uint16_t year = 0;
    std::uint16_t month = 0;
    std::uint16_t day_of_week = 0;
    std::uint16_t day = 0;
    std::uint16_t hour = 0;
    std::uint16_t minute = 0;
    std::uint16_t second = 0;
    std::uint16_t millisecond = 0;
};

// FILETIME counts 100-nanosecond intervals since 1601-01-01 00:00 UTC.
inline constexpr std::uint64_t kFileTimeTicksPerSecond = 10000000ULL;
inline constexpr std::uint64_t kFileTimeUnixEpoch = 116444736000000000ULL;

// FileTimeToSystemTime: exact for any FILETIME below 2^63.
Win32SystemTime FileTimeToSystemTime(std::uint64_t file_time);
// SystemTimeToFileTime: nothing for a field out of range (month 1-12, a day
// the month has, hour < 24, minute and second < 60, millisecond < 1000, year
// 1601-30827). The day of week is ignored, as on Windows.
std::optional<std::uint64_t> SystemTimeToFileTime(const Win32SystemTime& time);

// The 16 bytes of a SYSTEMTIME as the guest lays them out.
std::array<std::uint8_t, 16> EncodeSystemTime(const Win32SystemTime& time);
Win32SystemTime DecodeSystemTime(const std::array<std::uint8_t, 16>& bytes);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_WIN32_TIME_H_
