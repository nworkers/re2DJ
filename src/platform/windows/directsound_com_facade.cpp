#include "directsound_com_facade.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../audio/sdl3_mixer_audio_backend.h"
#include "re2dj/audio/legacy_audio_buffer.h"

namespace
{
using re2dj::audio::LegacyAudioBuffer;
using re2dj::audio::LegacyAudioFormat;
using re2dj::audio::LegacyAudioLock;
using re2dj::audio::Sdl3MixerAudioBackend;

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
          voice_(Sdl3MixerAudioBackend::Instance().CreateVoice()) {}
    DirectSoundBufferFacade(const DirectSoundBufferFacade& source)
        : flags_(source.flags_), wave_(source.wave_), buffer_(source.buffer_.Duplicate()),
          voice_(Sdl3MixerAudioBackend::Instance().CreateVoice()) {}
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
    HRESULT STDMETHODCALLTYPE Play(DWORD, DWORD, DWORD flags) override { buffer_.set_playing(true, (flags & DSBPLAY_LOOPING) != 0); TraceBuffer("play", flags, static_cast<DWORD>(buffer_.byte_count())); return Sdl3MixerAudioBackend::Instance().Play(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD position) override { buffer_.set_current_position(position); return Sdl3MixerAudioBackend::Instance().SetPosition(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetFormat(const WAVEFORMATEX* format) override { if (!format) return DSERR_INVALIDPARAM; wave_ = *format; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetVolume(LONG value) override { buffer_.set_volume(value); return Sdl3MixerAudioBackend::Instance().UpdateControls(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetPan(LONG value) override { buffer_.set_pan(value); return Sdl3MixerAudioBackend::Instance().UpdateControls(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD value) override { buffer_.set_frequency(value == DSBFREQUENCY_ORIGINAL ? wave_.nSamplesPerSec : value); return Sdl3MixerAudioBackend::Instance().UpdateControls(voice_, buffer_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE Stop() override { buffer_.set_playing(false, false); return Sdl3MixerAudioBackend::Instance().Stop(voice_) ? DS_OK : DSERR_GENERIC; }
    HRESULT STDMETHODCALLTYPE Unlock(void* first, DWORD first_bytes, void* second, DWORD second_bytes) override { LegacyAudioLock lock{std::span<std::byte>(static_cast<std::byte*>(first), first_bytes), std::span<std::byte>(static_cast<std::byte*>(second), second_bytes)}; if (!buffer_.ValidateUnlock(lock) || first != active_lock_.first.data() || first_bytes != active_lock_.first.size() || second_bytes != active_lock_.second.size() || (second_bytes && second != active_lock_.second.data())) return DSERR_INVALIDPARAM; active_lock_ = {}; TraceBuffer("unlock", flags_, first_bytes + second_bytes); return DS_OK; }
    HRESULT STDMETHODCALLTYPE Restore() override { return DS_OK; }
    bool is_primary() const { return (flags_ & DSBCAPS_PRIMARYBUFFER) != 0; }
    DWORD flags() const { return flags_; }
    DWORD byte_count() const { return static_cast<DWORD>(buffer_.byte_count()); }
private:
    std::atomic<ULONG> refs_{1}; DWORD flags_; WAVEFORMATEX wave_{}; LegacyAudioBuffer buffer_; LegacyAudioLock active_lock_{}; Sdl3MixerAudioBackend::Voice* voice_ = nullptr;
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
    if (!backend.ready()) return DSERR_NODRIVER;
    auto* facade = new (std::nothrow) DirectSoundFacade;
    if (!facade) return DSERR_OUTOFMEMORY;
    *direct_sound = facade;
    return DS_OK;
}
