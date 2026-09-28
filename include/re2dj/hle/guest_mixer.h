#ifndef RE2DJ_HLE_GUEST_MIXER_H_
#define RE2DJ_HLE_GUEST_MIXER_H_

#include <cstdint>
#include <map>
#include <span>
#include <string_view>
#include <vector>

#include "re2dj/hle/guest_handles.h"

// The one audio mixer the winmm facade models: its lines and controls as a
// Windows 11 host reports its first mixer, and the control values the guest
// sets. The host's device name is replaced with a fixed one, as the DirectDraw
// facade names its display adapter.
namespace re2dj::hle
{

// winmm results (mmsystem.h).
inline constexpr std::uint32_t kMmsyserrNoError = 0;
inline constexpr std::uint32_t kMmsyserrBadDeviceId = 2;
inline constexpr std::uint32_t kMmsyserrInvalHandle = 5;
inline constexpr std::uint32_t kMmsyserrInvalFlag = 10;
inline constexpr std::uint32_t kMmsyserrInvalParam = 11;
inline constexpr std::uint32_t kMixerrInvalLine = 1024;
inline constexpr std::uint32_t kMixerrInvalControl = 1025;

inline constexpr std::string_view kMixerName = "re2DJ Audio";
inline constexpr std::uint16_t kMixerManufacturer = 0xFFFF;
inline constexpr std::uint16_t kMixerProduct = 0xFFFF;
inline constexpr std::uint32_t kMixerDriverVersion = 0x0143;

// A MIXERLINE as measured, with the target device named kMixerName where the
// host named its own.
struct GuestMixerLine
{
    std::uint32_t destination = 0;
    std::uint32_t source = 0;
    std::uint32_t line_id = 0;
    std::uint32_t flags = 0;
    std::uint32_t component_type = 0;
    std::uint32_t channels = 0;
    std::uint32_t connections = 0;
    std::uint32_t controls = 0;
    std::string_view short_name;
    std::string_view name;
    std::uint32_t target_type = 0;
    std::uint16_t target_manufacturer = 0;
    std::uint16_t target_product = 0;
    std::uint32_t target_version = 0;
    std::string_view target_name;
};

// A MIXERCONTROL as measured, with the line it belongs to.
struct GuestMixerControl
{
    std::uint32_t id = 0;
    std::uint32_t line_id = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::string_view short_name;
    std::string_view name;
    std::uint32_t minimum = 0;
    std::uint32_t maximum = 0;
    std::uint32_t steps = 0;
    std::uint32_t initial = 0;
};

inline constexpr std::uint32_t kMixerControlTypeMute = 0x20010002U;
inline constexpr std::uint32_t kMixerControlTypeVolume = 0x50030001U;

// The destination (speakers) and its two sources (CD audio, wave out).
std::span<const GuestMixerLine> GuestMixerDestinations();
std::span<const GuestMixerLine> GuestMixerSources();
// Every control, grouped by line: each line's mute, then its volume.
std::span<const GuestMixerControl> GuestMixerControls();

const GuestMixerLine* FindGuestMixerLine(std::uint32_t line_id);
const GuestMixerLine* FindGuestMixerComponent(std::uint32_t component_type);
const GuestMixerControl* FindGuestMixerControl(std::uint32_t control_id);
const GuestMixerControl* FindGuestMixerControlByType(std::uint32_t line_id, std::uint32_t type);

// The process's open mixer handles and the control values the guest set.
class GuestMixer
{
public:
    std::uint32_t Open(GuestHandleAllocator& handles);
    // False for a handle that is not open.
    bool Close(std::uint32_t handle);
    bool IsOpen(std::uint32_t handle) const;

    std::uint32_t Value(const GuestMixerControl& control) const;
    // A value outside the control's bounds is ignored, as Windows 11 ignores
    // it while still answering MMSYSERR_NOERROR.
    void SetValue(const GuestMixerControl& control, std::uint32_t value);

private:
    std::vector<std::uint32_t> open_;
    std::map<std::uint32_t, std::uint32_t> values_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_MIXER_H_
