#include "test_support.h"

#include <cstdint>

#include "re2dj/audio/directsound_buffer_policy.h"

void RunDirectSoundBufferPolicyTests(re2dj::test::Context& context)
{
    using re2dj::audio::IsStreamingBufferDescription;

    // Buffer descriptions observed in each build's audio trace (task 303).
    // Rings: 1st Tracks and 1st SE carry LOCHARDWARE and STATIC; every later
    // build carries only GETCURRENTPOSITION2. All are 360,448 bytes.
    RE2DJ_CHECK(context, IsStreamingBufferDescription(0x000140c6, 360448));
    RE2DJ_CHECK(context, IsStreamingBufferDescription(0x000140c0, 360448));

    // One-shot effects. 1st Tracks (0x140c2) and 1st SE (0x140e2) put
    // GETCURRENTPOSITION2 on them alongside STATIC, which is what used to send
    // them down the streaming path and make them repeat endlessly.
    RE2DJ_CHECK(context, !IsStreamingBufferDescription(0x000140c2, 35520));
    RE2DJ_CHECK(context, !IsStreamingBufferDescription(0x000140e2, 98812));
    // 2nd through 5th and EZ2Dancer 2nd MOVE effects carry neither flag.
    RE2DJ_CHECK(context, !IsStreamingBufferDescription(0x000040e0, 50300));
    RE2DJ_CHECK(context, !IsStreamingBufferDescription(0x000040c0, 50300));

    // The ring size decides on its own, whatever the flags say.
    RE2DJ_CHECK(context, IsStreamingBufferDescription(0x00000000, 360448));
    // Without STATIC, either position flag still marks a stream.
    RE2DJ_CHECK(context, IsStreamingBufferDescription(0x00000004, 88200));
    RE2DJ_CHECK(context, IsStreamingBufferDescription(0x00010000, 88200));
    RE2DJ_CHECK(context, !IsStreamingBufferDescription(0x00000000, 88200));
}
