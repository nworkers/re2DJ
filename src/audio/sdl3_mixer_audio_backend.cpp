#include "sdl3_mixer_audio_backend.h"

#include <algorithm>
#include <cmath>
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

bool QueueRing(SDL_AudioStream* stream, const LegacyAudioBuffer& buffer,
               std::uint32_t start_position)
{
    const auto samples = buffer.samples();
    if (samples.empty()) return false;
    const std::size_t start = start_position % samples.size();
    return QueueSpan(stream, samples.subspan(start)) && QueueSpan(stream, samples.first(start));
}

bool QueueRingRange(SDL_AudioStream* stream, std::span<const std::byte> samples,
                    std::size_t offset, std::size_t bytes)
{
    if (samples.empty() || offset >= samples.size() || bytes > samples.size()) return false;
    const std::size_t first_size = (std::min)(bytes, samples.size() - offset);
    return QueueSpan(stream, samples.subspan(offset, first_size)) &&
           QueueSpan(stream, samples.first(bytes - first_size));
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
    if (voice != nullptr) voice->track = MIX_CreateTrack(mixer_);
    if (voice != nullptr && voice->track == nullptr) { delete voice; return nullptr; }
    return voice;
}
void Sdl3MixerAudioBackend::DestroyVoice(Voice* voice)
{
    if (voice == nullptr) return;
    MIX_DestroyTrack(voice->track);
    MIX_DestroyAudio(voice->audio);
    SDL_DestroyAudioStream(voice->stream);
    delete voice;
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
    const SDL_AudioSpec spec = ToSdlSpec(buffer);
    if (streaming)
    {
        if (voice->stream == nullptr)
        {
            voice->stream = SDL_CreateAudioStream(&spec, nullptr);
            if (voice->stream == nullptr || !MIX_SetTrackAudioStream(voice->track, voice->stream))
            {
                SDL_DestroyAudioStream(voice->stream);
                voice->stream = nullptr;
                return false;
            }
        }
        if (!SDL_ClearAudioStream(voice->stream) ||
            !QueueRing(voice->stream, buffer, buffer.current_position()))
            return false;
        voice->stream_start_position = buffer.current_position();
        voice->committed_samples.assign(buffer.samples().begin(), buffer.samples().end());
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
                              streaming ? 0 : (buffer.looping() ? -1 : 0));
    const bool played = configured && MIX_PlayTrack(voice->track, options);
    SDL_DestroyProperties(options);
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
    const std::size_t align = (std::max<std::uint16_t>)(1, buffer.format().block_align);
    const std::size_t frame_count = samples.size() / align;
    if (frame_count == 0 || voice->committed_samples.size() != samples.size()) return result;

    std::vector<std::size_t> changed_frames;
    changed_frames.reserve(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame)
    {
        const std::size_t offset = frame * align;
        if (std::memcmp(samples.data() + offset,
                        voice->committed_samples.data() + offset,
                        align) != 0)
            changed_frames.push_back(frame);
    }
    if (changed_frames.empty())
    {
        result.success = true;
        result.queued_bytes = SDL_GetAudioStreamQueued(voice->stream);
        return result;
    }

    std::size_t largest_unchanged_gap = 0;
    std::size_t dirty_start_frame = changed_frames.front();
    for (std::size_t index = 0; index < changed_frames.size(); ++index)
    {
        const std::size_t current = changed_frames[index];
        const std::size_t next = index + 1 < changed_frames.size() ?
            changed_frames[index + 1] : changed_frames.front() + frame_count;
        const std::size_t gap = next - current - 1;
        if (gap > largest_unchanged_gap)
        {
            largest_unchanged_gap = gap;
            dirty_start_frame = next % frame_count;
        }
    }
    result.offset = dirty_start_frame * align;
    result.bytes = (frame_count - largest_unchanged_gap) * align;
    result.success = QueueRingRange(voice->stream, samples, result.offset, result.bytes);
    if (result.success)
        voice->committed_samples.assign(samples.begin(), samples.end());
    result.queued_bytes = SDL_GetAudioStreamQueued(voice->stream);
    return result;
}
bool Sdl3MixerAudioBackend::Stop(Voice* voice) { return voice != nullptr && MIX_StopTrack(voice->track, 0); }
bool Sdl3MixerAudioBackend::SetPosition(Voice* voice, const LegacyAudioBuffer& buffer)
{
    if (voice == nullptr) return false;
    if (voice->stream != nullptr)
    {
        voice->stream_start_position = buffer.current_position();
        if (!buffer.playing()) return true;
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
}  // namespace re2dj::audio
