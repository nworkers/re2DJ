#ifndef RE2DJ_AUDIO_DIRECTSOUND_ABI_H_
#define RE2DJ_AUDIO_DIRECTSOUND_ABI_H_

#include <array>
#include <cstdint>

// DirectSound structures and constants as the 32-bit guest sees them, with
// fixed-width members so the layouts are the same on either host width. The
// Windows adapter checks them against the SDK headers at compile time.
namespace re2dj::audio
{

// HRESULTs (dsound.h, winerror.h).
inline constexpr std::uint32_t kDsOk = 0;
inline constexpr std::uint32_t kDsErrInvalidParam = 0x80070057U;
inline constexpr std::uint32_t kDsErrNoAggregation = 0x80040110U;
inline constexpr std::uint32_t kDsErrOutOfMemory = 0x8007000EU;
inline constexpr std::uint32_t kDsErrGeneric = 0x80004005U;
inline constexpr std::uint32_t kDsErrInvalidCall = 0x88780032U;
inline constexpr std::uint32_t kDsErrBadFormat = 0x88780064U;
inline constexpr std::uint32_t kDsErrNoDriver = 0x88780078U;
inline constexpr std::uint32_t kDsErrAlreadyInitialized = 0x88780082U;

inline constexpr std::uint16_t kWaveFormatPcm = 1;
inline constexpr std::uint32_t kDsbcapsPrimaryBuffer = 0x00000001U;
inline constexpr std::uint32_t kDscapsPrimaryStereo = 0x00000002U;
inline constexpr std::uint32_t kDscaps16Bit = 0x00000008U;
inline constexpr std::uint32_t kDsSpeakerStereo = 0x00000004U;
inline constexpr std::uint32_t kDsbLockEntireBuffer = 0x00000002U;
inline constexpr std::uint32_t kDsbPlayLooping = 0x00000001U;
inline constexpr std::uint32_t kDsbStatusPlaying = 0x00000001U;
inline constexpr std::uint32_t kDsbStatusLooping = 0x00000004U;
inline constexpr std::uint32_t kDsbFrequencyOriginal = 0;
inline constexpr std::int32_t kDsbVolumeMin = -10000;
inline constexpr std::int32_t kDsbVolumeMax = 0;
inline constexpr std::int32_t kDsbPanLeft = -10000;
inline constexpr std::int32_t kDsbPanRight = 10000;

// WAVEFORMATEX (18 bytes, byte-packed as in mmeapi.h).
#pragma pack(push, 1)
struct WaveFormatEx
{
    std::uint16_t format_tag = 0;
    std::uint16_t channels = 0;
    std::uint32_t samples_per_second = 0;
    std::uint32_t average_bytes_per_second = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint16_t extra_size = 0;
};
#pragma pack(pop)
static_assert(sizeof(WaveFormatEx) == 18);

// DSBUFFERDESC (36 bytes); DirectX 3's DSBUFFERDESC1 is its first 20.
// format is the guest address of a WAVEFORMATEX.
struct DsBufferDesc
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t buffer_bytes = 0;
    std::uint32_t reserved = 0;
    std::uint32_t format = 0;
    std::array<std::uint8_t, 16> algorithm_3d{};
};
static_assert(sizeof(DsBufferDesc) == 36);
inline constexpr std::uint32_t kDsBufferDesc1Size = 20;

// DSCAPS (96 bytes). Only the members this facade reports are named.
struct DsCaps
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::array<std::uint32_t, 22> unreported{};
};
static_assert(sizeof(DsCaps) == 96);

// DSBCAPS (20 bytes).
struct DsbCaps
{
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t buffer_bytes = 0;
    std::uint32_t unlock_transfer_rate = 0;
    std::uint32_t play_cpu_overhead = 0;
};
static_assert(sizeof(DsbCaps) == 20);

// Interface identifiers (dsound.h).
inline constexpr std::array<std::uint8_t, 16> kIidDirectSound = {0x83, 0xFA, 0x9A, 0x27, 0x81, 0x49, 0xCE, 0x11,
                                                                 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60};
inline constexpr std::array<std::uint8_t, 16> kIidDirectSoundBuffer = {0x85, 0xFA, 0x9A, 0x27, 0x81, 0x49,
                                                                       0xCE, 0x11, 0xA5, 0x21, 0x00, 0x20,
                                                                       0xAF, 0x0B, 0xE5, 0x60};

}  // namespace re2dj::audio

#endif  // RE2DJ_AUDIO_DIRECTSOUND_ABI_H_
