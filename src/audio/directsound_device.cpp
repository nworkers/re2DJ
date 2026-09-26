#include "re2dj/audio/directsound_device.h"

#include <algorithm>

namespace re2dj::audio
{

SoundBufferPlan PlanSoundBuffer(const DsBufferDesc& description, const WaveFormatEx* format)
{
    SoundBufferPlan plan;
    if (description.size < kDsBufferDesc1Size)
    {
        plan.result = kDsErrInvalidParam;
        return plan;
    }
    plan.flags = description.flags;
    plan.primary = (description.flags & kDsbcapsPrimaryBuffer) != 0;
    if (plan.primary)
    {
        plan.bytes = kPrimaryBufferBytes;
        plan.format = kPrimaryFormat;
        return plan;
    }
    plan.bytes = description.buffer_bytes;
    if (format == nullptr || format->format_tag != kWaveFormatPcm || plan.bytes == 0 ||
        plan.bytes > kMaxSoundBufferBytes)
    {
        plan.result = kDsErrBadFormat;
        return plan;
    }
    plan.format = *format;
    return plan;
}

DsCaps DeviceCaps()
{
    DsCaps caps;
    caps.size = sizeof(DsCaps);
    caps.flags = kDscapsPrimaryStereo | kDscaps16Bit;
    return caps;
}

DsbCaps BufferCaps(std::uint32_t flags, std::uint32_t bytes)
{
    DsbCaps caps;
    caps.size = sizeof(DsbCaps);
    caps.flags = flags;
    caps.buffer_bytes = bytes;
    return caps;
}

std::uint32_t CheckDuplicate(bool source_is_primary)
{
    return source_is_primary ? kDsErrInvalidCall : kDsOk;
}

bool PlanLock(std::uint32_t buffer_bytes,
              std::uint32_t offset,
              std::uint32_t bytes,
              std::uint32_t flags,
              LockRegions* regions)
{
    if (buffer_bytes == 0)
    {
        return false;
    }
    if ((flags & kDsbLockEntireBuffer) != 0)
    {
        offset = 0;
        bytes = buffer_bytes;
    }
    if (offset >= buffer_bytes || bytes > buffer_bytes)
    {
        return false;
    }
    regions->first_offset = offset;
    regions->first_bytes = std::min(bytes, buffer_bytes - offset);
    regions->second_bytes = bytes - regions->first_bytes;
    return true;
}

}  // namespace re2dj::audio
