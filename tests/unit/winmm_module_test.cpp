#include "re2dj/hle/modules/winmm_module.h"

#include <cstdint>
#include <initializer_list>
#include <string>

#include "memory_services.h"
#include "re2dj/hle/guest_mixer.h"
#include "test_support.h"

namespace
{

using re2dj::test::CallModuleExport;
using re2dj::test::MemoryServices;
namespace hle = re2dj::hle;

constexpr std::uint32_t kHandle = 0x80000000U;

// The one mixer answers as a Windows 11 host's first mixer does, from opening
// it through its lines, controls, and values to closing it.
void CheckMixer(re2dj::test::Context& context)
{
    const auto descriptor = re2dj::hle::modules::MakeWinmmModuleDescriptor();
    MemoryServices services;
    const auto call = [&](const char* name, std::initializer_list<std::uint32_t> arguments) {
        return CallModuleExport(context, services, descriptor, name, arguments).eax;
    };
    constexpr std::uint32_t kOut = MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kLine = MemoryServices::kArenaBase;
    constexpr std::uint32_t kRequest = MemoryServices::kArenaBase + 0x200;
    constexpr std::uint32_t kControls = MemoryServices::kArenaBase + 0x300;
    constexpr std::uint32_t kValues = MemoryServices::kArenaBase + 0x500;

    RE2DJ_CHECK_EQ(context, call("mixerGetNumDevs", {}), 1U);
    RE2DJ_CHECK_EQ(context, call("mixerOpen", {kOut, 1, 0, 0, 0}), hle::kMmsyserrBadDeviceId);
    RE2DJ_CHECK_EQ(context, call("mixerOpen", {kOut, 0, 0, 0, 0}), hle::kMmsyserrNoError);
    const std::uint32_t mixer = services.U32(kOut);
    RE2DJ_CHECK(context, mixer != 0);

    // Lines: the speakers destination, its wave source, and refusals.
    services.PutU32(kLine, 168);
    services.PutU32(kLine + 4, 0);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer, kLine, kHandle}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 8), 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 12), 0xFFFF0000U);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 24), 4U);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 32), 2U);
    services.PutU32(kLine + 8, 1);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer, kLine, kHandle | 1}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 12), 0x00010000U);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 16), 0x80000001U);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 120), 1U);
    std::string name;
    std::string read_error;
    services.ReadGuestString(re2dj::runtime::GuestAddress(kLine + 136), &name, &read_error);
    RE2DJ_CHECK_EQ(context, name, std::string(hle::kMixerName));
    // Mixer ID 0 names the same mixer without a handle.
    services.PutU32(kLine + 24, 0x1005);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {0, kLine, 3}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kLine + 12), 0U);
    // Under MIXER_OBJECTF_MIXER an open handle names the mixer too, as the 4th
    // passes it; any other value is not a mixer.
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer, kLine, 3}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer + 4, kLine, 3}), hle::kMmsyserrBadDeviceId);
    services.PutU32(kLine + 12, 0x12345678U);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer, kLine, kHandle | 2}), hle::kMixerrInvalLine);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer, kLine, kHandle | 0xF}), hle::kMmsyserrInvalFlag);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer + 4, kLine, kHandle}), hle::kMmsyserrInvalHandle);
    services.PutU32(kLine, 100);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineInfoA", {mixer, kLine, kHandle}), hle::kMmsyserrInvalParam);

    // Controls: all of the destination's, one by type, one by ID.
    const auto put_request = [&](std::uint32_t line_id, std::uint32_t control, std::uint32_t count) {
        services.PutU32(kRequest, 24);
        services.PutU32(kRequest + 4, line_id);
        services.PutU32(kRequest + 8, control);
        services.PutU32(kRequest + 12, count);
        services.PutU32(kRequest + 16, 148);
        services.PutU32(kRequest + 20, kControls);
    };
    put_request(0xFFFF0000U, 0, 2);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineControlsA", {mixer, kRequest, kHandle}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kControls + 4), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kControls + 148 + 4), 2U);
    RE2DJ_CHECK_EQ(context, services.U32(kControls + 148 + 104), 65535U);
    RE2DJ_CHECK_EQ(context, services.U32(kControls + 148 + 124), 192U);
    put_request(0x00010000U, hle::kMixerControlTypeVolume, 1);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineControlsA", {mixer, kRequest, kHandle | 2}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kControls + 4), 6U);
    put_request(0x00010000U, 0x50030002U, 1);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineControlsA", {mixer, kRequest, kHandle | 2}), hle::kMixerrInvalControl);
    put_request(0, 1, 1);
    RE2DJ_CHECK_EQ(context, call("mixerGetLineControlsA", {mixer, kRequest, kHandle | 1}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kRequest + 4), 0xFFFF0000U);

    // Values: volumes start at full scale; an out-of-range value is ignored.
    const auto put_details = [&](std::uint32_t control, std::uint32_t channels) {
        services.PutU32(kRequest, 24);
        services.PutU32(kRequest + 4, control);
        services.PutU32(kRequest + 8, channels);
        services.PutU32(kRequest + 12, 0);
        services.PutU32(kRequest + 16, 4);
        services.PutU32(kRequest + 20, kValues);
    };
    put_details(2, 2);
    RE2DJ_CHECK_EQ(context, call("mixerGetControlDetailsA", {mixer, kRequest, kHandle}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kValues), 65535U);
    RE2DJ_CHECK_EQ(context, services.U32(kValues + 4), 65535U);
    put_details(1, 2);
    RE2DJ_CHECK_EQ(context, call("mixerGetControlDetailsA", {mixer, kRequest, kHandle}), hle::kMmsyserrInvalParam);
    put_details(9, 1);
    RE2DJ_CHECK_EQ(context, call("mixerGetControlDetailsA", {mixer, kRequest, kHandle}), hle::kMixerrInvalControl);
    put_details(2, 1);
    services.PutU32(kValues, 12345);
    RE2DJ_CHECK_EQ(context, call("mixerSetControlDetails", {mixer, kRequest, kHandle}), hle::kMmsyserrNoError);
    services.PutU32(kValues, 70000);
    RE2DJ_CHECK_EQ(context, call("mixerSetControlDetails", {mixer, kRequest, kHandle}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, call("mixerGetControlDetailsA", {mixer, kRequest, kHandle}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, services.U32(kValues), 12345U);
    services.PutU32(kRequest, 20);
    RE2DJ_CHECK_EQ(context, call("mixerGetControlDetailsA", {mixer, kRequest, kHandle}), hle::kMmsyserrInvalParam);

    RE2DJ_CHECK_EQ(context, call("mixerClose", {mixer}), hle::kMmsyserrNoError);
    RE2DJ_CHECK_EQ(context, call("mixerClose", {mixer}), hle::kMmsyserrInvalHandle);
}

}  // namespace

void RunWinmmModuleTests(re2dj::test::Context& context)
{
    CheckMixer(context);
}
