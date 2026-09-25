#ifndef RE2DJ_HLE_GUEST_DEVICES_H_
#define RE2DJ_HLE_GUEST_DEVICES_H_

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/hle/guest_handles.h"
#include "re2dj/hle/hardlock/device.h"
#include "re2dj/hle/hardlock/device_call.h"

namespace re2dj::hle
{

// The devices a profile lets the guest open, such as the Hardlock driver the
// protection talks to through "\\.\FEnteDev".
struct GuestDeviceConfig
{
    // CreateFile names starting with this prefix open the device; empty means
    // the profile provides no device.
    std::string device_path_prefix;
    // Hardlock material for the device, when the profile and the user's cfg
    // supply it. Without it the device still answers with its defaults.
    std::optional<hardlock::HardlockDeviceOptions> hardlock;
};

// Handles for the guest's open devices. Values come from a
// GuestHandleAllocator: the set's own, or the process's once connected.
class GuestDeviceSet
{
public:
    static constexpr std::uint32_t kFirstHandle = GuestHandleAllocator::kFirstHandle;

    GuestDeviceSet() = default;
    explicit GuestDeviceSet(GuestDeviceConfig config);

    // A new handle for a configured device name, or 0 when the name opens no
    // device this set provides.
    std::uint32_t Open(std::string_view name);
    // Takes handles from allocator (not owned) from now on, so device handles
    // share the guest's single handle space.
    void SetHandleAllocator(GuestHandleAllocator* allocator) { shared_handles_ = allocator; }
    bool IsOpen(std::uint32_t handle) const;
    bool Close(std::uint32_t handle);

    // Answers a DeviceIoControl on an open handle. An unknown handle, or a
    // code outside the device contract, comes back with handled == false.
    hardlock::HardlockDeviceCall Control(std::uint32_t handle,
                                         std::uint32_t control_code,
                                         std::span<const std::uint8_t> input,
                                         std::span<std::uint8_t> output,
                                         std::uint64_t tick_ms);

    const hardlock::HardlockDeviceActivity& activity() const { return activity_; }

private:
    GuestDeviceConfig config_;
    hardlock::HardlockDevice hardlock_device_;
    std::vector<std::uint32_t> open_handles_;
    GuestHandleAllocator own_handles_;
    GuestHandleAllocator* shared_handles_ = nullptr;
    hardlock::HardlockDeviceActivity activity_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_DEVICES_H_
