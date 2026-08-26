#ifndef RE2DJ_AUDIO_SDL3_MIXER_AUDIO_BACKEND_H_
#define RE2DJ_AUDIO_SDL3_MIXER_AUDIO_BACKEND_H_

#include <cstdint>
#include <string>

#include "re2dj/audio/legacy_audio_buffer.h"

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;

namespace re2dj::audio
{
class Sdl3MixerAudioBackend
{
public:
    struct Voice { MIX_Track* track = nullptr; MIX_Audio* audio = nullptr; };
    static Sdl3MixerAudioBackend& Instance();
    bool ready() const;
    bool has_playback_device() const;
    const std::string& error() const;
    Voice* CreateVoice();
    void DestroyVoice(Voice* voice);
    bool Play(Voice* voice, const LegacyAudioBuffer& buffer);
    bool Stop(Voice* voice);
    bool SetPosition(Voice* voice, const LegacyAudioBuffer& buffer);
    bool UpdateControls(Voice* voice, const LegacyAudioBuffer& buffer);
    std::uint32_t PositionBytes(Voice* voice, const LegacyAudioBuffer& buffer) const;
    bool IsPlaying(Voice* voice) const;

private:
    Sdl3MixerAudioBackend();
    ~Sdl3MixerAudioBackend();
    MIX_Mixer* mixer_ = nullptr;
    bool initialized_ = false;
    bool has_playback_device_ = false;
    std::string error_;
};
}  // namespace re2dj::audio

#endif  // RE2DJ_AUDIO_SDL3_MIXER_AUDIO_BACKEND_H_
