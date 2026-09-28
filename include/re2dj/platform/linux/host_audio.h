#ifndef RE2DJ_PLATFORM_LINUX_HOST_AUDIO_H_
#define RE2DJ_PLATFORM_LINUX_HOST_AUDIO_H_

#include <cstdint>
#include <map>
#include <string>

#include "re2dj/hle/host_audio.h"

namespace re2dj::audio
{
class Sdl3MixerAudioBackend;
}

namespace re2dj::platform::linux
{

// The Linux host's sound output: the shared SDL3_mixer backend the Windows
// DirectSound facade plays through, on the default playback device.
class LinuxHostAudio final : public hle::HostAudio
{
public:
    // Opens the backend with the master gain (linear) the Windows host takes
    // from --audio-gain-db or the profile. False with error when it cannot
    // mix at all; with no playback device it still mixes, unheard, and
    // headless_reason says why.
    bool Initialize(float master_gain, std::string* error);
    const std::string& headless_reason() const { return headless_reason_; }

    std::uint32_t CreateVoice() override;
    void DestroyVoice(std::uint32_t voice) override;
    bool Play(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer, bool streaming) override;
    bool CommitStreamingWrite(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) override;
    bool Stop(std::uint32_t voice) override;
    bool SetPosition(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) override;
    bool UpdateControls(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) override;
    std::uint32_t PositionBytes(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) const override;
    bool IsPlaying(std::uint32_t voice) const override;

private:
    // The backend's voice behind an id (its Sdl3MixerAudioBackend::Voice),
    // or null for an unknown id, which the backend plays as nothing.
    void* FindVoice(std::uint32_t voice) const;

    audio::Sdl3MixerAudioBackend* backend_ = nullptr;
    std::string headless_reason_;
    std::uint32_t next_voice_ = 1;
    std::map<std::uint32_t, void*> voices_;
};

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_HOST_AUDIO_H_
