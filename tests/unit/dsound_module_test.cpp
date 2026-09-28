#include "re2dj/hle/modules/dsound_module.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "memory_services.h"
#include "re2dj/audio/directsound_abi.h"
#include "re2dj/audio/directsound_buffer_policy.h"
#include "re2dj/audio/legacy_audio_buffer.h"
#include "re2dj/hle/host_audio.h"
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
    call("IDirectSoundBuffer::SetVolume", {secondary, static_cast<std::uint32_t>(-500)});
    RE2DJ_CHECK_EQ(context, call("IDirectSound::DuplicateSoundBuffer", {device, secondary, kOut}), ds::kDsOk);
    const std::uint32_t duplicate = services.U32(kOut);
    RE2DJ_CHECK(context, duplicate != secondary);
    call("IDirectSoundBuffer::GetVolume", {duplicate, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), static_cast<std::uint32_t>(-500));
    call("IDirectSoundBuffer::Lock", {duplicate, 0, 0, kLock, kLock + 4, 0, 0, ds::kDsbLockEntireBuffer});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), base);
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 4), 1000U);

    // Controls: silent playback follows the clock; a duplicate starts
    // stopped with the original's position, volume, pan, and frequency.
    re2dj::hle::GuestClockReading clock;
    clock.tick_ms = 1000;
    services.SetClock(clock);
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::SetVolume", {secondary, static_cast<std::uint32_t>(-20000)}),
                   ds::kDsOk);
    call("IDirectSoundBuffer::GetVolume", {secondary, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), static_cast<std::uint32_t>(-10000));
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::GetVolume", {secondary, 0}), ds::kDsErrInvalidParam);
    call("IDirectSoundBuffer::SetFrequency", {secondary, 0});
    call("IDirectSoundBuffer::GetFrequency", {secondary, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 22050U);
    call("IDirectSoundBuffer::SetCurrentPosition", {secondary, 1100});
    call("IDirectSoundBuffer::GetCurrentPosition", {secondary, kLock, kLock + 4});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 100U);
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Play", {secondary, 0, 0, ds::kDsbPlayLooping}), ds::kDsOk);
    call("IDirectSoundBuffer::GetStatus", {secondary, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), ds::kDsbStatusPlaying | ds::kDsbStatusLooping);
    // 22050 frames of 4 bytes a second: 10 ms moves 882 bytes, wrapping at 1000.
    clock.tick_ms = 1010;
    services.SetClock(clock);
    call("IDirectSoundBuffer::GetCurrentPosition", {secondary, kLock, 0});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), (100U + 882U) % 1000U);
    call("IDirectSoundBuffer::Stop", {secondary});
    call("IDirectSoundBuffer::GetStatus", {secondary, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 0U);
    call("IDirectSoundBuffer::Play", {secondary, 0, 0, 0});
    clock.tick_ms = 1100;
    services.SetClock(clock);
    call("IDirectSoundBuffer::GetStatus", {secondary, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 0U);

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

// A host output that records what the facade asks of it.
class FakeAudio final : public re2dj::hle::HostAudio
{
public:
    bool refuse = false;
    std::uint32_t position = 0;
    bool playing = false;
    std::vector<std::string> log;
    // The samples the last commit or play saw.
    std::vector<std::uint8_t> seen;

    std::uint32_t CreateVoice() override
    {
        if (refuse) return 0;
        log.push_back("create " + std::to_string(next_));
        return next_++;
    }
    void DestroyVoice(std::uint32_t voice) override { log.push_back("destroy " + std::to_string(voice)); }
    bool Play(std::uint32_t voice, const ds::LegacyAudioBuffer& buffer, bool streaming) override
    {
        Keep(buffer);
        playing = true;
        log.push_back("play " + std::to_string(voice) + (streaming ? " streaming" : " whole") +
                      (buffer.looping() ? " looping" : " once") + " at " + std::to_string(buffer.current_position()));
        return true;
    }
    bool CommitStreamingWrite(std::uint32_t voice, const ds::LegacyAudioBuffer& buffer) override
    {
        Keep(buffer);
        log.push_back("commit " + std::to_string(voice));
        return true;
    }
    bool Stop(std::uint32_t voice) override
    {
        playing = false;
        log.push_back("stop " + std::to_string(voice));
        return true;
    }
    bool SetPosition(std::uint32_t voice, const ds::LegacyAudioBuffer& buffer) override
    {
        log.push_back("position " + std::to_string(voice) + " " + std::to_string(buffer.current_position()));
        return true;
    }
    bool UpdateControls(std::uint32_t voice, const ds::LegacyAudioBuffer& buffer) override
    {
        log.push_back("controls " + std::to_string(voice) + " " + std::to_string(buffer.volume()) + " " +
                      std::to_string(buffer.pan()) + " " + std::to_string(buffer.frequency()));
        return true;
    }
    std::uint32_t PositionBytes(std::uint32_t, const ds::LegacyAudioBuffer&) const override { return position; }
    bool IsPlaying(std::uint32_t) const override { return playing; }

private:
    void Keep(const ds::LegacyAudioBuffer& buffer)
    {
        seen.clear();
        for (const std::byte value : buffer.samples())
        {
            seen.push_back(static_cast<std::uint8_t>(value));
        }
    }
    std::uint32_t next_ = 1;
};

// With a host output, each buffer has a voice fed from a host copy: unlocked
// samples reach it (committed for a streaming buffer), and play, stop,
// position, status, and controls go through the voice as the Windows facade
// drives its backend. A host with no voice to give fails creation with
// DSERR_NODRIVER.
void CheckHostAudio(re2dj::test::Context& context)
{
    const auto descriptor = modules::MakeDsoundModuleDescriptor();
    MemoryServices services;
    FakeAudio audio;
    services.audio = &audio;
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
    re2dj::hle::GuestClockReading clock;
    clock.tick_ms = 1000;
    services.SetClock(clock);
    call("DirectSoundCreate", {0, kOut, 0});
    const std::uint32_t device = services.U32(kOut);

    // A streaming secondary (GETCURRENTPOSITION2, not static) of 22050 Hz
    // 16-bit stereo.
    services.PutU32(kFormat, 0x00020001U);
    services.PutU32(kFormat + 4, 22050);
    services.PutU32(kFormat + 8, 88200);
    services.PutU32(kFormat + 12, 0x00100004U);
    services.PutU32(kDesc, ds::kDsBufferDesc1Size);
    services.PutU32(kDesc + 4, ds::kDsbcapsGetCurrentPosition2);
    services.PutU32(kDesc + 8, 1000);
    services.PutU32(kDesc + 12, 0);
    services.PutU32(kDesc + 16, kFormat);
    RE2DJ_CHECK_EQ(context, call("IDirectSound::CreateSoundBuffer", {device, kDesc, kOut, 0}), ds::kDsOk);
    const std::uint32_t streaming = services.U32(kOut);
    RE2DJ_CHECK(context, audio.log == std::vector<std::string>{"create 1"});

    // What the guest writes between Lock and Unlock reaches the voice.
    call("IDirectSoundBuffer::Lock", {streaming, 900, 200, kLock, kLock + 4, kLock + 8, kLock + 12, 0});
    const std::uint32_t base = services.U32(kLock) - 900;
    services.Byte(base + 900) = 0x11;
    services.Byte(base + 999) = 0x12;
    services.Byte(base) = 0x22;
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Unlock", {streaming, base + 900, 100, base, 100}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("commit 1"));
    RE2DJ_CHECK_EQ(context, audio.seen.size(), std::size_t{1000});
    if (audio.seen.size() == 1000)
    {
        RE2DJ_CHECK_EQ(context, audio.seen[900], std::uint8_t{0x11});
        RE2DJ_CHECK_EQ(context, audio.seen[999], std::uint8_t{0x12});
        RE2DJ_CHECK_EQ(context, audio.seen[0], std::uint8_t{0x22});
    }

    // Controls, position, play, status, and stop go through the voice.
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::SetVolume", {streaming, static_cast<std::uint32_t>(-20000)}),
                   ds::kDsOk);
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("controls 1 -10000 0 22050"));
    call("IDirectSoundBuffer::SetPan", {streaming, 500});
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("controls 1 -10000 500 22050"));
    call("IDirectSoundBuffer::SetFrequency", {streaming, 11025});
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("controls 1 -10000 500 11025"));
    call("IDirectSoundBuffer::SetCurrentPosition", {streaming, 1100});
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("position 1 100"));
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Play", {streaming, 0, 0, ds::kDsbPlayLooping}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("play 1 streaming looping at 100"));
    audio.position = 640;
    call("IDirectSoundBuffer::GetCurrentPosition", {streaming, kLock, kLock + 4});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 640U);
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 4), 640U);
    // Without the play cursor asked for, the write cursor is the buffer's own.
    call("IDirectSoundBuffer::GetCurrentPosition", {streaming, 0, kLock + 4});
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 4), 100U);
    call("IDirectSoundBuffer::GetStatus", {streaming, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), ds::kDsbStatusPlaying | ds::kDsbStatusLooping);
    audio.position = 300;
    RE2DJ_CHECK_EQ(context, call("IDirectSoundBuffer::Stop", {streaming}), ds::kDsOk);
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("stop 1"));
    call("IDirectSoundBuffer::GetCurrentPosition", {streaming, 0, kLock + 4});
    RE2DJ_CHECK_EQ(context, services.U32(kLock + 4), 300U);
    call("IDirectSoundBuffer::GetStatus", {streaming, kLock});
    RE2DJ_CHECK_EQ(context, services.U32(kLock), 0U);

    // A static buffer is played whole and not committed; its duplicate
    // shares its samples and is never streaming.
    services.PutU32(kDesc + 4, ds::kDsbcapsStatic | ds::kDsbcapsGetCurrentPosition2);
    call("IDirectSound::CreateSoundBuffer", {device, kDesc, kOut, 0});
    const std::uint32_t whole = services.U32(kOut);
    call("IDirectSoundBuffer::Lock", {whole, 0, 0, kLock, kLock + 4, 0, 0, ds::kDsbLockEntireBuffer});
    services.Byte(services.U32(kLock) + 5) = 0x55;
    audio.log.clear();
    call("IDirectSoundBuffer::Unlock", {whole, services.U32(kLock), 1000, 0, 0});
    RE2DJ_CHECK(context, audio.log.empty());
    RE2DJ_CHECK_EQ(context, call("IDirectSound::DuplicateSoundBuffer", {device, whole, kOut}), ds::kDsOk);
    const std::uint32_t duplicate = services.U32(kOut);
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("create 3"));
    call("IDirectSoundBuffer::Play", {duplicate, 0, 0, 0});
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("play 3 whole once at 0"));
    RE2DJ_CHECK(context, audio.seen.size() == 1000 && audio.seen[5] == 0x55);

    // Releasing a buffer lets its voice go.
    call("IDirectSoundBuffer::Release", {duplicate});
    RE2DJ_CHECK_EQ(context, audio.log.back(), std::string("destroy 3"));

    // A host with no voice to give fails creation as the Windows facade does.
    audio.refuse = true;
    RE2DJ_CHECK_EQ(context, call("IDirectSound::CreateSoundBuffer", {device, kDesc, kOut, 0}), ds::kDsErrNoDriver);
    RE2DJ_CHECK_EQ(context, services.U32(kOut), 0U);
    services.audio = nullptr;
}

}  // namespace

void RunDsoundModuleTests(re2dj::test::Context& context)
{
    CheckDevice(context);
    CheckHostAudio(context);
}
