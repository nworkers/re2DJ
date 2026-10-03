#include "re2dj/hle/guest_gdi.h"

#include <utility>

namespace re2dj::hle
{

std::uint32_t GuestGdi::AddDc(GuestDc dc)
{
    const std::uint32_t handle = next_handle_;
    next_handle_ += 4;
    dcs_[handle] = dc;
    return handle;
}

void GuestGdi::PutDc(std::uint32_t handle, GuestDc dc)
{
    dcs_[handle] = dc;
}

GuestDc* GuestGdi::FindDc(std::uint32_t handle)
{
    const auto found = dcs_.find(handle);
    return found == dcs_.end() ? nullptr : &found->second;
}

std::uint32_t GuestGdi::AddBitmap(GuestBitmap bitmap)
{
    const std::uint32_t handle = next_handle_;
    next_handle_ += 4;
    bitmaps_[handle] = std::move(bitmap);
    return handle;
}

GuestBitmap* GuestGdi::FindBitmap(std::uint32_t handle)
{
    const auto found = bitmaps_.find(handle);
    return found == bitmaps_.end() ? nullptr : &found->second;
}

std::uint32_t GuestGdi::DcSelecting(std::uint32_t bitmap) const
{
    for (const auto& [handle, dc] : dcs_)
    {
        if (dc.bitmap == bitmap)
        {
            return handle;
        }
    }
    return 0;
}

std::uint32_t GuestGdi::AddBrush(std::uint32_t color)
{
    const std::uint32_t handle = next_handle_;
    next_handle_ += 4;
    brushes_[handle] = color;
    return handle;
}

const std::uint32_t* GuestGdi::FindBrush(std::uint32_t handle) const
{
    const auto found = brushes_.find(handle);
    return found == brushes_.end() ? nullptr : &found->second;
}

bool GuestGdi::Delete(std::uint32_t handle)
{
    return dcs_.erase(handle) != 0 || bitmaps_.erase(handle) != 0 || brushes_.erase(handle) != 0;
}

}  // namespace re2dj::hle
