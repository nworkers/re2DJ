#include "re2dj/hle/guest_user.h"

#include <algorithm>
#include <array>
#include <utility>

#include "re2dj/storage/guest_path.h"

namespace re2dj::hle
{
namespace
{

// winuser.h IDI_* and IDC_* resource IDs.
constexpr std::array<std::uint32_t, 7> kSystemIconIds = {
    32512, 32513, 32514, 32515, 32516, 32517, 32518,
};
constexpr std::array<std::uint32_t, 16> kSystemCursorIds = {
    32512, 32513, 32514, 32515, 32516, 32640, 32641, 32642,
    32643, 32644, 32645, 32646, 32648, 32649, 32650, 32651,
};

template <std::size_t N>
bool Contains(const std::array<std::uint32_t, N>& ids, std::uint32_t id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool HasValue(const std::map<std::uint32_t, std::uint32_t>& handles, std::uint32_t handle)
{
    return handle != 0 && std::any_of(handles.begin(), handles.end(), [handle](const auto& entry) {
               return entry.second == handle;
           });
}

}  // namespace

std::uint32_t GuestUser::AllocateHandle()
{
    const std::uint32_t handle = next_handle_;
    next_handle_ += 4;
    return handle;
}

std::uint32_t GuestUser::SystemIcon(std::uint32_t id)
{
    if (!Contains(kSystemIconIds, id))
    {
        return 0;
    }
    const auto [entry, inserted] = system_icons_.try_emplace(id, 0);
    if (inserted)
    {
        entry->second = AllocateHandle();
    }
    return entry->second;
}

std::uint32_t GuestUser::SystemCursor(std::uint32_t id)
{
    if (!Contains(kSystemCursorIds, id))
    {
        return 0;
    }
    const auto [entry, inserted] = system_cursors_.try_emplace(id, 0);
    if (inserted)
    {
        entry->second = AllocateHandle();
    }
    return entry->second;
}

bool GuestUser::IsIcon(std::uint32_t handle) const
{
    return HasValue(system_icons_, handle);
}

bool GuestUser::IsCursor(std::uint32_t handle) const
{
    return HasValue(system_cursors_, handle);
}

std::uint16_t GuestUser::AddClass(GuestWindowClass window_class)
{
    if (FindClass(window_class.name, window_class.instance) != nullptr || next_atom_ == 0xFFFFU)
    {
        return 0;
    }
    window_class.atom = next_atom_++;
    const std::uint16_t atom = window_class.atom;
    classes_.emplace(atom, std::move(window_class));
    return atom;
}

const GuestWindowClass* GuestUser::FindClass(std::string_view name, std::uint32_t instance) const
{
    for (const auto& [atom, window_class] : classes_)
    {
        if (window_class.instance == instance && storage::EqualsIgnoreAsciiCase(window_class.name, name))
        {
            return &window_class;
        }
    }
    return nullptr;
}

const GuestWindowClass* GuestUser::FindClass(std::uint16_t atom) const
{
    const auto found = classes_.find(atom);
    return found == classes_.end() ? nullptr : &found->second;
}

std::uint32_t GuestUser::AddWindow(GuestWindow window)
{
    window.handle = AllocateHandle();
    // Each window's device context comes from the same space, so no handle
    // the guest sees names two objects.
    window.device_context = AllocateHandle();
    const std::uint32_t handle = window.handle;
    windows_.emplace(handle, std::move(window));
    return handle;
}

std::int32_t GuestUser::ShowCursor(bool show)
{
    cursor_count_ += show ? 1 : -1;
    return cursor_count_;
}

std::uint32_t GuestUser::PrimaryMonitor()
{
    if (primary_monitor_ == 0)
    {
        primary_monitor_ = AllocateHandle();
    }
    return primary_monitor_;
}

GuestWindow* GuestUser::LookupWindow(std::uint32_t handle)
{
    const auto found = windows_.find(handle);
    return found == windows_.end() ? nullptr : &found->second;
}

}  // namespace re2dj::hle
