#include "directsound_com_facade.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../audio/sdl3_mixer_audio_backend.h"
#include "re2dj/audio/legacy_audio_buffer.h"
#include "audio_volume_trace.h"

extern "C" __declspec(dllexport) volatile float g_re2dj_audio_master_gain = 1.0f;

namespace
{
using re2dj::audio::LegacyAudioBuffer;
using re2dj::audio::LegacyAudioFormat;
using re2dj::audio::LegacyAudioLock;
using re2dj::audio::Sdl3MixerAudioBackend;

struct PcmLevels
{
    double peak = 0.0;
    double rms = 0.0;
    std::size_t sample_count = 0;
};

PcmLevels MeasurePcm(const LegacyAudioBuffer& buffer)
{
    PcmLevels result;
    const auto samples = buffer.samples();
    double square_sum = 0.0;
    if (buffer.format().bits_per_sample == 16)
    {
        result.sample_count = samples.size() / sizeof(std::int16_t);
        for (std::size_t index = 0; index < result.sample_count; ++index)
        {
            std::int16_t value = 0;
            std::memcpy(&value, samples.data() + index * sizeof(value), sizeof(value));
            const double normalized = static_cast<double>(value) / 32768.0;
            result.peak = (std::max)(result.peak, std::abs(normalized));
            square_sum += normalized * normalized;
        }
    }
    else if (buffer.format().bits_per_sample == 8)
    {
        result.sample_count = samples.size();
        for (const std::byte sample : samples)
        {
            const double normalized =
                (static_cast<double>(std::to_integer<unsigned char>(sample)) - 128.0) / 128.0;
            result.peak = (std::max)(result.peak, std::abs(normalized));
            square_sum += normalized * normalized;
        }
    }
    if (result.sample_count != 0)
    {
        result.rms = std::sqrt(square_sum / static_cast<double>(result.sample_count));
    }
    return result;
}

void TraceBuffer(const char* operation, DWORD flags, DWORD bytes)
{
    char message[160] = {};
    std::snprintf(message, sizeof(message),
                  "re2dj:audio:%s:flags=0x%08x:bytes=%u", operation,
                  static_cast<unsigned>(flags), static_cast<unsigned>(bytes));
    OutputDebugStringA(message);
}

class DirectSoundBufferFacade final : public IDirectSoundBuffer
{
public:
    DirectSoundBufferFacade(const DSBUFFERDESC& desc, const WAVEFORMATEX& wave)
        : flags_(desc.dwFlags), wave_(wave), buffer_({wave.nChannels, wave.nSamplesPerSec,
          wave.wBitsPerSample, wave.nBlockAlign}, desc.dwBufferBytes),
          voice_(Sdl3MixerAudioBackend::Instance().CreateVoice())
    {
        Re2djAudioTrace("directsound:create buffer=%p primary=%u flags=0x%08lx bytes=%lu channels=%u rate=%lu bits=%u align=%u",
                        this, is_primary() ? 1U : 0U, static_cast<unsigned long>(flags_),
                        static_cast<unsigned long>(desc.dwBufferBytes), wave_.nChannels,
                        static_cast<unsigned long>(wave_.nSamplesPerSec), wave_.wBitsPerSample,
                        wave_.nBlockAlign);
    }
    DirectSoundBufferFacade(const DirectSoundBufferFacade& source)
        : flags_(source.flags_), wave_(source.wave_), buffer_(source.buffer_.Duplicate()),
          voice_(Sdl3MixerAudioBackend::Instance().CreateVoice())
    {
        Re2djAudioTrace("directsound:duplicate source=%p buffer=%p bytes=%lu volume=%ld",
                        &source, this, static_cast<unsigned long>(buffer_.byte_count()),
                        static_cast<long>(buffer_.volume()));
    }
    ~DirectSoundBufferFacade() { Sdl3MixerAudioBackend::Instance().DestroyVoice(voice_); }
    bool ready() const { return voice_ != nullptr; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override { if (!object) return E_POINTER; *object = nullptr; if (iid == IID_IUnknown || iid == IID_IDirectSoundBuffer) { *object = this; AddRef(); return S_OK; } return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG value = --refs_; if (!value) delete this; return value; }
    HRESULT STDMETHODCALLTYPE GetCaps(DSBCAPS* caps) override { if (!caps || caps->dwSize < sizeof(DSBCAPS)) return DSERR_INVALIDPARAM; std::memset(caps, 0, sizeof(*caps)); caps->dwSize = sizeof(DSBCAPS); caps->dwFlags = flags_; caps->dwBufferBytes = static_cast<DWORD>(buffer_.byte_count()); return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(DWORD* play, DWORD* write) override { if (play) *play = Sdl3MixerAudioBackend::Instance().PositionBytes(voice_, buffer_); if (write) *write = play ? *play : buffer_.current_position(); return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetFormat(WAVEFORMATEX* format, DWORD size, DWORD* written) override { if (written) *written = sizeof(WAVEFORMATEX); if (!format) return size == 0 ? DS_OK : DSERR_INVALIDPARAM; if (size < sizeof(WAVEFORMATEX)) return DSERR_INVALIDPARAM; *format = wave_; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetVolume(LONG* value) override { if (!value) return DSERR_INVALIDPARAM; *value = buffer_.volume(); return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPan(LONG* value) override { if (!value) return DSERR_INVALIDPARAM; *value = buffer_.pan(); return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetFrequency(DWORD* value) override { if (!value) return DSERR_INVALIDPARAM; *value = buffer_.frequency(); return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetStatus(DWORD* status) override { if (!status) return DSERR_INVALIDPARAM; *status = Sdl3MixerAudioBackend::Instance().IsPlaying(voice_) ? DSBSTATUS_PLAYING | (buffer_.looping() ? DSBSTATUS_LOOPING : 0) : 0; return DS_OK; }
    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND, const DSBUFFERDESC*) override { return DSERR_ALREADYINITIALIZED; }
    HRESULT STDMETHODCALLTYPE Lock(DWORD offset, DWORD bytes, void** first, DWORD* first_bytes, void** second, DWORD* second_bytes, DWORD flags) override { if (!first || !first_bytes) return DSERR_INVALIDPARAM; LegacyAudioLock lock; if (!buffer_.Lock(offset, bytes, (flags & DSBLOCK_ENTIREBUFFER) != 0, &lock)) return DSERR_INVALIDPARAM; active_lock_ = lock; *first = lock.first.data(); *first_bytes = static_cast<DWORD>(lock.first.size()); if (second) *second = lock.second.empty() ? nullptr : lock.second.data(); if (second_bytes) *second_bytes = static_cast<DWORD>(lock.second.size()); TraceBuffer("lock", flags, *first_bytes + (second_bytes ? *second_bytes : 0)); return DS_OK; }
    HRESULT STDMETHODCALLTYPE Play(DWORD, DWORD, DWORD flags) override { buffer_.set_playing(true, (flags & DSBPLAY_LOOPING) != 0); TraceBuffer("play", flags, static_cast<DWORD>(buffer_.byte_count())); if (!play_traced_) { const PcmLevels levels = MeasurePcm(buffer_); Re2djAudioTrace("directsound:first-play buffer=%p flags=0x%08lx bytes=%lu samples=%lu peak=%.9f rms=%.9f volume=%ld linear=%.9f pan=%ld frequency=%lu", this, static_cast<unsigned long>(flags), static_cast<unsigned long>(buffer_.byte_count()), static_cast<unsigned long>(levels.sample_count), levels.peak, levels.rms, static_cast<long>(buffer_.volume()), std::pow(10.0, static_cast<double>(buffer_.volume()) / 2000.0), static_cast<long>(buffer_.pan()), static_cast<unsigned long>(buffer_.frequency())); play_traced_ = true; } return Sdl3MixerAudioBackend::Instance().Play(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD position) override { buffer_.set_current_position(position); return Sdl3MixerAudioBackend::Instance().SetPosition(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetFormat(const WAVEFORMATEX* format) override { if (!format) return DSERR_INVALIDPARAM; wave_ = *format; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetVolume(LONG value) override { buffer_.set_volume(value); Re2djAudioTrace("directsound:set-volume buffer=%p requested=%ld applied=%ld linear=%.9f", this, static_cast<long>(value), static_cast<long>(buffer_.volume()), std::pow(10.0, static_cast<double>(buffer_.volume()) / 2000.0)); return Sdl3MixerAudioBackend::Instance().UpdateControls(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetPan(LONG value) override { buffer_.set_pan(value); return Sdl3MixerAudioBackend::Instance().UpdateControls(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD value) override { buffer_.set_frequency(value == DSBFREQUENCY_ORIGINAL ? wave_.nSamplesPerSec : value); return Sdl3MixerAudioBackend::Instance().UpdateControls(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE Stop() override { buffer_.set_playing(false, false); return Sdl3MixerAudioBackend::Instance().Stop(voice_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE Unlock(void* first, DWORD first_bytes, void* second, DWORD second_bytes) override { LegacyAudioLock lock{std::span<std::byte>(static_cast<std::byte*>(first), first_bytes), std::span<std::byte>(static_cast<std::byte*>(second), second_bytes)}; if (!buffer_.ValidateUnlock(lock) || first != active_lock_.first.data() || first_bytes != active_lock_.first.size() || second_bytes != active_lock_.second.size() || (second_bytes && second != active_lock_.second.data())) return DSERR_INVALIDPARAM; active_lock_ = {}; TraceBuffer("unlock", flags_, first_bytes + second_bytes); if (buffer_.playing() && streaming_unlock_traces_ < 8) { const PcmLevels levels = MeasurePcm(buffer_); Re2djAudioTrace("directsound:streaming-unlock buffer=%p update=%u bytes=%lu peak=%.9f rms=%.9f backend-refresh=0", this, streaming_unlock_traces_ + 1, static_cast<unsigned long>(first_bytes + second_bytes), levels.peak, levels.rms); ++streaming_unlock_traces_; } return DS_OK; }
    HRESULT STDMETHODCALLTYPE Restore() override { return DS_OK; }
    bool is_primary() const { return (flags_ & DSBCAPS_PRIMARYBUFFER) != 0; }
    DWORD flags() const { return flags_; }
    DWORD byte_count() const { return static_cast<DWORD>(buffer_.byte_count()); }
private:
    std::atomic<ULONG> refs_{1}; DWORD flags_; WAVEFORMATEX wave_{}; LegacyAudioBuffer buffer_; LegacyAudioLock active_lock_{}; Sdl3MixerAudioBackend::Voice* voice_ = nullptr; bool play_traced_ = false; unsigned streaming_unlock_traces_ = 0;
};

class DirectSoundFacade final : public IDirectSound
{
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override { if (!object) return E_POINTER; *object = nullptr; if (iid == IID_IUnknown || iid == IID_IDirectSound) { *object = this; AddRef(); return S_OK; } return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG value = --refs_; if (!value) delete this; return value; }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(const DSBUFFERDESC* desc, LPDIRECTSOUNDBUFFER* output, IUnknown* outer) override
    {
        if (!output) return DSERR_INVALIDPARAM;
        *output = nullptr;
        if (outer) return DSERR_NOAGGREGATION;
        if (!desc || desc->dwSize < sizeof(DSBUFFERDESC1)) return DSERR_INVALIDPARAM;
        DSBUFFERDESC normalized = *desc;
        WAVEFORMATEX primary_format = {WAVE_FORMAT_PCM, 2, 48000, 192000, 4, 16, 0};
        const bool primary = (desc->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
        if (primary)
        {
            normalized.dwBufferBytes = 4096;
            normalized.lpwfxFormat = &primary_format;
        }
        TraceBuffer(primary ? "create-primary" : "create-secondary",
                    normalized.dwFlags, normalized.dwBufferBytes);
        if (!normalized.lpwfxFormat || normalized.lpwfxFormat->wFormatTag != WAVE_FORMAT_PCM ||
            normalized.dwBufferBytes == 0 || normalized.dwBufferBytes > 64u * 1024u * 1024u)
            return DSERR_BADFORMAT;
        auto* buffer = new (std::nothrow) DirectSoundBufferFacade(normalized, *normalized.lpwfxFormat);
        if (!buffer) return DSERR_OUTOFMEMORY;
        if (!buffer->ready()) { buffer->Release(); return DSERR_NODRIVER; }
        *output = buffer;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCaps(DSCAPS* caps) override { if (!caps || caps->dwSize < sizeof(DSCAPS)) return DSERR_INVALIDPARAM; std::memset(caps, 0, sizeof(*caps)); caps->dwSize = sizeof(DSCAPS); caps->dwFlags = DSCAPS_PRIMARYSTEREO | DSCAPS_PRIMARY16BIT; return DS_OK; }
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER original, LPDIRECTSOUNDBUFFER* output) override
    {
        if (!output) return DSERR_INVALIDPARAM;
        *output = nullptr;
        if (!original) return DSERR_INVALIDPARAM;
        auto* source = dynamic_cast<DirectSoundBufferFacade*>(original);
        if (!source) return DSERR_INVALIDPARAM;
        if (source->is_primary()) return DSERR_INVALIDCALL;
        auto* duplicate = new (std::nothrow) DirectSoundBufferFacade(*source);
        if (!duplicate) return DSERR_OUTOFMEMORY;
        if (!duplicate->ready()) { duplicate->Release(); return DSERR_NODRIVER; }
        *output = duplicate;
        char message[192] = {};
        std::snprintf(message, sizeof(message),
                      "re2dj:audio:duplicate:source=%p:result=%p:flags=0x%08x:bytes=%u",
                      static_cast<void*>(original), static_cast<void*>(*output),
                      static_cast<unsigned>(source->flags()),
                      static_cast<unsigned>(source->byte_count()));
        OutputDebugStringA(message);
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Compact() override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(DWORD* config) override { if (!config) return DSERR_INVALIDPARAM; *config = DSSPEAKER_STEREO; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Initialize(const GUID*) override { return DSERR_ALREADYINITIALIZED; }
private: std::atomic<ULONG> refs_{1};
};
}  // namespace

extern "C" HRESULT WINAPI Re2djHleDirectSoundCreate(GUID*, LPDIRECTSOUND* direct_sound, IUnknown* outer)
{
    if (!direct_sound) return DSERR_INVALIDPARAM;
    *direct_sound = nullptr;
    if (outer) return DSERR_NOAGGREGATION;
    auto& backend = Sdl3MixerAudioBackend::Instance();
    OutputDebugStringA("re2dj:audio:DirectSoundCreate");
    if (!backend.has_playback_device())
    {
        char message[512] = {};
        std::snprintf(message, sizeof(message), "re2dj:audio:sdl3-headless:%s", backend.error().c_str());
        OutputDebugStringA(message);
    }
    if (!backend.ready() ||
        !backend.SetMasterGain(static_cast<float>(g_re2dj_audio_master_gain)))
        return DSERR_NODRIVER;
    Re2djAudioTrace("directsound:create-device master-linear=%.9f", static_cast<double>(g_re2dj_audio_master_gain));
    auto* facade = new (std::nothrow) DirectSoundFacade;
    if (!facade) return DSERR_OUTOFMEMORY;
    *direct_sound = facade;
    return DS_OK;
}

extern "C" __declspec(dllexport) float WINAPI Re2djHleGetAudioMasterGain()
{
    return Sdl3MixerAudioBackend::Instance().master_gain();
}
