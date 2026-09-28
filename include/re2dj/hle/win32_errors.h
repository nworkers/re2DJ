#ifndef RE2DJ_HLE_WIN32_ERRORS_H_
#define RE2DJ_HLE_WIN32_ERRORS_H_

#include <cstdint>

namespace re2dj::hle
{

// Win32 last-error values HLE handlers report (winerror.h, "System Error
// Codes (0-499)" and "(500-999)" on Microsoft Learn).
inline constexpr std::uint32_t kWin32ErrorSuccess = 0;
inline constexpr std::uint32_t kWin32ErrorInvalidFunction = 1;
inline constexpr std::uint32_t kWin32ErrorFileNotFound = 2;
inline constexpr std::uint32_t kWin32ErrorPathNotFound = 3;
inline constexpr std::uint32_t kWin32ErrorAccessDenied = 5;
inline constexpr std::uint32_t kWin32ErrorInvalidHandle = 6;
inline constexpr std::uint32_t kWin32ErrorNotEnoughMemory = 8;
inline constexpr std::uint32_t kWin32ErrorInvalidData = 13;
inline constexpr std::uint32_t kWin32ErrorNoMoreFiles = 18;
inline constexpr std::uint32_t kWin32ErrorWriteFault = 29;
inline constexpr std::uint32_t kWin32ErrorReadFault = 30;
inline constexpr std::uint32_t kWin32ErrorFileExists = 80;
inline constexpr std::uint32_t kWin32ErrorInvalidParameter = 87;
inline constexpr std::uint32_t kWin32ErrorInsufficientBuffer = 122;
inline constexpr std::uint32_t kWin32ErrorInvalidName = 123;
inline constexpr std::uint32_t kWin32ErrorModuleNotFound = 126;
inline constexpr std::uint32_t kWin32ErrorNegativeSeek = 131;
inline constexpr std::uint32_t kWin32ErrorAlreadyExists = 183;
inline constexpr std::uint32_t kWin32ErrorDirectory = 267;
inline constexpr std::uint32_t kWin32ErrorEnvironmentVariableNotFound = 203;
inline constexpr std::uint32_t kWin32ErrorPartialCopy = 299;
inline constexpr std::uint32_t kWin32ErrorInvalidAddress = 487;
inline constexpr std::uint32_t kWin32ErrorNoAccess = 998;
inline constexpr std::uint32_t kWin32ErrorInvalidWindowHandle = 1400;
inline constexpr std::uint32_t kWin32ErrorClassAlreadyExists = 1410;
inline constexpr std::uint32_t kWin32ErrorInvalidIndex = 1413;
inline constexpr std::uint32_t kWin32ErrorResourceTypeNotFound = 1813;
inline constexpr std::uint32_t kWin32ErrorResourceNameNotFound = 1814;

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_WIN32_ERRORS_H_
