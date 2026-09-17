#ifndef RE2DJ_AUDIO_DIRECTSOUND_BUFFER_POLICY_H_
#define RE2DJ_AUDIO_DIRECTSOUND_BUFFER_POLICY_H_

#include <cstdint>

namespace re2dj::audio
{

// DirectSound buffer capability bits this policy reads. Mirrored here so the
// shared core never includes a platform SDK header; the values are fixed by
// the DirectSound ABI.
constexpr std::uint32_t kDsbcapsStatic = 0x00000002;
constexpr std::uint32_t kDsbcapsLocHardware = 0x00000004;
constexpr std::uint32_t kDsbcapsGetCurrentPosition2 = 0x00010000;

// Size of the looping ring every observed EZ2DJ and EZ2Dancer build streams
// its background audio through.
constexpr std::uint32_t kStreamingRingBytes = 360448;

// Whether a secondary buffer is a streaming ring the guest keeps refilling,
// rather than one-shot audio written once and played.
//
// The difference matters because the two take different playback paths, and a
// one-shot buffer on the streaming path never reaches an end. The builds
// disagree on flags: 1st Tracks and 1st SE rings carry LOCHARDWARE and STATIC,
// 2nd through 5th and EZ2Dancer 2nd MOVE rings only GETCURRENTPOSITION2, and
// 1st Tracks and 1st SE also put GETCURRENTPOSITION2 on their one-shot sound
// effects, together with STATIC.
//
// DirectSound defines STATIC only as a request for on-board sound card memory,
// not as a promise that the buffer plays once. Using it to tell effects apart
// is a heuristic fitted to the flags these builds were observed to use, which
// is why the ring size decides first.
//
// A buffer is therefore a ring when it has the ring size, or when it asks for
// hardware placement or precise positions without being static.
bool IsStreamingBufferDescription(std::uint32_t flags, std::uint32_t bytes);

}  // namespace re2dj::audio

#endif  // RE2DJ_AUDIO_DIRECTSOUND_BUFFER_POLICY_H_
