// dsound.dll's entry point, IDirectSound, and IDirectSoundBuffer, following
// the shared DirectSound core. Samples live in guest memory. With a host
// audio output, each buffer also keeps a host copy of its samples and
// controls that a host voice plays, driven as the Windows facade drives its
// backend; without one, buffers play silently by the clock.

#include "re2dj/hle/modules/dsound_module.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "facade_com.h"
#include "re2dj/audio/directsound_abi.h"
#include "re2dj/audio/directsound_buffer_policy.h"
#include "re2dj/audio/directsound_device.h"
#include "re2dj/audio/legacy_audio_buffer.h"
#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/host_audio.h"

namespace re2dj::hle::modules
{
namespace
{

namespace ds = re2dj::audio;
using com::CallName;
using com::Fail;
using com::MethodProcess;
using com::Succeed;

constexpr std::string_view kModule = "dsound.dll";
constexpr std::string_view kDirectSound = "IDirectSound";
constexpr std::string_view kDirectSoundBuffer = "IDirectSoundBuffer";

// GuestComObject::kind of the dsound facade objects, apart from ddraw's.
constexpr std::uint32_t kDirectSoundObject = 0x10;
constexpr std::uint32_t kSoundBufferObject = 0x11;

// A buffer's samples in the guest's VirtualAlloc arena, shared by the buffer
// and its duplicates.
struct SampleMemory
{
    std::uint32_t address = 0;
    std::uint32_t bytes = 0;
};

// A buffer: its shape, its samples, the lock the guest holds, and its
// controls. With no host audio, a playing buffer's cursor follows the host
// clock from play_start_ms (the core's silent playback); with one, the host
// voice and its copy of the buffer (host) decide the cursor and state.
struct SoundBufferState final : GuestComState
{
    bool primary = false;
    std::uint32_t flags = 0;
    ds::WaveFormatEx format;
    std::shared_ptr<SampleMemory> samples;
    bool locked = false;
    ds::LockRegions lock;
    std::uint32_t position = 0;
    std::int32_t volume = 0;
    std::int32_t pan = 0;
    std::uint32_t frequency = 0;
    bool playing = false;
    bool looping = false;
    std::uint32_t play_start_ms = 0;
    // The host output, its voice for this buffer, and the host copy of the
    // samples and controls it plays; the host outlives the guest process.
    HostAudio* audio = nullptr;
    std::uint32_t voice = 0;
    std::optional<ds::LegacyAudioBuffer> host;
    bool duplicate = false;

    bool voiced() const { return voice != 0 && host.has_value(); }
    // Fed as the guest writes it rather than from what it holds, as the
    // Windows facade decides.
    bool streaming() const
    {
        return !primary && !duplicate && ds::IsStreamingBufferDescription(flags, samples->bytes);
    }

    void ReleaseResources(GuestProcess& process) override
    {
        if (audio != nullptr && voice != 0)
        {
            audio->DestroyVoice(voice);
            voice = 0;
        }
        // The last buffer sharing the samples returns them.
        if (samples != nullptr && samples.use_count() == 1 && samples->address != 0)
        {
            process.VirtualFree(samples->address, 0, kMemRelease);
        }
        samples.reset();
    }
};

SoundBufferState& BufferOf(GuestProcess& process, std::uint32_t buffer)
{
    return *process.com().Find(buffer)->StateAs<SoundBufferState>();
}

// Answers IUnknown and the object's own interface with the object itself;
// anything else is E_NOINTERFACE, as the Windows facade answers.
bool AnswerQueryInterface(const ImportCall& call,
                          ImportReturn* result,
                          std::uint32_t kind,
                          const std::array<std::uint8_t, 16>& own,
                          std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kind, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[2] == 0)
    {
        return Succeed(result, 0x80004003U, error);
    }
    re2dj::directx::Guid iid{};
    if (!com::ReadGuid(call, call.arguments[1], &iid, error))
    {
        return false;
    }
    const bool known = iid == re2dj::directx::kIidUnknown || iid == own;
    if (!com::WriteWord(call, call.arguments[2], known ? call.arguments[0] : 0U, error))
    {
        return false;
    }
    if (!known)
    {
        return Succeed(result, 0x80004002U, error);
    }
    process->com().AddRef(call.arguments[0]);
    return Succeed(result, ds::kDsOk, error);
}

// ---------------------------------------------------------------------------
// IDirectSoundBuffer
// ---------------------------------------------------------------------------

bool BufferQueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return AnswerQueryInterface(call, result, kSoundBufferObject, ds::kIidDirectSoundBuffer, error);
}

// GetCaps(this, lpDSBufferCaps): a DSBCAPS whose dwSize is at least its own.
bool BufferGetCaps(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    std::uint32_t size = 0;
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    if (!com::ReadStruct(call, call.arguments[1], &size, error))
    {
        return false;
    }
    if (size < sizeof(ds::DsbCaps))
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    const SoundBufferState& buffer = BufferOf(*process, call.arguments[0]);
    return com::WriteStruct(call, call.arguments[1], ds::BufferCaps(buffer.flags, buffer.samples->bytes), error) &&
           Succeed(result, ds::kDsOk, error);
}

// GetFormat(this, lpwfxFormat, dwSizeAllocated, lpdwSizeWritten): the
// WAVEFORMATEX, with its size written whenever asked; asking for the size
// alone (no structure, 0 bytes) succeeds.
bool BufferGetFormat(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[3] != 0 && !com::WriteWord(call, call.arguments[3], sizeof(ds::WaveFormatEx), error))
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, call.arguments[2] == 0 ? ds::kDsOk : ds::kDsErrInvalidParam, error);
    }
    if (call.arguments[2] < sizeof(ds::WaveFormatEx))
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    return com::WriteStruct(call, call.arguments[1], BufferOf(*process, call.arguments[0]).format, error) &&
           Succeed(result, ds::kDsOk, error);
}

// The host clock's milliseconds, for silent playback.
bool NowMs(const ImportCall& call, std::uint32_t* now, std::string* error)
{
    GuestClockReading reading;
    if (!call.services->ReadClock(&reading))
    {
        return Fail(error, CallName(call) + " needs the host clock");
    }
    *now = reading.tick_ms;
    return true;
}

// Brings a playing buffer's cursor up to now, stopping a one-shot buffer
// that has reached its end.
void CatchUp(SoundBufferState& buffer, std::uint32_t now)
{
    if (!buffer.playing)
    {
        return;
    }
    const ds::SilentPlayback playback =
        ds::AdvanceSilentPlayback(buffer.position, buffer.samples->bytes, buffer.frequency,
                                  buffer.format.block_align, buffer.looping, now - buffer.play_start_ms);
    buffer.position = playback.position;
    buffer.play_start_ms = now;
    if (playback.finished)
    {
        buffer.playing = false;
        buffer.looping = false;
    }
}

// The buffer a control method names, caught up to now; null with the call
// failed when the shape or clock is wrong.
SoundBufferState* ControlledBuffer(const ImportCall& call,
                                   ImportReturn* result,
                                   std::size_t argument_count,
                                   std::uint32_t* now,
                                   std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, argument_count, kSoundBufferObject, error);
    if (process == nullptr || !NowMs(call, now, error))
    {
        return nullptr;
    }
    SoundBufferState& buffer = BufferOf(*process, call.arguments[0]);
    CatchUp(buffer, *now);
    return &buffer;
}

// Play(this, dwReserved1, dwPriority, dwFlags): from the current position,
// looping with DSBPLAY_LOOPING.
bool BufferPlay(const ImportCall& call, ImportReturn* result, std::string* error)
{
    std::uint32_t now = 0;
    SoundBufferState* buffer = ControlledBuffer(call, result, 4, &now, error);
    if (buffer == nullptr)
    {
        return false;
    }
    buffer->playing = true;
    buffer->looping = (call.arguments[3] & ds::kDsbPlayLooping) != 0;
    buffer->play_start_ms = now;
    if (buffer->voiced())
    {
        buffer->host->set_playing(true, buffer->looping);
        const bool played = buffer->audio->Play(buffer->voice, *buffer->host, buffer->streaming());
        return Succeed(result, played ? ds::kDsOk : ds::kDsErrGeneric, error);
    }
    return Succeed(result, ds::kDsOk, error);
}

// Stop(this): the cursor stays where playback reached.
bool BufferStop(const ImportCall& call, ImportReturn* result, std::string* error)
{
    std::uint32_t now = 0;
    SoundBufferState* buffer = ControlledBuffer(call, result, 1, &now, error);
    if (buffer == nullptr)
    {
        return false;
    }
    buffer->playing = false;
    buffer->looping = false;
    if (buffer->voiced())
    {
        // The cursor stays where the voice had reached.
        buffer->host->set_current_position(buffer->audio->PositionBytes(buffer->voice, *buffer->host));
        buffer->host->set_playing(false, false);
        return Succeed(result, buffer->audio->Stop(buffer->voice) ? ds::kDsOk : ds::kDsErrGeneric, error);
    }
    return Succeed(result, ds::kDsOk, error);
}

// SetCurrentPosition(this, dwNewPosition): wrapped into the buffer; a
// playing buffer continues from there.
bool BufferSetCurrentPosition(const ImportCall& call, ImportReturn* result, std::string* error)
{
    std::uint32_t now = 0;
    SoundBufferState* buffer = ControlledBuffer(call, result, 2, &now, error);
    if (buffer == nullptr)
    {
        return false;
    }
    buffer->position = ds::WrapPosition(call.arguments[1], buffer->samples->bytes);
    if (buffer->voiced())
    {
        buffer->host->set_current_position(call.arguments[1]);
        return Succeed(result,
                       buffer->audio->SetPosition(buffer->voice, *buffer->host) ? ds::kDsOk : ds::kDsErrGeneric,
                       error);
    }
    return Succeed(result, ds::kDsOk, error);
}

// GetCurrentPosition(this, lpdwCurrentPlayCursor, lpdwCurrentWriteCursor):
// both cursors at the playback position, as the Windows facade reports them.
bool BufferGetCurrentPosition(const ImportCall& call, ImportReturn* result, std::string* error)
{
    std::uint32_t now = 0;
    SoundBufferState* buffer = ControlledBuffer(call, result, 3, &now, error);
    if (buffer == nullptr)
    {
        return false;
    }
    std::uint32_t play = buffer->position;
    std::uint32_t write = buffer->position;
    if (buffer->voiced())
    {
        // The write cursor is the play cursor, or the buffer's own position
        // when the guest does not ask for the play cursor.
        play = buffer->audio->PositionBytes(buffer->voice, *buffer->host);
        write = call.arguments[1] != 0 ? play : buffer->host->current_position();
    }
    if ((call.arguments[1] != 0 && !com::WriteWord(call, call.arguments[1], play, error)) ||
        (call.arguments[2] != 0 && !com::WriteWord(call, call.arguments[2], write, error)))
    {
        return false;
    }
    return Succeed(result, ds::kDsOk, error);
}

// GetStatus(this, lpdwStatus).
bool BufferGetStatus(const ImportCall& call, ImportReturn* result, std::string* error)
{
    std::uint32_t now = 0;
    SoundBufferState* buffer = ControlledBuffer(call, result, 2, &now, error);
    if (buffer == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    const std::uint32_t status = buffer->voiced()
                                     ? ds::BufferStatus(buffer->audio->IsPlaying(buffer->voice), buffer->host->looping())
                                     : ds::BufferStatus(buffer->playing, buffer->looping);
    return com::WriteWord(call, call.arguments[1], status, error) && Succeed(result, ds::kDsOk, error);
}

// Hands a voiced buffer's new volume, pan, or frequency to its voice.
bool UpdateVoice(SoundBufferState& buffer, ImportReturn* result, std::string* error)
{
    return Succeed(result, buffer.audio->UpdateControls(buffer.voice, *buffer.host) ? ds::kDsOk : ds::kDsErrGeneric,
                   error);
}

// SetVolume, SetPan, and SetFrequency: clamped or resolved by the core.
bool BufferSetVolume(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    SoundBufferState& buffer = BufferOf(*process, call.arguments[0]);
    buffer.volume = ds::ClampVolume(static_cast<std::int32_t>(call.arguments[1]));
    if (buffer.voiced())
    {
        buffer.host->set_volume(buffer.volume);
        return UpdateVoice(buffer, result, error);
    }
    return Succeed(result, ds::kDsOk, error);
}

bool BufferSetPan(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    SoundBufferState& buffer = BufferOf(*process, call.arguments[0]);
    buffer.pan = ds::ClampPan(static_cast<std::int32_t>(call.arguments[1]));
    if (buffer.voiced())
    {
        buffer.host->set_pan(buffer.pan);
        return UpdateVoice(buffer, result, error);
    }
    return Succeed(result, ds::kDsOk, error);
}

bool BufferSetFrequency(const ImportCall& call, ImportReturn* result, std::string* error)
{
    std::uint32_t now = 0;
    SoundBufferState* buffer = ControlledBuffer(call, result, 2, &now, error);
    if (buffer == nullptr)
    {
        return false;
    }
    buffer->frequency = ds::ResolveFrequency(call.arguments[1], buffer->format);
    if (buffer->voiced())
    {
        buffer->host->set_frequency(buffer->frequency);
        return UpdateVoice(*buffer, result, error);
    }
    return Succeed(result, ds::kDsOk, error);
}

// GetVolume, GetPan, and GetFrequency write the value into a pointer that
// must not be null.
bool WriteControl(const ImportCall& call,
                  ImportReturn* result,
                  std::uint32_t (*value)(const SoundBufferState&),
                  std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    return com::WriteWord(call, call.arguments[1], value(BufferOf(*process, call.arguments[0])), error) &&
           Succeed(result, ds::kDsOk, error);
}

bool BufferGetVolume(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return WriteControl(
        call, result, [](const SoundBufferState& buffer) { return static_cast<std::uint32_t>(buffer.volume); }, error);
}

bool BufferGetPan(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return WriteControl(
        call, result, [](const SoundBufferState& buffer) { return static_cast<std::uint32_t>(buffer.pan); }, error);
}

bool BufferGetFrequency(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return WriteControl(call, result, [](const SoundBufferState& buffer) { return buffer.frequency; }, error);
}

// SetFormat(this, lpcfxFormat): the buffer takes the format as given.
bool BufferSetFormat(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 2, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    return com::ReadStruct(call, call.arguments[1], &BufferOf(*process, call.arguments[0]).format, error) &&
           Succeed(result, ds::kDsOk, error);
}

bool BufferInitialize(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 3, kSoundBufferObject, error) != nullptr &&
           Succeed(result, ds::kDsErrAlreadyInitialized, error);
}

// Lock(this, dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2,
// pdwAudioBytes2, dwFlags): guest addresses into the buffer's samples,
// divided by the core.
bool BufferLock(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 8, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t first = call.arguments[3];
    const std::uint32_t first_bytes = call.arguments[4];
    const std::uint32_t second = call.arguments[5];
    const std::uint32_t second_bytes = call.arguments[6];
    if (first == 0 || first_bytes == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    SoundBufferState& buffer = BufferOf(*process, call.arguments[0]);
    ds::LockRegions regions;
    if (!ds::PlanLock(buffer.samples->bytes, call.arguments[1], call.arguments[2], call.arguments[7], &regions))
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    const std::uint32_t base = buffer.samples->address;
    if (!com::WriteWord(call, first, base + regions.first_offset, error) ||
        !com::WriteWord(call, first_bytes, regions.first_bytes, error) ||
        (second != 0 && !com::WriteWord(call, second, regions.second_bytes == 0 ? 0U : base, error)) ||
        (second_bytes != 0 && !com::WriteWord(call, second_bytes, regions.second_bytes, error)))
    {
        return false;
    }
    buffer.locked = true;
    buffer.lock = regions;
    return Succeed(result, ds::kDsOk, error);
}

// Unlock(this, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2): the
// regions the last Lock gave, whole.
bool BufferUnlock(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 5, kSoundBufferObject, error);
    if (process == nullptr)
    {
        return false;
    }
    SoundBufferState& buffer = BufferOf(*process, call.arguments[0]);
    const std::uint32_t base = buffer.samples->address;
    const bool matches = buffer.locked && call.arguments[1] == base + buffer.lock.first_offset &&
                         call.arguments[2] == buffer.lock.first_bytes &&
                         call.arguments[4] == buffer.lock.second_bytes &&
                         (buffer.lock.second_bytes == 0 || call.arguments[3] == base);
    if (!matches)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    const ds::LockRegions regions = buffer.lock;
    buffer.locked = false;
    buffer.lock = {};
    if (!buffer.voiced())
    {
        return Succeed(result, ds::kDsOk, error);
    }
    // What the guest wrote reaches the host copy, and a streaming buffer's
    // voice takes it, as the Windows facade commits at unlock.
    ds::LegacyAudioLock host_lock;
    if (!buffer.host->Lock(regions.first_offset, regions.first_bytes + regions.second_bytes, false, &host_lock) ||
        host_lock.first.size() != regions.first_bytes || host_lock.second.size() != regions.second_bytes)
    {
        return Fail(error, CallName(call) + " cannot reach the host copy of the unlocked samples");
    }
    const auto as_bytes = [](std::span<std::byte> span) {
        return std::span<std::uint8_t>(reinterpret_cast<std::uint8_t*>(span.data()), span.size());
    };
    if (!com::ReadBytes(call, base + regions.first_offset, as_bytes(host_lock.first), error) ||
        (regions.second_bytes != 0 && !com::ReadBytes(call, base, as_bytes(host_lock.second), error)))
    {
        return false;
    }
    if (buffer.streaming())
    {
        const bool committed = buffer.audio->CommitStreamingWrite(buffer.voice, *buffer.host);
        return Succeed(result, committed ? ds::kDsOk : ds::kDsErrGeneric, error);
    }
    return Succeed(result, ds::kDsOk, error);
}

bool BufferRestore(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 1, kSoundBufferObject, error) != nullptr && Succeed(result, ds::kDsOk, error);
}

// IDirectSoundBuffer in vtable order (dsound.h).
constexpr com::Method kBufferMethods[] = {
    {"QueryInterface", 3, &BufferQueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"GetCaps", 2, &BufferGetCaps},
    {"GetCurrentPosition", 3, &BufferGetCurrentPosition},
    {"GetFormat", 4, &BufferGetFormat},
    {"GetVolume", 2, &BufferGetVolume},
    {"GetPan", 2, &BufferGetPan},
    {"GetFrequency", 2, &BufferGetFrequency},
    {"GetStatus", 2, &BufferGetStatus},
    {"Initialize", 3, &BufferInitialize},
    {"Lock", 8, &BufferLock},
    {"Play", 4, &BufferPlay},
    {"SetCurrentPosition", 2, &BufferSetCurrentPosition},
    {"SetFormat", 2, &BufferSetFormat},
    {"SetVolume", 2, &BufferSetVolume},
    {"SetPan", 2, &BufferSetPan},
    {"SetFrequency", 2, &BufferSetFrequency},
    {"Stop", 1, &BufferStop},
    {"Unlock", 5, &BufferUnlock},
    {"Restore", 1, &BufferRestore},
};

// A buffer object of the device, sharing samples when given them (a
// duplicate) and otherwise with zeroed samples of its own.
std::uint32_t MakeBuffer(const ImportCall& call,
                         GuestProcess& process,
                         std::uint32_t direct_sound,
                         const ds::SoundBufferPlan& plan,
                         std::shared_ptr<SampleMemory> shared,
                         std::string* error)
{
    auto state = std::make_shared<SoundBufferState>();
    state->primary = plan.primary;
    state->flags = plan.flags;
    state->format = plan.format;
    state->samples = std::move(shared);
    state->frequency = plan.format.samples_per_second;
    if (state->samples == nullptr)
    {
        auto samples = std::make_shared<SampleMemory>();
        samples->bytes = plan.bytes;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
        if (process.VirtualAlloc(0, plan.bytes, kMemCommit | kMemReserve, kPageReadWrite, &samples->address,
                                 &committed) != GuestMemoryResult::kOk)
        {
            Fail(error, CallName(call) + " has no guest memory for a " + std::to_string(plan.bytes) +
                            "-byte buffer");
            return 0;
        }
        for (const auto& [address, length] : committed)
        {
            const std::vector<std::uint8_t> zeros(length, 0);
            if (!com::WriteBytes(call, address, zeros, error))
            {
                process.VirtualFree(samples->address, 0, kMemRelease);
                return 0;
            }
        }
        state->samples = std::move(samples);
    }
    GuestComObject object;
    object.kind = kSoundBufferObject;
    object.parent = direct_sound;
    object.state = state;
    const std::uint32_t buffer =
        com::CreateObject(call, process, kModule, kDirectSoundBuffer, kBufferMethods, object, error);
    if (buffer == 0)
    {
        state->ReleaseResources(process);
        return 0;
    }
    process.com().AddRef(direct_sound);
    return buffer;
}

// ---------------------------------------------------------------------------
// IDirectSound
// ---------------------------------------------------------------------------

bool QueryInterface(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return AnswerQueryInterface(call, result, kDirectSoundObject, ds::kIidDirectSound, error);
}

// With a host audio output, gives a buffer its voice and host copy: fresh
// and silent, or for a duplicate the original's copy sharing its samples.
// False when the host has no voice to give; true with nothing done when
// there is no host output.
bool AttachVoice(const ImportCall& call, SoundBufferState& buffer, const ds::LegacyAudioBuffer* source)
{
    HostAudio* audio = call.services->Audio();
    if (audio == nullptr)
    {
        return true;
    }
    const std::uint32_t voice = audio->CreateVoice();
    if (voice == 0)
    {
        return false;
    }
    buffer.audio = audio;
    buffer.voice = voice;
    if (source != nullptr)
    {
        buffer.host = source->Duplicate();
    }
    else
    {
        buffer.host.emplace(ds::LegacyAudioFormat{buffer.format.channels, buffer.format.samples_per_second,
                                                  buffer.format.bits_per_sample, buffer.format.block_align},
                            buffer.samples->bytes);
    }
    return true;
}

// CreateSoundBuffer(this, lpcDSBufferDesc, lplpDirectSoundBuffer,
// pUnkOuter) under the core's plan.
bool CreateSoundBuffer(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 4, kDirectSoundObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t out = call.arguments[2];
    if (out == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    if (!com::WriteWord(call, out, 0, error))
    {
        return false;
    }
    if (call.arguments[3] != 0)
    {
        return Succeed(result, ds::kDsErrNoAggregation, error);
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    // A DSBUFFERDESC1 is all the guest need provide.
    ds::DsBufferDesc description;
    std::array<std::uint8_t, ds::kDsBufferDesc1Size> head{};
    if (!com::ReadBytes(call, call.arguments[1], head, error))
    {
        return false;
    }
    std::memcpy(static_cast<void*>(&description), head.data(), head.size());
    ds::WaveFormatEx format;
    const bool has_format = description.size >= ds::kDsBufferDesc1Size && description.format != 0;
    if (has_format && !com::ReadStruct(call, description.format, &format, error))
    {
        return false;
    }
    const ds::SoundBufferPlan plan = ds::PlanSoundBuffer(description, has_format ? &format : nullptr);
    if (plan.result != ds::kDsOk)
    {
        return Succeed(result, plan.result, error);
    }
    const std::uint32_t buffer = MakeBuffer(call, *process, call.arguments[0], plan, nullptr, error);
    if (buffer == 0)
    {
        return false;
    }
    if (!AttachVoice(call, BufferOf(*process, buffer), nullptr))
    {
        // The Windows facade's answer when its backend has no voice to give.
        process->com().Release(*process, buffer);
        return Succeed(result, ds::kDsErrNoDriver, error);
    }
    if (!com::WriteWord(call, out, buffer, error))
    {
        process->com().Release(*process, buffer);
        return false;
    }
    return Succeed(result, ds::kDsOk, error);
}

// GetCaps(this, lpDSCaps): a DSCAPS whose dwSize is at least its own.
bool GetCaps(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 2, kDirectSoundObject, error) == nullptr)
    {
        return false;
    }
    std::uint32_t size = 0;
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    if (!com::ReadStruct(call, call.arguments[1], &size, error))
    {
        return false;
    }
    if (size < sizeof(ds::DsCaps))
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    return com::WriteStruct(call, call.arguments[1], ds::DeviceCaps(), error) && Succeed(result, ds::kDsOk, error);
}

// DuplicateSoundBuffer(this, lpDsbOriginal, lplpDsbDuplicate): a new buffer
// sharing the original's samples and shape.
bool DuplicateSoundBuffer(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MethodProcess(call, result, 3, kDirectSoundObject, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t out = call.arguments[2];
    if (out == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    if (!com::WriteWord(call, out, 0, error))
    {
        return false;
    }
    const GuestComObject* original = call.arguments[1] == 0 ? nullptr : process->com().Find(call.arguments[1]);
    const SoundBufferState* source = original == nullptr ? nullptr : original->StateAs<SoundBufferState>();
    if (source == nullptr)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    const std::uint32_t checked = ds::CheckDuplicate(source->primary);
    if (checked != ds::kDsOk)
    {
        return Succeed(result, checked, error);
    }
    ds::SoundBufferPlan plan;
    plan.primary = false;
    plan.flags = source->flags;
    plan.bytes = source->samples->bytes;
    plan.format = source->format;
    const std::uint32_t duplicate = MakeBuffer(call, *process, call.arguments[0], plan, source->samples, error);
    if (duplicate == 0)
    {
        return false;
    }
    // A duplicate starts stopped, with the original's position, volume, pan,
    // and frequency, as the Windows facade's does.
    SoundBufferState& copy = BufferOf(*process, duplicate);
    copy.position = source->position;
    copy.volume = source->volume;
    copy.pan = source->pan;
    copy.frequency = source->frequency;
    copy.duplicate = true;
    if (!AttachVoice(call, copy, source->host.has_value() ? &*source->host : nullptr))
    {
        process->com().Release(*process, duplicate);
        return Succeed(result, ds::kDsErrNoDriver, error);
    }
    if (!com::WriteWord(call, out, duplicate, error))
    {
        process->com().Release(*process, duplicate);
        return false;
    }
    return Succeed(result, ds::kDsOk, error);
}

// SetCooperativeLevel, Compact, and SetSpeakerConfig change nothing a
// facade device keeps.
bool SetCooperativeLevel(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 3, kDirectSoundObject, error) != nullptr && Succeed(result, ds::kDsOk, error);
}

bool Compact(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 1, kDirectSoundObject, error) != nullptr && Succeed(result, ds::kDsOk, error);
}

bool SetSpeakerConfig(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 2, kDirectSoundObject, error) != nullptr && Succeed(result, ds::kDsOk, error);
}

bool GetSpeakerConfig(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (MethodProcess(call, result, 2, kDirectSoundObject, error) == nullptr)
    {
        return false;
    }
    if (call.arguments[1] == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    return com::WriteWord(call, call.arguments[1], ds::kDsSpeakerStereo, error) && Succeed(result, ds::kDsOk, error);
}

bool Initialize(const ImportCall& call, ImportReturn* result, std::string* error)
{
    return MethodProcess(call, result, 2, kDirectSoundObject, error) != nullptr &&
           Succeed(result, ds::kDsErrAlreadyInitialized, error);
}

// IDirectSound in vtable order (dsound.h).
constexpr com::Method kDirectSoundMethods[] = {
    {"QueryInterface", 3, &QueryInterface},
    {"AddRef", 1, &com::AddRef},
    {"Release", 1, &com::Release},
    {"CreateSoundBuffer", 4, &CreateSoundBuffer},
    {"GetCaps", 2, &GetCaps},
    {"DuplicateSoundBuffer", 3, &DuplicateSoundBuffer},
    {"SetCooperativeLevel", 3, &SetCooperativeLevel},
    {"Compact", 1, &Compact},
    {"GetSpeakerConfig", 2, &GetSpeakerConfig},
    {"SetSpeakerConfig", 2, &SetSpeakerConfig},
    {"Initialize", 2, &Initialize},
};

// DirectSoundCreate(lpGuid, ppDS, pUnkOuter): the facade device, whichever
// device the guest names, as the Windows facade answers.
bool DirectSoundCreate(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 3)
    {
        return Fail(error, result == nullptr ? "dsound result is null"
                                             : "dsound DirectSoundCreate argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return Fail(error, "dsound DirectSoundCreate needs the guest process");
    }
    const std::uint32_t out = call.arguments[1];
    if (out == 0)
    {
        return Succeed(result, ds::kDsErrInvalidParam, error);
    }
    if (!com::WriteWord(call, out, 0, error))
    {
        return false;
    }
    if (call.arguments[2] != 0)
    {
        return Succeed(result, ds::kDsErrNoAggregation, error);
    }
    GuestComObject object;
    object.kind = kDirectSoundObject;
    const std::uint32_t direct_sound =
        com::CreateObject(call, *process, kModule, kDirectSound, kDirectSoundMethods, object, error);
    if (direct_sound == 0)
    {
        return false;
    }
    if (!com::WriteWord(call, out, direct_sound, error))
    {
        process->com().Release(*process, direct_sound);
        return false;
    }
    return Succeed(result, ds::kDsOk, error);
}

}  // namespace

GuestModuleDescriptor MakeDsoundModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = std::string(kModule);
    descriptor.aliases = {"dsound"};
    GuestExportDescriptor create = com::MakeExport("DirectSoundCreate", 3, &DirectSoundCreate);
    // The 4th imports it by ordinal 1.
    create.ordinal = std::uint16_t{1};
    descriptor.exports.push_back(std::move(create));
    com::AddMethods(&descriptor, kDirectSound, kDirectSoundMethods);
    com::AddMethods(&descriptor, kDirectSoundBuffer, kBufferMethods);
    return descriptor;
}

}  // namespace re2dj::hle::modules
