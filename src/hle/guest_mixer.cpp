#include "re2dj/hle/guest_mixer.h"

#include <algorithm>

namespace re2dj::hle
{
namespace
{

// MIXERLINE_LINEF_ACTIVE, MIXERLINE_LINEF_SOURCE.
constexpr std::uint32_t kLineActive = 0x00000001U;
constexpr std::uint32_t kLineSource = 0x80000000U;
// MIXERLINE_COMPONENTTYPE_* and MIXERLINE_TARGETTYPE_*.
constexpr std::uint32_t kDestinationSpeakers = 0x00000004U;
constexpr std::uint32_t kSourceCompactDisc = 0x00001005U;
constexpr std::uint32_t kSourceWaveOut = 0x00001008U;
constexpr std::uint32_t kTargetUndefined = 0;
constexpr std::uint32_t kTargetWaveOut = 1;
// MIXERCONTROL_CONTROLF_UNIFORM.
constexpr std::uint32_t kControlUniform = 0x00000001U;

// Measured on Windows 11 (the first mixer's lines). A destination's dwSource
// comes back as 0xFFFFFFFF.
constexpr GuestMixerLine kDestinations[] = {
    {0, 0xFFFFFFFFU, 0xFFFF0000U, kLineActive, kDestinationSpeakers, 1, 2, 2, "Volume", "Master Volume",
     kTargetUndefined, kMixerManufacturer, kMixerProduct, 0, ""},
};
constexpr GuestMixerLine kSources[] = {
    {0, 0, 0x00000000U, kLineSource | kLineActive, kSourceCompactDisc, 1, 0, 2, "CDAudio", "CD Audio",
     kTargetUndefined, kMixerManufacturer, kMixerProduct, kMixerDriverVersion, kMixerName},
    {0, 1, 0x00010000U, kLineSource | kLineActive, kSourceWaveOut, 1, 0, 2, "Volume", "Master Volume",
     kTargetWaveOut, kMixerManufacturer, kMixerProduct, kMixerDriverVersion, kMixerName},
};
constexpr GuestMixerControl kControls[] = {
    {1, 0xFFFF0000U, kMixerControlTypeMute, kControlUniform, "Mute", "Mute", 0, 1, 0, 0},
    {2, 0xFFFF0000U, kMixerControlTypeVolume, kControlUniform, "Volume", "Volume", 0, 65535, 192, 65535},
    {3, 0x00000000U, kMixerControlTypeMute, kControlUniform, "Mute", "Mute", 0, 1, 0, 0},
    {4, 0x00000000U, kMixerControlTypeVolume, kControlUniform, "Volume", "Volume", 0, 65535, 192, 65535},
    {5, 0x00010000U, kMixerControlTypeMute, kControlUniform, "Mute", "Mute", 0, 1, 0, 0},
    {6, 0x00010000U, kMixerControlTypeVolume, kControlUniform, "Volume", "Volume", 0, 65535, 192, 65535},
};

}  // namespace

std::span<const GuestMixerLine> GuestMixerDestinations()
{
    return kDestinations;
}

std::span<const GuestMixerLine> GuestMixerSources()
{
    return kSources;
}

std::span<const GuestMixerControl> GuestMixerControls()
{
    return kControls;
}

const GuestMixerLine* FindGuestMixerLine(std::uint32_t line_id)
{
    for (const GuestMixerLine& line : kDestinations)
    {
        if (line.line_id == line_id)
        {
            return &line;
        }
    }
    for (const GuestMixerLine& line : kSources)
    {
        if (line.line_id == line_id)
        {
            return &line;
        }
    }
    return nullptr;
}

const GuestMixerLine* FindGuestMixerComponent(std::uint32_t component_type)
{
    for (const GuestMixerLine& line : kDestinations)
    {
        if (line.component_type == component_type)
        {
            return &line;
        }
    }
    for (const GuestMixerLine& line : kSources)
    {
        if (line.component_type == component_type)
        {
            return &line;
        }
    }
    return nullptr;
}

const GuestMixerControl* FindGuestMixerControl(std::uint32_t control_id)
{
    for (const GuestMixerControl& control : kControls)
    {
        if (control.id == control_id)
        {
            return &control;
        }
    }
    return nullptr;
}

const GuestMixerControl* FindGuestMixerControlByType(std::uint32_t line_id, std::uint32_t type)
{
    for (const GuestMixerControl& control : kControls)
    {
        if (control.line_id == line_id && control.type == type)
        {
            return &control;
        }
    }
    return nullptr;
}

std::uint32_t GuestMixer::Open(GuestHandleAllocator& handles)
{
    const std::uint32_t handle = handles.Allocate();
    open_.push_back(handle);
    return handle;
}

bool GuestMixer::Close(std::uint32_t handle)
{
    const auto found = std::find(open_.begin(), open_.end(), handle);
    if (found == open_.end())
    {
        return false;
    }
    open_.erase(found);
    return true;
}

bool GuestMixer::IsOpen(std::uint32_t handle) const
{
    return std::find(open_.begin(), open_.end(), handle) != open_.end();
}

std::uint32_t GuestMixer::Value(const GuestMixerControl& control) const
{
    const auto found = values_.find(control.id);
    return found == values_.end() ? control.initial : found->second;
}

void GuestMixer::SetValue(const GuestMixerControl& control, std::uint32_t value)
{
    if (value >= control.minimum && value <= control.maximum)
    {
        values_[control.id] = value;
    }
}

}  // namespace re2dj::hle
