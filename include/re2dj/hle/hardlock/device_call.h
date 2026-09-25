#ifndef RE2DJ_HLE_HARDLOCK_DEVICE_CALL_H_
#define RE2DJ_HLE_HARDLOCK_DEVICE_CALL_H_

#include <cstdint>
#include <span>
#include <string>

#include "re2dj/hle/hardlock/device.h"
#include "re2dj/hle/win32_errors.h"

namespace re2dj::hle::hardlock
{

// One DeviceIoControl answered by the Hardlock device, in Win32 terms.
struct HardlockDeviceCall
{
    // False when the request is outside the device contract; the caller then
    // keeps whatever it would otherwise have done.
    bool handled = false;
    // The DeviceIoControl BOOL result.
    bool succeeded = false;
    std::uint32_t bytes_returned = 0;
    // The value the caller passes to SetLastError.
    std::uint32_t win32_error = kWin32ErrorSuccess;
    HardlockDeviceResult result;
};

// A completed request succeeds with the bytes written; a request with the
// wrong buffer shape fails with ERROR_INVALID_DATA and no bytes.
HardlockDeviceCall CompleteHardlockDeviceIoControl(HardlockDevice& device,
                                                   std::uint32_t control_code,
                                                   std::span<const std::uint8_t> input,
                                                   std::span<std::uint8_t> output);

// Running totals of the requests a process answered, kept for its exit record.
struct HardlockDeviceActivity
{
    unsigned total = 0;
    unsigned initialize = 0;
    unsigned handshake = 0;
    unsigned descriptor = 0;
    unsigned transform = 0;
    unsigned other = 0;
    unsigned rejected = 0;
    const char* last_kind = "none";
    const char* last_outcome = "none";
    unsigned last_bytes = 0;
    std::uint64_t last_tick = 0;
};

void RecordHardlockDeviceCall(const HardlockDeviceResult& result,
                              std::uint64_t tick_ms,
                              HardlockDeviceActivity* activity);

// The trace line for one answered request, ending in "\r\n". It carries only
// request kinds, counts, and Booleans, never material or block bytes.
std::string FormatHardlockDeviceTrace(const HardlockDeviceResult& result, std::uint64_t tick_ms);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_DEVICE_CALL_H_
