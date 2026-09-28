#ifndef RE2DJ_HLE_HOST_AUDIO_H_
#define RE2DJ_HLE_HOST_AUDIO_H_

#include <cstdint>

namespace re2dj::audio
{
class LegacyAudioBuffer;
}

namespace re2dj::hle
{

// The host's sound output, as the dsound facade drives it: one voice per
// guest buffer, fed from a host copy of the buffer's samples and controls
// (audio::LegacyAudioBuffer) the facade keeps. The calls and their order are
// the Windows DirectSound facade's, so both hosts play a buffer alike. The
// host outlives the guest process. Without one, buffers play silently.
class HostAudio
{
public:
    virtual ~HostAudio() = default;

    // A new voice, or 0 when the host cannot make one.
    virtual std::uint32_t CreateVoice() = 0;
    virtual void DestroyVoice(std::uint32_t voice) = 0;
    // Starts the buffer from its current position; a streaming buffer is fed
    // as the guest writes it, the others from what they hold.
    virtual bool Play(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer, bool streaming) = 0;
    // A streaming buffer's newly unlocked samples.
    virtual bool CommitStreamingWrite(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) = 0;
    virtual bool Stop(std::uint32_t voice) = 0;
    virtual bool SetPosition(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) = 0;
    // Volume, pan, and frequency.
    virtual bool UpdateControls(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) = 0;
    // The play cursor, in bytes into the buffer.
    virtual std::uint32_t PositionBytes(std::uint32_t voice, const audio::LegacyAudioBuffer& buffer) const = 0;
    virtual bool IsPlaying(std::uint32_t voice) const = 0;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_HOST_AUDIO_H_
