#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "sdl3_mixer_audio_backend.h"

namespace
{
void Cooked(void* userdata, MIX_Track*, const SDL_AudioSpec*, float* pcm, int samples)
{
    auto* saw_negative = static_cast<std::atomic<bool>*>(userdata);
    for (int index = 0; index < samples; ++index)
        if (pcm[index] < -0.1f) saw_negative->store(true);
}
}

int main()
{
    using namespace re2dj::audio;
    auto& backend = Sdl3MixerAudioBackend::Instance();
    auto* voice = backend.CreateVoice();
    if (!backend.has_playback_device() || voice == nullptr) return 1;
    LegacyAudioBuffer buffer({2, 44100, 16, 4}, 35280);
    LegacyAudioLock lock;
    if (!buffer.Lock(0, 0, true, &lock)) return 2;
    for (std::size_t offset = 0; offset < lock.first.size(); offset += 2)
    {
        const std::int16_t sample = 8192;
        std::memcpy(lock.first.data() + offset, &sample, sizeof(sample));
    }
    buffer.set_playing(true, true);
    if (!backend.Play(voice, buffer, true)) return 3;
    std::atomic<bool> saw_negative{false};
    MIX_SetTrackCookedCallback(voice->track, Cooked, &saw_negative);
    bool passed = true;
    for (int tick = 0; tick < 100; ++tick)
    {
        SDL_Delay(10);
        passed = backend.CommitStreamingWrite(voice, buffer).success && passed;
    }
    const auto consumed_frames = MIX_GetTrackPlaybackPosition(voice->track);
    if (!backend.Play(voice, buffer, true) || !backend.LastPlayContinued(voice))
        passed = false;
    if (backend.StreamingQueuedBytes(voice) > static_cast<int>(buffer.byte_count()))
        passed = false;
    if (consumed_frames < static_cast<Sint64>(buffer.byte_count() / buffer.format().block_align))
    {
        std::fprintf(stderr, "Initial stream did not consume a full ring: %lld frames\n",
                     static_cast<long long>(consumed_frames));
        passed = false;
    }
    for (std::size_t offset = 0; offset < lock.first.size(); offset += 2)
    {
        const std::int16_t sample = -8192;
        std::memcpy(lock.first.data() + offset, &sample, sizeof(sample));
    }
    for (int tick = 0; tick < 60; ++tick)
    {
        passed = backend.CommitStreamingWrite(voice, buffer).success && passed;
        SDL_Delay(10);
    }
    if (!saw_negative.load())
    {
        std::fprintf(stderr, "Changed ring data never reached cooked output\n");
        passed = false;
    }
    backend.Stop(voice);
    buffer.set_playing(false, false);
    buffer.set_current_position(12);
    if (!backend.SetPosition(voice, buffer) || backend.PositionBytes(voice, buffer) != 12)
        passed = false;
    buffer.set_playing(true, true);
    if (!backend.Play(voice, buffer, true) || backend.LastPlayContinued(voice) ||
        !backend.IsPlaying(voice))
        passed = false;
    backend.Stop(voice);
    backend.DestroyVoice(voice);
    return passed ? 0 : 4;
}
