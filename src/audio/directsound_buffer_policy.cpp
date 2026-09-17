#include "re2dj/audio/directsound_buffer_policy.h"

namespace re2dj::audio
{

bool IsStreamingBufferDescription(std::uint32_t flags, std::uint32_t bytes)
{
    if (bytes == kStreamingRingBytes)
    {
        return true;
    }
    const bool position_flags = (flags & (kDsbcapsLocHardware | kDsbcapsGetCurrentPosition2)) != 0;
    return position_flags && (flags & kDsbcapsStatic) == 0;
}

}  // namespace re2dj::audio
