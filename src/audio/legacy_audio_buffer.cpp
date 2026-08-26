#include "re2dj/audio/legacy_audio_buffer.h"

#include <algorithm>
#include <utility>

namespace re2dj::audio
{
LegacyAudioBuffer::LegacyAudioBuffer(LegacyAudioFormat format, std::size_t byte_count)
    : LegacyAudioBuffer(format, std::make_shared<std::vector<std::byte>>(byte_count)) {}
LegacyAudioBuffer::LegacyAudioBuffer(LegacyAudioFormat format,
                                     std::shared_ptr<std::vector<std::byte>> samples)
    : format_(format), samples_(std::move(samples)), frequency_(format.sample_rate) {}
LegacyAudioBuffer LegacyAudioBuffer::Duplicate() const
{
    LegacyAudioBuffer duplicate(format_, samples_);
    duplicate.current_position_ = current_position_;
    duplicate.volume_ = volume_;
    duplicate.pan_ = pan_;
    duplicate.frequency_ = frequency_;
    return duplicate;
}
const LegacyAudioFormat& LegacyAudioBuffer::format() const { return format_; }
std::span<const std::byte> LegacyAudioBuffer::samples() const { return *samples_; }
std::size_t LegacyAudioBuffer::byte_count() const { return samples_->size(); }
bool LegacyAudioBuffer::Lock(std::size_t offset, std::size_t byte_count, bool entire_buffer, LegacyAudioLock* lock)
{
    if (lock == nullptr || samples_->empty()) return false;
    if (entire_buffer) { offset = 0; byte_count = samples_->size(); }
    if (offset >= samples_->size() || byte_count > samples_->size()) return false;
    const std::size_t first_size = std::min(byte_count, samples_->size() - offset);
    lock->first = std::span<std::byte>(*samples_).subspan(offset, first_size);
    lock->second = std::span<std::byte>(*samples_).first(byte_count - first_size);
    return true;
}
bool LegacyAudioBuffer::ValidateUnlock(const LegacyAudioLock& lock) const
{
    const auto* begin = samples_->data();
    const auto* end = begin + samples_->size();
    const auto valid = [begin, end](std::span<std::byte> region) {
        return region.empty() || (region.data() >= begin && region.data() + region.size() <= end);
    };
    return valid(lock.first) && valid(lock.second) && lock.first.size() + lock.second.size() <= samples_->size();
}
void LegacyAudioBuffer::set_current_position(std::uint32_t position) { current_position_ = samples_->empty() ? 0 : position % static_cast<std::uint32_t>(samples_->size()); }
std::uint32_t LegacyAudioBuffer::current_position() const { return current_position_; }
void LegacyAudioBuffer::set_volume(std::int32_t volume) { volume_ = std::clamp(volume, -10000, 0); }
std::int32_t LegacyAudioBuffer::volume() const { return volume_; }
void LegacyAudioBuffer::set_pan(std::int32_t pan) { pan_ = std::clamp(pan, -10000, 10000); }
std::int32_t LegacyAudioBuffer::pan() const { return pan_; }
void LegacyAudioBuffer::set_frequency(std::uint32_t frequency) { frequency_ = frequency; }
std::uint32_t LegacyAudioBuffer::frequency() const { return frequency_; }
void LegacyAudioBuffer::set_playing(bool playing, bool looping) { playing_ = playing; looping_ = playing && looping; }
bool LegacyAudioBuffer::playing() const { return playing_; }
bool LegacyAudioBuffer::looping() const { return looping_; }
}  // namespace re2dj::audio
