#include "re2dj/hle/modules/dsound_module.h"

#include <cstdint>
#include <initializer_list>

#include "memory_services.h"
#include "re2dj/audio/directsound_abi.h"
#include "re2dj/hle/guest_process.h"
#include "test_support.h"

namespace
{

using re2dj::test::CallModuleExport;
using re2dj::test::MemoryServices;
namespace modules = re2dj::hle::modules;
namespace ds = re2dj::audio;

// DirectSoundCreate gives a device that makes a primary and secondary
// buffers with samples in guest memory, duplicates a secondary sharing its
// samples, locks and unlocks by the core's regions, and returns the memory
// once the last buffer sharing it goes.
void CheckDevice(re2dj::test::Context& context)
{
    const auto descriptor = modules::MakeDsoundModuleDescriptor();
    RE2DJ_CHECK(context, descriptor.exports[0].ordinal == std::uint16_t{1});
    // DirectSoundCreate, IDirectSound's 11 methods, IDirectSoundBuffer's 21.
    RE2DJ_CHECK_EQ(context, descriptor.exports.size(), std::size_t{33});
    MemoryServices services;
    // The vtables point at the module's exports.
    services.extra_module = "dsound.dll";
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
    constexpr std::uint32_t kDesc = MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kFormat = MemoryServices::kBase + 0x140;
    constexpr std::uint32_t kLock = MemoryServices::kBase + 0x180;

    RE2DJ_CHECK_EQ(context, call("DirectSoundCreate", {0, 0, 0}), ds::kDsErrInvalidParam);
    RE2DJ_CHECK_EQ(context, call("DirectSoundCreate", {0, kOut, 0x1234}), ds::kDsErrNoAggregation);
    RE2DJ_CHECK_EQ(context, call("DirectSoundCreate", {0, kOut, 0}), ds::kDsOk);
    const std::uint32_t device = services.U32(kOut);
    RE2DJ_CHECK(context, device != 0);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::SetCooperativeLevel", {device, 0x10014, 2}), ds::kDsOk);
    const std::size_t blocks_before = services.Process()->live_blocks();

    // The primary: whatever the guest asks, 4096 bytes of 48 kHz stereo.
    services.PutU32(kDesc, ds::kDsBufferDesc1Size);
    services.PutU32(kDesc + 4, ds::kDsbcapsPrimaryBuffer);
    services.PutU32(kDesc + 8, 0);
    services.PutU32(kDesc + 16, 0);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::CreateSoundBuffer", {device, kDesc, kOut, 0}), ds::kDsOk);
    const std::uint32_t primary = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::GetFormat", {primary, kFormat, 18, kLock}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, services.U32(kFormat + 4), 48000U);
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 18U);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::DuplicateSoundBuffer", {device, primary, kOut}),
                   ds::kDsErrInvalidCall);

    // A secondary of the guest's format, and a refusal of another format.
    services.PutU32(kFormat, 0x00020001U);
    services.PutU32(kFormat + 4, 22050);
    services.PutU32(kFormat + 8, 88200);
    services.PutU32(kFormat + 12, 0x00100004U);
    services.PutU32(kDesc + 4, 0x00010000U);
    services.PutU32(kDesc + 8, 1000);
    services.PutU32(kDesc + 16, kFormat);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::CreateSoundBuffer", {device, kDesc, kOut, 0}), ds::kDsOk);
    const std::uint32_t secondary = services.U32(kOut);
    services.PutU32(kDesc + 8, 0);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::CreateSoundBuffer", {device, kDesc, kOut, 0}), ds::kDsErrBadFormat);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);

    // Lock gives guest addresses; a wrapped lock starts again at the base.
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Lock",
                                 {secondary, 900, 200, kLock, kLock + 4, kLock + 8, kLock + 12, 0}),
                   ds::kDsOk);
    const std::uint32_t base = services.U32(kLock) - 900;
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 4), 100U);
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 8), base);
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 12), 100U);
    services.Byte(base) = 0x7F;
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Unlock", {secondary, base + 900, 100, base, 99}),
                   ds::kDsErrInvalidParam);
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Unlock", {secondary, base + 900, 100, base, 100}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Lock",
                                 {secondary, 0, 1001, kLock, kLock + 4, 0, 0, 0}),
                   ds::kDsErrInvalidParam);

    // A duplicate shares the samples.
    RE2DJ_CHECK_EQ(context, call("IDirectSound::DuplicateSoundBuffer", {device, secondary, kOut}), ds::kDsOk);
    const std::uint32_t duplicate = services.U32(kOut);
    RE2DJ_CHECK(context, duplicate != secondary);
    call("IDirectSoundBuffer::Lock", {duplicate, 0, 0, kLock, kLock + 4, 0, 0, ds::kDsbLockEntireBuffer});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), base);
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 4), 1000U);

    constexpr std::uint32_t kCaps = MemoryServices::kBase + 0x200;
    services.PutU32(kCaps, 20);
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::GetCaps", {duplicate, kCaps}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, services.U32(kCaps + 4), 0x00010000U);
    RE2DJ_CHECK_EQ(context, services.U32(kCaps + 8), 1000U);
    services.PutU32(kCaps, 96);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::GetCaps", {device, kCaps}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, services.U32(kCaps + 4), ds::kDscapsPrimaryStereo | ds::kDscaps16Bit);

    // The samples outlive the original while the duplicate holds them.
    call("IDirectSoundBuffer::Release", {secondary});
    RE2DJ_CHECK_EQ(context, services.Byte(base), 0x7F);
    call("IDirectSoundBuffer::Release", {duplicate});
    call("IDirectSoundBuffer::Release", {primary});
    // Only the IDirectSoundBuffer vtable, built once, stays.
    RE2DJ_CHECK_EQ(context, services.Process()->live_blocks(), blocks_before + 1);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::Release", {device}), 0U);
}

}  // namespace

void RunDsoundModuleTests(re2dj::test::Context& context)
{
    CheckDevice(context);
}
