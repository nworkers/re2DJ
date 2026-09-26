#ifndef RE2DJ_AUDIO_DIRECTSOUND_DEVICE_H_
#define RE2DJ_AUDIO_DIRECTSOUND_DEVICE_H_

#include <cstdint>

#include "re2dj/audio/directsound_abi.h"

// DirectSound as the HLE models it: which buffers CreateSoundBuffer makes,
// what the device and its buffers report, and how a lock divides a buffer.
// Both hosts' facades follow these rules; each keeps the samples and plays
// them its own way.
namespace re2dj::audio
{

// The primary buffer's fixed shape: 4096 bytes of 48 kHz 16-bit stereo PCM,
// whatever the guest asks for.
inline constexpr std::uint32_t kPrimaryBufferBytes = 4096;
inline constexpr WaveFormatEx kPrimaryFormat = {kWaveFormatPcm, 2, 48000, 192000, 4, 16, 0};
inline constexpr std::uint32_t kMaxSoundBufferBytes = 64U * 1024U * 1024U;

// What CreateSoundBuffer makes of a description.
struct SoundBufferPlan
{
    std::uint32_t result = kDsOk;
    bool primary = false;
    std::uint32_t flags = 0;
    std::uint32_t bytes = 0;
    WaveFormatEx format;
};

// IDirectSound::CreateSoundBuffer's rules for a DSBUFFERDESC whose format,
// when it names one, the facade has read into *format (null when it names
// none):
// - dwSize below DSBUFFERDESC1's is DSERR_INVALIDPARAM;
// - a primary buffer takes the fixed primary shape;
// - otherwise the format must be PCM and the size from 1 byte to 64 MiB, or
//   it is DSERR_BADFORMAT.
SoundBufferPlan PlanSoundBuffer(const DsBufferDesc& description, const WaveFormatEx* format);

// IDirectSound::GetCaps: primary stereo, 16-bit.
DsCaps DeviceCaps();
// IDirectSoundBuffer::GetCaps: the buffer's flags and size.
DsbCaps BufferCaps(std::uint32_t flags, std::uint32_t bytes);

// IDirectSound::DuplicateSoundBuffer: the primary buffer cannot be
// duplicated (DSERR_INVALIDCALL); a duplicate shares its source's samples.
std::uint32_t CheckDuplicate(bool source_is_primary);

// How IDirectSoundBuffer::Lock divides a buffer: from offset, wrapping to
// the start. DSBLOCK_ENTIREBUFFER takes the whole buffer from 0. An empty
// buffer, an offset past the end, or more bytes than the buffer holds is
// refused (false).
struct LockRegions
{
    std::uint32_t first_offset = 0;
    std::uint32_t first_bytes = 0;
    std::uint32_t second_bytes = 0;
};
bool PlanLock(std::uint32_t buffer_bytes,
              std::uint32_t offset,
              std::uint32_t bytes,
              std::uint32_t flags,
              LockRegions* regions);

}  // namespace re2dj::audio

#endif  // RE2DJ_AUDIO_DIRECTSOUND_DEVICE_H_
