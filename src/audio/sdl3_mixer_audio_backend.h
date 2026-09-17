#ifndef RE2DJ_AUDIO_SDL3_MIXER_AUDIO_BACKEND_H_
#define RE2DJ_AUDIO_SDL3_MIXER_AUDIO_BACKEND_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/audio/legacy_audio_buffer.h"

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;
struct SDL_AudioSpec;
struct SDL_AudioStream;

namespace re2dj::audio
{
class Sdl3MixerAudioBackend
{
public:
    using DiagnosticCallback = void (*)(const char* message, void* userdata);

    struct StreamingWriteResult
    {
        bool success = false;
        std::size_t offset = 0;
        std::size_t bytes = 0;
        int queued_bytes = 0;
    };
    struct Voice
    {
        Sdl3MixerAudioBackend* backend = nullptr;
        MIX_Track* track = nullptr;
        MIX_Audio* audio = nullptr;
        SDL_AudioStream* stream = nullptr;
        std::uint32_t stream_start_position = 0;
        std::size_t stream_read_position = 0;
        std::size_t stream_block_align = 1;
        std::vector<std::byte> stream_ring;
        // Set when a streaming voice was played without DSBPLAY_LOOPING. Such a
        // play delivers the ring once, from its start position to the end of
        // the buffer, and then lets the track run dry and halt, which is what
        // DirectSound does with a non-looping play regardless of how the buffer
        // was created. Written before the track starts and read on the audio
        // thread, hence atomic.
        std::atomic<bool> stream_once{false};
        std::atomic<std::size_t> stream_once_remaining{0};
        std::atomic<bool> cooked_trace_armed{false};
        std::atomic<unsigned> cooked_trace_count{0};
        std::atomic<bool> last_play_continued{false};
    };
    static Sdl3MixerAudioBackend& Instance();
    bool ready() const;
    bool has_playback_device() const;
    bool SetDiagnosticCallback(DiagnosticCallback callback, void* userdata);
    bool SetMasterGain(float gain);
    float master_gain() const;
    float TrackGain(Voice* voice) const;
    const std::string& error() const;
    Voice* CreateVoice();
    void DestroyVoice(Voice* voice);
    bool Play(Voice* voice, const LegacyAudioBuffer& buffer, bool streaming);
    StreamingWriteResult CommitStreamingWrite(Voice* voice,
                                              const LegacyAudioBuffer& buffer);
    bool Stop(Voice* voice);
    bool SetPosition(Voice* voice, const LegacyAudioBuffer& buffer);
    bool UpdateControls(Voice* voice, const LegacyAudioBuffer& buffer);
    std::uint32_t PositionBytes(Voice* voice, const LegacyAudioBuffer& buffer) const;
    int StreamingQueuedBytes(Voice* voice) const;
    bool IsPlaying(Voice* voice) const;
    bool LastPlayContinued(Voice* voice) const;

private:
    Sdl3MixerAudioBackend();
    ~Sdl3MixerAudioBackend();
    bool ResetTrack(Voice* voice);
    void Trace(const char* format, ...) const;
    static void PostMixCallback(void* userdata, MIX_Mixer* mixer,
                                const SDL_AudioSpec* spec, float* pcm,
                                int samples);
    static void TrackCookedCallback(void* userdata, MIX_Track* track,
                                    const SDL_AudioSpec* spec, float* pcm,
                                    int samples);
    static void StreamingGetCallback(void* userdata, SDL_AudioStream* stream,
                                     int additional_amount, int total_amount);

    MIX_Mixer* mixer_ = nullptr;
    bool initialized_ = false;
    bool has_playback_device_ = false;
    std::string error_;
    DiagnosticCallback diagnostic_callback_ = nullptr;
    void* diagnostic_userdata_ = nullptr;
    mutable std::atomic<unsigned> postmix_zero_trace_count_{0};
    mutable std::atomic<unsigned> postmix_nonzero_trace_count_{0};
};
}  // namespace re2dj::audio

#endif  // RE2DJ_AUDIO_SDL3_MIXER_AUDIO_BACKEND_H_
