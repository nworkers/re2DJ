#include "re2dj/audio/legacy_audio_buffer.h"

#include "test_support.h"

void RunLegacyAudioBufferTests(re2dj::test::Context& context)
{
    re2dj::audio::LegacyAudioBuffer buffer({2, 44100, 16, 4}, 16);
    re2dj::audio::LegacyAudioLock lock;
    RE2DJ_CHECK(context, buffer.Lock(12, 8, false, &lock));
    RE2DJ_CHECK_EQ(context, lock.first.size(), std::size_t{4});
    RE2DJ_CHECK_EQ(context, lock.second.size(), std::size_t{4});
    RE2DJ_CHECK(context, buffer.ValidateUnlock(lock));
    RE2DJ_CHECK(context, buffer.Lock(9, 1, true, &lock));
    RE2DJ_CHECK_EQ(context, lock.first.size(), std::size_t{16});
    RE2DJ_CHECK_EQ(context, lock.second.size(), std::size_t{0});
    buffer.set_current_position(18);
    buffer.set_volume(-12000);
    buffer.set_pan(12000);
    RE2DJ_CHECK_EQ(context, buffer.current_position(), std::uint32_t{2});
    RE2DJ_CHECK_EQ(context, buffer.volume(), std::int32_t{-10000});
    RE2DJ_CHECK_EQ(context, buffer.pan(), std::int32_t{10000});

    buffer.set_volume(-1200);
    buffer.set_pan(3000);
    buffer.set_frequency(22050);
    buffer.set_playing(true, true);
    re2dj::audio::LegacyAudioBuffer duplicate = buffer.Duplicate();
    RE2DJ_CHECK_EQ(context, duplicate.current_position(), buffer.current_position());
    RE2DJ_CHECK_EQ(context, duplicate.volume(), buffer.volume());
    RE2DJ_CHECK_EQ(context, duplicate.pan(), buffer.pan());
    RE2DJ_CHECK_EQ(context, duplicate.frequency(), buffer.frequency());
    RE2DJ_CHECK(context, !duplicate.playing());
    RE2DJ_CHECK(context, !duplicate.looping());
    re2dj::audio::LegacyAudioLock duplicate_lock;
    RE2DJ_CHECK(context, duplicate.Lock(0, 1, false, &duplicate_lock));
    duplicate_lock.first[0] = std::byte{0x5a};
    RE2DJ_CHECK_EQ(context, buffer.samples()[0], std::byte{0x5a});
    duplicate.set_current_position(7);
    duplicate.set_volume(-2400);
    duplicate.set_playing(true, false);
    RE2DJ_CHECK_EQ(context, buffer.current_position(), std::uint32_t{2});
    RE2DJ_CHECK_EQ(context, buffer.volume(), std::int32_t{-1200});
    RE2DJ_CHECK(context, buffer.looping());
}
