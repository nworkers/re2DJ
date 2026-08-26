#include "sdl3_mixer_audio_backend.h"

#include <algorithm>
#include <cmath>
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
Sdl3MixerAudioBackend& Sdl3MixerAudioBackend::Instance() { static Sdl3MixerAudioBackend backend; return backend; }
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
bool Sdl3MixerAudioBackend::Play(Voice* voice, const LegacyAudioBuffer& buffer)
{
    if (voice == nullptr || buffer.samples().empty()) return false;
    SDL_AudioSpec spec = {};
    spec.freq = static_cast<int>(buffer.format().sample_rate);
    spec.channels = static_cast<int>(buffer.format().channels);
    spec.format = buffer.format().bits_per_sample == 16 ? SDL_AUDIO_S16LE : SDL_AUDIO_U8;
    MIX_Audio* audio = MIX_LoadRawAudio(mixer_, buffer.samples().data(), buffer.samples().size(), &spec);
    if (audio == nullptr || !MIX_SetTrackAudio(voice->track, audio)) { MIX_DestroyAudio(audio); return false; }
    MIX_DestroyAudio(voice->audio);
    voice->audio = audio;
    if (!UpdateControls(voice, buffer)) return false;
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options == 0) return false;
    const std::uint32_t align = std::max<std::uint16_t>(1, buffer.format().block_align);
    const bool configured = SDL_SetNumberProperty(options, MIX_PROP_PLAY_START_FRAME_NUMBER,
                                                   buffer.current_position() / align) &&
                            SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER,
                                                   buffer.looping() ? -1 : 0);
    const bool played = configured && MIX_PlayTrack(voice->track, options);
    SDL_DestroyProperties(options);
    return played;
}
bool Sdl3MixerAudioBackend::Stop(Voice* voice) { return voice != nullptr && MIX_StopTrack(voice->track, 0); }
bool Sdl3MixerAudioBackend::SetPosition(Voice* voice, const LegacyAudioBuffer& buffer)
{
    if (voice == nullptr || voice->audio == nullptr) return true;
    const std::uint32_t align = std::max<std::uint16_t>(1, buffer.format().block_align);
    return MIX_SetTrackPlaybackPosition(voice->track, buffer.current_position() / align);
}
std::uint32_t Sdl3MixerAudioBackend::PositionBytes(Voice* voice, const LegacyAudioBuffer& buffer) const
{
    if (voice == nullptr || voice->audio == nullptr) return buffer.current_position();
    const Sint64 frames = MIX_GetTrackPlaybackPosition(voice->track);
    if (frames < 0) return buffer.current_position();
    return static_cast<std::uint32_t>((frames * buffer.format().block_align) % buffer.byte_count());
}
bool Sdl3MixerAudioBackend::IsPlaying(Voice* voice) const { return voice != nullptr && MIX_TrackPlaying(voice->track); }
}  // namespace re2dj::audio
