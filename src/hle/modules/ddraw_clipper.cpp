#include <cstdint>
#include <memory>
#include <string>

#include "ddraw_interfaces.h"
#include "re2dj/directx/abi.h"
#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_process.h"

namespace re2dj::hle::modules::ddraw
{
namespace
{

namespace dx = re2dj::directx;
using com::Fail;
using com::MethodProcess;
using com::Succeed;

// A clipper's state: the window whose client area it clips to. The host
// window is the guest's only window and nothing overlaps it, so the window is
// all a clipper needs to answer what the guest asks of it (#15).
struct ClipperState final : GuestComState
{
    std::uint32_t window = 0;
};

ClipperState* ClipperStateOf(GuestProcess& process, std::uint32_t clipper)
{
    const GuestComObject* object = process.com().Find(clipper);
    return object == nullptr || object->kind != kClipperObject ? nullptr : object->StateAs<ClipperState>();
}

// GetHWnd(this, lphWnd): the window SetHWnd gave, 0 before one.
bool GetHWnd(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kClipperObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    const ClipperState* state = ClipperStateOf(*process, call.arguments[0]);
    return com::WriteWord(call, call.arguments[1], state == nullptr ? 0 : state->window, error) &&
           Succeed(result, dx::kDdOk, error);
}

// SetHWnd(this, dwFlags, hWnd): the window to clip to. dwFlags is reserved
// and must be 0.
bool SetHWnd(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kClipperObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] != 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    ClipperState* state = ClipperStateOf(*process, call.arguments[0]);
    if (state != nullptr)
    {
        state->window = call.arguments[2];
    }
    return Succeed(result, dx::kDdOk, error);
}

// IDirectDrawClipper in vtable order (ddraw.h). A clip list is not modelled:
// a windowed title reaches the screen only through Blt onto the primary,
// which the presentation shows whole.
constexpr com::Method kMethods[] = {
    {"QueryInterface", 3, &UnimplementedExport},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"GetClipList", 4, &UnimplementedExport},
    {"GetHWnd", 2, &GetHWnd},
    {"Initialize", 3, &UnimplementedExport},
    {"IsClipListChanged", 2, &UnimplementedExport},
    {"SetClipList", 3, &UnimplementedExport},
    {"SetHWnd", 3, &SetHWnd},
};

}  // namespace

std::span<const com::Method> DirectDrawClipperMethods()
{
    return kMethods;
}

bool CreateClipperOf(const ImportCall& call, ImportReturn* result, std::uint32_t kind, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kind, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[3] != 0)
    {
        return Succeed(result, dx::kClassENoAggregation, error);
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, dx::kDdErrInvalidParams, error);
    }
    GuestComObject object;
    object.kind = kClipperObject;
    object.state = std::make_shared<ClipperState>();
    const std::uint32_t clipper =
        com::CreateObject(call, *process, kModule, kDirectDrawClipper, kMethods, std::move(object), error);
    if (clipper == 0)
    {
        return false;
    }
    return com::WriteWord(call, call.arguments[2], clipper, error) && Succeed(result, dx::kDdOk, error);
}

bool IsClipper(GuestProcess& process, std::uint32_t address)
{
    return ClipperStateOf(process, address) != nullptr;
}

}  // namespace re2dj::hle::modules::ddraw
