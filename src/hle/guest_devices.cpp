#include "re2dj/hle/guest_devices.h"

#include <algorithm>
#include <utility>

#include "re2dj/hle/guest_device_path.h"

namespace re2dj::hle
{

GuestDeviceSet::GuestDeviceSet(GuestDeviceConfig config)
    : config_(std::move(config)),
      hardlock_device_(config_.hardlock.value_or(hardlock::HardlockDeviceOptions{}))
{
}

std::uint32_t GuestDeviceSet::Open(std::string_view name)
{
    if (!MatchesGuestDevicePrefix(name, config_.device_path_prefix))
    {
        return 0;
    }
    const std::uint32_t handle =
        shared_handles_ != nullptr ? shared_handles_->Allocate() : own_handles_.Allocate();
    open_handles_.push_back(handle);
    return handle;
}

bool GuestDeviceSet::IsOpen(std::uint32_t handle) const
{
    return std::find(open_handles_.begin(), open_handles_.end(), handle) != open_handles_.end();
}

bool GuestDeviceSet::Close(std::uint32_t handle)
{
    const auto found = std::find(open_handles_.begin(), open_handles_.end(), handle);
    if (found == open_handles_.end())
    {
        return false;
    }
    open_handles_.erase(found);
    return true;
}

hardlock::HardlockDeviceCall GuestDeviceSet::Control(std::uint32_t handle,
                                                     std::uint32_t control_code,
                                                     std::span<const std::uint8_t> input,
                                                     std::span<std::uint8_t> output,
                                                     std::uint64_t tick_ms)
{
    if (!IsOpen(handle))
    {
        return {};
    }
    hardlock::HardlockDeviceCall call = hardlock::CompleteHardlockDeviceIoControl(
        hardlock_device_, control_code, input, output);
    if (call.handled)
    {
        hardlock::RecordHardlockDeviceCall(call.result, tick_ms, &activity_);
    }
    return call;
}

}  // namespace re2dj::hle
