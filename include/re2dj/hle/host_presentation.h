#ifndef RE2DJ_HLE_HOST_PRESENTATION_H_
#define RE2DJ_HLE_HOST_PRESENTATION_H_

#include <cstdint>
#include <string>

namespace re2dj::hle
{

// What the host shows for the guest: a window on the host desktop standing for
// the guest's own. Platform-neutral; a host that presents implements it and
// hands it to the facades through ImportCallServices::Presentation().
class HostPresentation
{
public:
    virtual ~HostPresentation() = default;

    // Shows guest_window at width x height, creating the host window the
    // first time. False with error when the host cannot show it.
    virtual bool ShowGuestWindow(std::uint32_t guest_window,
                                 std::uint32_t width,
                                 std::uint32_t height,
                                 std::string* error) = 0;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_HOST_PRESENTATION_H_
