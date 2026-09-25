#include "re2dj/hle/hardlock/device_material.h"

#include <array>
#include <cstdint>
#include <string>

#include "re2dj/hle/guest_device_path.h"
#include "re2dj/hle/hardlock/device_call.h"
#include "re2dj/hle/hardlock/protocol.h"
#include "temporary_tree.h"
#include "test_support.h"

namespace
{

namespace hardlock = re2dj::hle::hardlock;

// Every value below is synthetic; real material never enters the repository.
constexpr char kProfile[] = "test-profile";
constexpr char kMapRow[] = "0011223344556677 8899aabbccddeeff\n";

hardlock::HardlockMaterialSources ProfileSources(const re2dj::test::TemporaryTree& tree)
{
    hardlock::HardlockMaterialSources sources;
    sources.profile_id = kProfile;
    sources.use_profile_cfg = true;
    sources.config_path = tree.root() / "hardlock.ini";
    sources.default_map_path = tree.root() / "hardlock-test-profile.map";
    return sources;
}

void CheckMaterialAbsence(re2dj::test::Context& context)
{
    // No cfg file and no map: a valid run with nothing applied.
    re2dj::test::TemporaryTree tree;
    hardlock::HardlockDeviceMaterial material;
    std::string error = "stale";
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(ProfileSources(tree), &material, &error));
    RE2DJ_CHECK(context, error.empty());
    RE2DJ_CHECK(context, !material.device_enabled);
    RE2DJ_CHECK(context, !material.handshake_response.has_value());
    RE2DJ_CHECK(context, !material.seeds.has_value());
    RE2DJ_CHECK(context, material.transform_map.empty());

    // A section with replay values but neither seeds nor a map applies nothing,
    // because the replay values only accompany material that enables the device.
    tree.WriteText("hardlock.ini",
                   "[test-profile]\nresponse450=0100fafa0010\ntail44c=0001\n");
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(ProfileSources(tree), &material, &error));
    RE2DJ_CHECK(context, !material.device_enabled);
    RE2DJ_CHECK(context, !material.handshake_response.has_value());
    RE2DJ_CHECK(context, !material.cfg_handshake);

    // A profile that does not allow cfg ignores the file entirely.
    tree.WriteText("hardlock.ini",
                   "[test-profile]\nmodule_address=0x1234\nseed1=1\nseed2=2\nseed3=3\n");
    auto disallowed = ProfileSources(tree);
    disallowed.use_profile_cfg = false;
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(disallowed, &material, &error));
    RE2DJ_CHECK(context, !material.device_enabled);
    RE2DJ_CHECK(context, !material.seeds.has_value());
}

void CheckMaterialFromCfg(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    tree.WriteText("hardlock.ini",
                   "[test-profile]\n"
                   "response450=0100fafa0010\n"
                   "tail44c=0001\n"
                   "module_address=0x1234\n"
                   "seed1=0x0011\n"
                   "seed2=34\n"
                   "seed3=0x0033\n");
    hardlock::HardlockDeviceMaterial material;
    std::string error;
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(ProfileSources(tree), &material, &error));
    RE2DJ_CHECK(context, material.device_enabled);
    RE2DJ_CHECK(context, material.seeds.has_value());
    if (material.seeds.has_value())
    {
        RE2DJ_CHECK_EQ(context, material.seeds->module_address, std::uint16_t{0x1234});
        RE2DJ_CHECK_EQ(context, material.seeds->seed1, std::uint16_t{0x0011});
        RE2DJ_CHECK_EQ(context, material.seeds->seed2, std::uint16_t{34});
        RE2DJ_CHECK_EQ(context, material.seeds->seed3, std::uint16_t{0x0033});
    }
    RE2DJ_CHECK(context, material.cfg_handshake && material.cfg_tail && !material.cfg_map);
    RE2DJ_CHECK(context, material.handshake_response.has_value());
    if (material.handshake_response.has_value())
    {
        const hardlock::HardlockHandshakeResponse expected = {0x01, 0x00, 0xfa, 0xfa, 0x00, 0x10};
        RE2DJ_CHECK(context, *material.handshake_response == expected);
    }
    RE2DJ_CHECK(context, material.descriptor_tail_word == std::uint16_t{0x0001});

    // Explicit options outrank cfg, and are not reported as cfg-provided.
    auto explicit_sources = ProfileSources(tree);
    explicit_sources.handshake_response_hex = "020000000000";
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(explicit_sources, &material, &error));
    RE2DJ_CHECK(context, !material.cfg_handshake);
    RE2DJ_CHECK(context, material.handshake_response.has_value() &&
                             (*material.handshake_response)[0] == 0x02);

    // A default map file enables the device and is read; the options carry it.
    tree.WriteText("hardlock-test-profile.map", std::string("# synthetic\n") + kMapRow);
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(ProfileSources(tree), &material, &error));
    RE2DJ_CHECK(context, material.cfg_map);
    RE2DJ_CHECK_EQ(context, material.transform_map.blocks.size(), std::size_t{1});
    const hardlock::HardlockDeviceOptions options = hardlock::MakeHardlockDeviceOptions(material);
    RE2DJ_CHECK_EQ(context, options.transform_responses.size(), std::size_t{1});
    RE2DJ_CHECK(context, options.seeds.has_value());
    RE2DJ_CHECK(context, options.handshake_response.has_value());

    // Unparseable seeds are left unset rather than failing the run.
    tree.WriteText("hardlock.ini",
                   "[test-profile]\nmodule_address=bogus\nseed1=1\nseed2=2\nseed3=3\n");
    RE2DJ_CHECK(context,
                hardlock::ResolveHardlockDeviceMaterial(ProfileSources(tree), &material, &error));
    RE2DJ_CHECK(context, !material.seeds.has_value());
}

void CheckMaterialErrors(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    hardlock::HardlockDeviceMaterial material;
    std::string error;

    auto bad_hex = ProfileSources(tree);
    bad_hex.handshake_response_hex = "zz";
    RE2DJ_CHECK(context, !hardlock::ResolveHardlockDeviceMaterial(bad_hex, &material, &error));
    RE2DJ_CHECK(context, !error.empty());

    auto missing_map = ProfileSources(tree);
    missing_map.transform_map_path = (tree.root() / "absent.map").string();
    RE2DJ_CHECK(context,
                !hardlock::ResolveHardlockDeviceMaterial(missing_map, &material, &error));
    RE2DJ_CHECK(context, !error.empty());
}

void CheckDeviceCall(re2dj::test::Context& context)
{
    hardlock::HardlockDevice device;
    hardlock::HardlockDeviceActivity activity;

    // Outside the four-IOCTL contract: not handled, no activity recorded.
    std::array<std::uint8_t, 4> small = {};
    hardlock::HardlockDeviceCall call =
        hardlock::CompleteHardlockDeviceIoControl(device, 0x00220000U, small, small);
    RE2DJ_CHECK(context, !call.handled);

    // The zero-sized initialize completes with no bytes.
    call = hardlock::CompleteHardlockDeviceIoControl(
        device, hardlock::kHardlockIoctlInitialize, {}, {});
    RE2DJ_CHECK(context, call.handled && call.succeeded);
    RE2DJ_CHECK_EQ(context, call.bytes_returned, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, call.win32_error, re2dj::hle::kWin32ErrorSuccess);
    hardlock::RecordHardlockDeviceCall(call.result, 100, &activity);

    // A handshake with the wrong buffer size fails with ERROR_INVALID_DATA.
    call = hardlock::CompleteHardlockDeviceIoControl(
        device, hardlock::kHardlockIoctlHandshake, small, small);
    RE2DJ_CHECK(context, call.handled && !call.succeeded);
    RE2DJ_CHECK_EQ(context, call.bytes_returned, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, call.win32_error, re2dj::hle::kWin32ErrorInvalidData);
    hardlock::RecordHardlockDeviceCall(call.result, 200, &activity);

    RE2DJ_CHECK_EQ(context, activity.total, 2U);
    RE2DJ_CHECK_EQ(context, activity.initialize, 1U);
    RE2DJ_CHECK_EQ(context, activity.handshake, 1U);
    RE2DJ_CHECK_EQ(context, activity.rejected, 1U);
    RE2DJ_CHECK_EQ(context, activity.last_tick, std::uint64_t{200});

    // The trace line keeps the Windows runtime's existing format.
    const std::string line = hardlock::FormatHardlockDeviceTrace(call.result, 200);
    RE2DJ_CHECK(context,
                line.rfind("re2dj:vfs:hardlock-device:request=handshake:outcome=", 0) == 0);
    RE2DJ_CHECK(context, line.size() >= 2 && line.substr(line.size() - 2) == "\r\n");
    RE2DJ_CHECK(context, line.find(":tick_ms=200\r\n") != std::string::npos);
}

void CheckGuestDevicePath(re2dj::test::Context& context)
{
    using re2dj::hle::MatchesGuestDevicePrefix;
    RE2DJ_CHECK(context, MatchesGuestDevicePrefix("\\\\.\\FEnteDev", "\\\\.\\FEnteDev"));
    RE2DJ_CHECK(context, MatchesGuestDevicePrefix("\\\\.\\fentedev", "\\\\.\\FEnteDev"));
    // A varying tail, such as an LPTDI port digit, still matches.
    RE2DJ_CHECK(context, MatchesGuestDevicePrefix("\\\\.\\LPTDI1", "\\\\.\\lptdi"));
    RE2DJ_CHECK(context, !MatchesGuestDevicePrefix("\\\\.\\NTICE", "\\\\.\\FEnteDev"));
    RE2DJ_CHECK(context, !MatchesGuestDevicePrefix("\\\\.\\FEnte", "\\\\.\\FEnteDev"));
    // An empty prefix matches nothing; a host supplies its own fallback.
    RE2DJ_CHECK(context, !MatchesGuestDevicePrefix("\\\\.\\FEnteDev", ""));
}

}  // namespace

void RunHardlockDeviceMaterialTests(re2dj::test::Context& context)
{
    CheckMaterialAbsence(context);
    CheckMaterialFromCfg(context);
    CheckMaterialErrors(context);
    CheckDeviceCall(context);
    CheckGuestDevicePath(context);
}
