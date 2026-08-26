#ifndef RE2DJ_AUDIO_LEGACY_AUDIO_BUFFER_H_
#define RE2DJ_AUDIO_LEGACY_AUDIO_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace re2dj::audio
{
struct LegacyAudioFormat { std::uint16_t channels = 0; std::uint32_t sample_rate = 0; std::uint16_t bits_per_sample = 0; std::uint16_t block_align = 0; };
struct LegacyAudioLock { std::span<std::byte> first; std::span<std::byte> second; };

class LegacyAudioBuffer
{
public:
    LegacyAudioBuffer(LegacyAudioFormat format, std::size_t byte_count);
    LegacyAudioBuffer Duplicate() const;
    const LegacyAudioFormat& format() const;
    std::span<const std::byte> samples() const;
    std::size_t byte_count() const;
    bool Lock(std::size_t offset, std::size_t byte_count, bool entire_buffer, LegacyAudioLock* lock);
    bool ValidateUnlock(const LegacyAudioLock& lock) const;
    void set_current_position(std::uint32_t position);
    std::uint32_t current_position() const;
    void set_volume(std::int32_t volume);
    std::int32_t volume() const;
    void set_pan(std::int32_t pan);
    std::int32_t pan() const;
    void set_frequency(std::uint32_t frequency);
    std::uint32_t frequency() const;
    void set_playing(bool playing, bool looping);
    bool playing() const;
    bool looping() const;

private:
    LegacyAudioBuffer(LegacyAudioFormat format,
                      std::shared_ptr<std::vector<std::byte>> samples);
    LegacyAudioFormat format_;
    std::shared_ptr<std::vector<std::byte>> samples_;
    std::uint32_t current_position_ = 0;
    std::int32_t volume_ = 0;
    std::int32_t pan_ = 0;
    std::uint32_t frequency_ = 0;
    bool playing_ = false;
    bool looping_ = false;
};
}  // namespace re2dj::audio

#endif  // RE2DJ_AUDIO_LEGACY_AUDIO_BUFFER_H_
