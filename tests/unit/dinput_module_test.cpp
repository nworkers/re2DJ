#include "re2dj/hle/modules/dinput_module.h"

#include <cstdint>
#include <initializer_list>
#include <string>

#include "memory_services.h"
#include "re2dj/directx/directinput.h"
#include "re2dj/hle/guest_process.h"
#include "test_support.h"

namespace
{

using re2dj::test::CallModuleExport;
using re2dj::test::MemoryServices;
namespace modules = re2dj::hle::modules;
namespace dx = re2dj::directx;

// DirectInputCreateA gives an object that makes the system keyboard and
// mouse; they take the guest's format, cooperative level, and acquisition,
// and report nothing held. Another device stops, naming it.
void CheckDevices(re2dj::test::Context& context)
{
    const auto descriptor = modules::MakeDinputModuleDescriptor();
    // DirectInputCreateA, IDirectInputA's 8 methods, IDirectInputDeviceA's 18.
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{27});
    MemoryServices services;
    services.extra_module = "dinput.dll";
    std::uint32_t next = 0x6F001000U;
    for (const auto& export_descriptor : descriptor.exports)
    {
        services.extra_exports[export_descriptor.name] = next;
        next += 0x10;
    }
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kGuid = MemoryServices::kBase + 0x60;
    constexpr std::uint32_t kState = MemoryServices::kBase + 0x200;
    const auto put_guid = [&](const dx::Guid& guid) {
        for (std::uint32_t index = 0; index < 16; ++index)
        {
            services.Byte(kGuid + index) = guid[index];
        }
    };

    RE2DJ_CHECK_EQ(context, call("DirectInputCreateA", {0x00400000U, 0x700, 0, 0}), dx::kEPointer);
    RE2DJ_CHECK_EQ(context, call("DirectInputCreateA", {0x00400000U, 0x700, kOut, 0}), dx::kDiOk);
    const std::uint32_t direct_input = services.U32(kOut);
    RE2DJ_CHECK(context, direct_input != 0);

    put_guid(dx::kGuidSysKeyboard);
    RE2DJ_CHECK_EQ(context, call("IDirectInputA::CreateDevice", {direct_input, kGuid, kOut, 0}), dx::kDiOk);
    const std::uint32_t keyboard = services.U32(kOut);
    put_guid(dx::kGuidSysMouse);
    RE2DJ_CHECK_EQ(context, call("IDirectInputA::CreateDevice", {direct_input, kGuid, kOut, 0}), dx::kDiOk);
    const std::uint32_t mouse = services.U32(kOut);
    RE2DJ_CHECK(context, keyboard != 0 && mouse != 0 && keyboard != mouse);

    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::SetDataFormat", {keyboard, 0x00401000U}), dx::kDiOk);
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::SetCooperativeLevel",
                                 {keyboard, 0x10014, dx::kDisclNonExclusive | dx::kDisclForeground}),
                   dx::kDiOk);
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::Acquire", {keyboard}), dx::kDiOk);
    for (std::uint32_t index = 0; index < 256; ++index)
    {
        services.Byte(kState + index) = 0x55;
    }
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::GetDeviceState", {keyboard, 256, kState}), dx::kDiOk);
    RE2DJ_CHECK_EQ(context, services.U32(kState), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kState + 252), 0U);
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::GetDeviceState", {mouse, 16, 0}), dx::kEPointer);

    // What the host holds: keys by scan code, mouse buttons.
    re2dj::test::InputPresentation host;
    services.presentation = &host;
    host.input.scan_codes.set(0x2C);
    host.input.mouse_buttons[1] = true;
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::GetDeviceState", {keyboard, 256, kState}), dx::kDiOk);
    RE2DJ_CHECK_EQ(context, services.Byte(kState + 0x2C), std::uint8_t{0x80});
    RE2DJ_CHECK_EQ(context, services.Byte(kState + 0x2D), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::GetDeviceState", {mouse, 16, kState}), dx::kDiOk);
    RE2DJ_CHECK_EQ(context, services.Byte(kState + 12), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, services.Byte(kState + 13), std::uint8_t{0x80});
    services.presentation = nullptr;

    // The device answers its own interfaces only.
    put_guid(dx::kIidDirectInputDevice7A);
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::QueryInterface", {keyboard, kGuid, kOut}), dx::kDiOk);
    put_guid(dx::kIidDirectInputA);
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::QueryInterface", {keyboard, kGuid, kOut}),
                   dx::kENoInterface);

    // A device other than the system keyboard and mouse is not modelled.
    bool handled = true;
    std::string error;
    CallModuleExport(context, services, descriptor, "IDirectInputA::CreateDevice", {direct_input, kGuid, kOut, 0},
                     &handled, &error);
    RE2DJ_CHECK(context, !handled);
    RE2DJ_CHECK(context, error.find("has no model of device") != std::string::npos);

    // Devices hold the DirectInput object.
    call("IDirectInputDeviceA::Release", {keyboard});
    call("IDirectInputDeviceA::Release", {keyboard});
    RE2DJ_CHECK_EQ(context, call("IDirectInputA::Release", {direct_input}), 1U);
    RE2DJ_CHECK_EQ(context, call("IDirectInputDeviceA::Release", {mouse}), 0U);
    RE2DJ_CHECK(context, services.Process()->com().Find(direct_input) == nullptr);
}

}  // namespace

void RunDinputModuleTests(re2dj::test::Context& context)
{
    CheckDevices(context);
}
