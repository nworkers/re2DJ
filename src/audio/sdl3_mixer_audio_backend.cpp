#include "sdl3_mixer_audio_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

#if defined(_MSC_VER)
#pragma warning(push)
// SDL and SDL_mixer ship headers whose encoding MSVC flags as C4819 on
// non-UTF-8 host code pages; the warning is a property of upstream bytes.
#pragma warning(disable : 4819)
#endif
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace re2dj::audio
{
namespace
{
SDL_AudioSpec ToSdlSpec(const LegacyAudioBuffer& buffer)
{
    SDL_AudioSpec spec = {};
    spec.freq = static_cast<int>(buffer.format().sample_rate);
    spec.channels = static_cast<int>(buffer.format().channels);
    spec.format = buffer.format().bits_per_sample == 16 ? SDL_AUDIO_S16LE : SDL_AUDIO_U8;
    return spec;
}

bool QueueSpan(SDL_AudioStream* stream, std::span<const std::byte> samples)
{
    if (samples.empty()) return true;
    if (samples.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
    return SDL_PutAudioStreamData(stream, samples.data(), static_cast<int>(samples.size()));
}

}  // namespace

Sdl3MixerAudioBackend& Sdl3MixerAudioBackend::Instance()
{
    // SDL audio teardown is unsafe after Windows has begun process-wide thread shutdown.
    static Sdl3MixerAudioBackend* const backend = new Sdl3MixerAudioBackend;
    return *backend;
}
Sdl3MixerAudioBackend::Sdl3MixerAudioBackend()
{
    initialized_ = SDL_InitSubSystem(SDL_INIT_AUDIO) && MIX_Init();
    if (initialized_)
    {
        mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        has_playback_device_ = mixer_ != nullptr;
        if (mixer_ == nullptr)
        {
            error_ = SDL_GetError();
            const SDL_AudioSpec fallback = {SDL_AUDIO_F32, 2, 48000};
            mixer_ = MIX_CreateMixer(&fallback);
        }
    }
    else
    {
        error_ = SDL_GetError();
    }
}
Sdl3MixerAudioBackend::~Sdl3MixerAudioBackend()
{
    MIX_DestroyMixer(mixer_);
    if (initialized_) { MIX_Quit(); SDL_QuitSubSystem(SDL_INIT_AUDIO); }
}
bool Sdl3MixerAudioBackend::ready() const { return mixer_ != nullptr; }
bool Sdl3MixerAudioBackend::has_playback_device() const { return has_playback_device_; }
bool Sdl3MixerAudioBackend::SetDiagnosticCallback(DiagnosticCallback callback, void* userdata)
{
    diagnostic_callback_ = callback;
    diagnostic_userdata_ = userdata;
    if (mixer_ == nullptr) return false;

    const bool callback_set = MIX_SetPostMixCallback(mixer_, &PostMixCallback, this);
    SDL_AudioSpec spec = {};
    const bool format_available = MIX_GetMixerFormat(mixer_, &spec);
    Trace("sdl3:backend:ready=%u:playback-device=%u:postmix=%u:format=%u:freq=%d:channels=%d:error=%s",
          ready() ? 1u : 0u, has_playback_device_ ? 1u : 0u,
          callback_set ? 1u : 0u, static_cast<unsigned>(spec.format), spec.freq,
          spec.channels, error_.empty() ? "" : error_.c_str());
    return callback_set && format_available;
}
void Sdl3MixerAudioBackend::Trace(const char* format, ...) const
{
    if (diagnostic_callback_ == nullptr || format == nullptr) return;
    char message[768] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    diagnostic_callback_(message, diagnostic_userdata_);
}
void Sdl3MixerAudioBackend::PostMixCallback(void* userdata, MIX_Mixer* mixer,
                                            const SDL_AudioSpec* spec, float* pcm,
                                            int samples)
{
    (void)mixer;
    auto* backend = static_cast<Sdl3MixerAudioBackend*>(userdata);
    if (backend == nullptr || spec == nullptr || pcm == nullptr || samples <= 0) return;

    float peak = 0.0f;
    double sum_squared = 0.0;
    for (int index = 0; index < samples; ++index)
    {
        const float sample = pcm[index];
        peak = (std::max)(peak, std::abs(sample));
        sum_squared += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const double rms = std::sqrt(sum_squared / static_cast<double>(samples));
    const bool nonzero = peak > 0.000001f;
    const unsigned sequence = nonzero ?
        backend->postmix_nonzero_trace_count_.fetch_add(1) :
        backend->postmix_zero_trace_count_.fetch_add(1);
    const unsigned limit = nonzero ? 16u : 4u;
    if (sequence >= limit) return;
    backend->Trace("sdl3:postmix:audible=%u:sequence=%u:format=%u:freq=%d:channels=%d:samples=%d:peak=%.9f:rms=%.9f",
                   nonzero ? 1u : 0u, sequence, static_cast<unsigned>(spec->format),
                   spec->freq, spec->channels, samples, static_cast<double>(peak), rms);
}
void Sdl3MixerAudioBackend::TrackCookedCallback(void* userdata, MIX_Track* track,
                                                const SDL_AudioSpec* spec, float* pcm,
                                                int samples)
{
    (void)track;
    auto* voice = static_cast<Voice*>(userdata);
    if (voice == nullptr || voice->backend == nullptr ||
        !voice->cooked_trace_armed.load() || spec == nullptr || pcm == nullptr || samples <= 0)
        return;
    const unsigned sequence = voice->cooked_trace_count.fetch_add(1);
    // Retain sparse observations beyond startup silence, without per-frame logs.
    if (sequence >= 36000 || (sequence >= 16 && sequence % 100 != 0)) return;

    float peak = 0.0f;
    double sum_squared = 0.0;
    for (int index = 0; index < samples; ++index)
    {
        const float sample = pcm[index];
        peak = (std::max)(peak, std::abs(sample));
        sum_squared += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const double rms = std::sqrt(sum_squared / static_cast<double>(samples));
    voice->backend->Trace(
        "sdl3:track-cooked:streaming=1:sequence=%u:format=%u:freq=%d:channels=%d:samples=%d:peak=%.9f:rms=%.9f",
        sequence, static_cast<unsigned>(spec->format), spec->freq, spec->channels,
        samples, static_cast<double>(peak), rms);
}
void Sdl3MixerAudioBackend::StreamingGetCallback(void* userdata,
                                                 SDL_AudioStream* stream,
                                                 int additional_amount,
                                                 int total_amount)
{
    (void)total_amount;
    auto* voice = static_cast<Voice*>(userdata);
    if (voice == nullptr || stream == nullptr || additional_amount <= 0 ||
        voice->stream_ring.empty())
        return;

    const std::size_t align = (std::max<std::size_t>)(1, voice->stream_block_align);
    std::size_t remaining = static_cast<std::size_t>(additional_amount);
    remaining = ((remaining + align - 1) / align) * align;
    while (remaining != 0)
    {
        const std::size_t available = voice->stream_ring.size() - voice->stream_read_position;
        const std::size_t bytes = (std::min)(remaining, available);
        const auto span = std::span<const std::byte>(voice->stream_ring)
                              .subspan(voice->stream_read_position, bytes);
        if (!QueueSpan(stream, span)) return;
        voice->stream_read_position =
            (voice->stream_read_position + bytes) % voice->stream_ring.size();
        remaining -= bytes;
    }
}
bool Sdl3MixerAudioBackend::SetMasterGain(float gain)
{
    return mixer_ != nullptr && MIX_SetMixerGain(mixer_, (std::max)(0.0f, gain));
}
float Sdl3MixerAudioBackend::master_gain() const
{
    return mixer_ == nullptr ? 0.0f : MIX_GetMixerGain(mixer_);
}
float Sdl3MixerAudioBackend::TrackGain(Voice* voice) const
{
    return voice == nullptr || voice->track == nullptr ? 0.0f : MIX_GetTrackGain(voice->track);
}
const std::string& Sdl3MixerAudioBackend::error() const { return error_; }
Sdl3MixerAudioBackend::Voice* Sdl3MixerAudioBackend::CreateVoice()
{
    if (!ready()) return nullptr;
    auto* voice = new (std::nothrow) Voice;
    if (voice != nullptr)
    {
        voice->backend = this;
        voice->track = MIX_CreateTrack(mixer_);
        if (voice->track != nullptr)
            MIX_SetTrackCookedCallback(voice->track, &TrackCookedCallback, voice);
    }
    if (voice != nullptr && voice->track == nullptr) { delete voice; return nullptr; }
    return voice;
}
void Sdl3MixerAudioBackend::DestroyVoice(Voice* voice)
{
    if (voice == nullptr) return;
    voice->cooked_trace_armed.store(false);
    MIX_DestroyTrack(voice->track);
    MIX_DestroyAudio(voice->audio);
    SDL_DestroyAudioStream(voice->stream);
    delete voice;
}
bool Sdl3MixerAudioBackend::ResetTrack(Voice* voice)
{
    if (voice == nullptr || voice->track == nullptr || MIX_TrackPlaying(voice->track))
        return false;

    voice->cooked_trace_armed.store(false);
    MIX_DestroyTrack(voice->track);
    voice->track = MIX_CreateTrack(mixer_);
    if (voice->track == nullptr)
        return false;

    if (!MIX_SetTrackCookedCallback(voice->track, &TrackCookedCallback, voice))
    {
        MIX_DestroyTrack(voice->track);
        voice->track = nullptr;
        return false;
    }
    if (voice->stream != nullptr && !MIX_SetTrackAudioStream(voice->track, voice->stream))
    {
        MIX_DestroyTrack(voice->track);
        voice->track = nullptr;
        return false;
    }
    return true;
}
bool Sdl3MixerAudioBackend::UpdateControls(Voice* voice, const LegacyAudioBuffer& buffer)
{
    if (voice == nullptr) return false;
    const float gain = std::pow(10.0f, static_cast<float>(buffer.volume()) / 2000.0f);
    const float pan = static_cast<float>(buffer.pan()) / 10000.0f;
    MIX_StereoGains stereo = {pan <= 0.0f ? 1.0f : 1.0f - pan,
                              pan >= 0.0f ? 1.0f : 1.0f + pan};
    const float ratio = buffer.format().sample_rate == 0 ? 1.0f :
        static_cast<float>(buffer.frequency()) / static_cast<float>(buffer.format().sample_rate);
    return MIX_SetTrackGain(voice->track, gain) && MIX_SetTrackStereo(voice->track, &stereo) &&
           MIX_SetTrackFrequencyRatio(voice->track, std::clamp(ratio, 0.01f, 100.0f));
}
bool Sdl3MixerAudioBackend::Play(Voice* voice, const LegacyAudioBuffer& buffer, bool streaming)
{
    if (voice == nullptr || buffer.samples().empty()) return false;
    voice->last_play_continued.store(false);
    if (streaming && MIX_TrackPlaying(voice->track))
    {
        const bool updated = UpdateControls(voice, buffer);
        voice->last_play_continued.store(updated);
        Trace("sdl3:play:streaming=1:configured=%u:played=%u:continued=1:restarted=0:queued=%d:track-playing=%u",
              updated ? 1u : 0u, updated ? 1u : 0u,
              StreamingQueuedBytes(voice), IsPlaying(voice) ? 1u : 0u);
        return updated;
    }
    const SDL_AudioSpec spec = ToSdlSpec(buffer);
    bool track_reset = false;
    if (streaming)
    {
        if (voice->stream != nullptr)
        {
            track_reset = true;
            if (!ResetTrack(voice)) return false;
        }
        if (voice->stream == nullptr)
        {
            voice->stream = SDL_CreateAudioStream(&spec, nullptr);
            if (voice->stream == nullptr ||
                !SDL_SetAudioStreamGetCallback(voice->stream,
                                               &StreamingGetCallback, voice) ||
                !MIX_SetTrackAudioStream(voice->track, voice->stream))
            {
                SDL_DestroyAudioStream(voice->stream);
                voice->stream = nullptr;
                return false;
            }
        }
        if (!SDL_ClearAudioStream(voice->stream))
            return false;
        voice->stream_start_position = buffer.current_position();
        voice->stream_read_position = buffer.current_position() % buffer.samples().size();
        voice->stream_block_align =
            (std::max<std::uint16_t>)(1, buffer.format().block_align);
        voice->stream_ring.assign(buffer.samples().begin(), buffer.samples().end());
    }
    else
    {
        MIX_Audio* audio = MIX_LoadRawAudio(mixer_, buffer.samples().data(), buffer.samples().size(), &spec);
        if (audio == nullptr || !MIX_SetTrackAudio(voice->track, audio))
        {
            MIX_DestroyAudio(audio);
            return false;
        }
        MIX_DestroyAudio(voice->audio);
        voice->audio = audio;
    }
    if (!UpdateControls(voice, buffer)) return false;
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options == 0) return false;
    const std::uint32_t align = std::max<std::uint16_t>(1, buffer.format().block_align);
    const bool configured =
        (streaming || SDL_SetNumberProperty(options, MIX_PROP_PLAY_START_FRAME_NUMBER,
                                             buffer.current_position() / align)) &&
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER,
                              streaming ? 0 : (buffer.looping() ? -1 : 0)) &&
        (!streaming || SDL_SetBooleanProperty(options,
                                               MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN,
                                               false));
    const bool played = configured && MIX_PlayTrack(voice->track, options);
    SDL_DestroyProperties(options);
    if (played && streaming) voice->cooked_trace_count.store(0);
    voice->cooked_trace_armed.store(played && streaming);
    Trace("sdl3:play:streaming=%u:configured=%u:played=%u:continued=0:restarted=%u:track-reset=%u:queued=%d:track-playing=%u",
          streaming ? 1u : 0u, configured ? 1u : 0u, played ? 1u : 0u,
          streaming ? 1u : 0u, track_reset ? 1u : 0u,
          StreamingQueuedBytes(voice), IsPlaying(voice) ? 1u : 0u);
    return played;
}
Sdl3MixerAudioBackend::StreamingWriteResult
Sdl3MixerAudioBackend::CommitStreamingWrite(Voice* voice,
                                            const LegacyAudioBuffer& buffer)
{
    StreamingWriteResult result;
    if (voice == nullptr || voice->stream == nullptr || !buffer.playing())
    {
        result.success = true;
        return result;
    }
    const auto samples = buffer.samples();
    if (samples.empty() || !SDL_LockAudioStream(voice->stream)) return result;
    if (voice->stream_ring.size() == samples.size())
    {
        std::memcpy(voice->stream_ring.data(), samples.data(), samples.size());
        result.success = true;
        result.offset = 0;
        result.bytes = samples.size();
    }
    SDL_UnlockAudioStream(voice->stream);
    result.queued_bytes = SDL_GetAudioStreamQueued(voice->stream);
    return result;
}
bool Sdl3MixerAudioBackend::Stop(Voice* voice)
{
    if (voice == nullptr) return false;
    voice->cooked_trace_armed.store(false);
    return MIX_StopTrack(voice->track, 0);
}
bool Sdl3MixerAudioBackend::SetPosition(Voice* voice, const LegacyAudioBuffer& buffer)
{
    if (voice == nullptr) return false;
    if (voice->stream != nullptr)
    {
        voice->stream_start_position = buffer.current_position();
        if (!buffer.playing()) return true;
        if (!Stop(voice)) return false;
        return Play(voice, buffer, true);
    }
    if (voice->audio == nullptr) return true;
    const std::uint32_t align = std::max<std::uint16_t>(1, buffer.format().block_align);
    return MIX_SetTrackPlaybackPosition(voice->track, buffer.current_position() / align);
}
std::uint32_t Sdl3MixerAudioBackend::PositionBytes(Voice* voice, const LegacyAudioBuffer& buffer) const
{
    if (voice == nullptr || (voice->audio == nullptr && voice->stream == nullptr))
        return buffer.current_position();
    if (voice->stream != nullptr && !MIX_TrackPlaying(voice->track))
        return buffer.current_position();
    const Sint64 frames = MIX_GetTrackPlaybackPosition(voice->track);
    if (frames < 0) return buffer.current_position();
    const std::uint64_t relative = static_cast<std::uint64_t>(frames) * buffer.format().block_align;
    const std::uint64_t absolute = voice->stream == nullptr ? relative :
        relative + voice->stream_start_position;
    return static_cast<std::uint32_t>(absolute % buffer.byte_count());
}
int Sdl3MixerAudioBackend::StreamingQueuedBytes(Voice* voice) const
{
    return voice == nullptr || voice->stream == nullptr ? 0 : SDL_GetAudioStreamQueued(voice->stream);
}
bool Sdl3MixerAudioBackend::IsPlaying(Voice* voice) const { return voice != nullptr && MIX_TrackPlaying(voice->track); }
bool Sdl3MixerAudioBackend::LastPlayContinued(Voice* voice) const
{
    return voice != nullptr && voice->last_play_continued.load();
}
}  // namespace re2dj::audio
