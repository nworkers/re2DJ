#ifndef RE2DJ_PLATFORM_WINDOWS_NATIVE_GUEST_RESERVATION_H_
#define RE2DJ_PLATFORM_WINDOWS_NATIVE_GUEST_RESERVATION_H_

#include <cstddef>
#include <cstdint>

namespace re2dj::platform::native
{

// Gives up the early reservation of the guest images' range when
// [address, address + size) lies in it, so the caller can map there at once.
// False when there is no reservation or the range lies outside it.
bool ReleaseGuestImageReservation(std::uintptr_t address, std::size_t size);
// Whether the range is still held, for diagnostics.
bool GuestImageRangeReserved();

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_WINDOWS_NATIVE_GUEST_RESERVATION_H_
