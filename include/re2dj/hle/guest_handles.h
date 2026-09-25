#ifndef RE2DJ_HLE_GUEST_HANDLES_H_
#define RE2DJ_HLE_GUEST_HANDLES_H_

#include <cstdint>

namespace re2dj::hle
{

// Win32 handle values share one space whatever object they name. Values follow
// kernel handles: multiples of four, never 0 or INVALID_HANDLE_VALUE.
class GuestHandleAllocator
{
public:
    static constexpr std::uint32_t kFirstHandle = 0x00001004U;

    std::uint32_t Allocate()
    {
        const std::uint32_t handle = next_;
        next_ += 4;
        return handle;
    }

private:
    std::uint32_t next_ = kFirstHandle;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_HANDLES_H_
