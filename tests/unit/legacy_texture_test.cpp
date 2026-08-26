#include "re2dj/graphics/legacy_texture.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "test_support.h"

void RunLegacyTextureTests(re2dj::test::Context& context)
{
    re2dj::graphics::Rgb565ColorKey key;
    key.enabled = true;
    key.low = 0x001f;
    key.high = 0x003f;

    RE2DJ_CHECK(context, re2dj::graphics::IsRgb565ColorKeyMatch(0x001f, key));
    RE2DJ_CHECK(context, re2dj::graphics::IsRgb565ColorKeyMatch(0x0020, key));
    RE2DJ_CHECK(context, re2dj::graphics::IsRgb565ColorKeyMatch(0x003f, key));
    RE2DJ_CHECK(context, !re2dj::graphics::IsRgb565ColorKeyMatch(0x001e, key));
    RE2DJ_CHECK(context, !re2dj::graphics::IsRgb565ColorKeyMatch(0x0040, key));

    key.enabled = false;
    RE2DJ_CHECK(context, !re2dj::graphics::IsRgb565ColorKeyMatch(0x0020, key));

    key.enabled = true;
    key.low = 2;
    key.high = 1;
    RE2DJ_CHECK(context, !re2dj::graphics::IsRgb565ColorKeyMatch(1, key));

    std::vector<std::uint16_t> source_pixels = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
    };
    std::vector<std::uint16_t> destination_pixels(20, 0xffff);
    re2dj::graphics::LegacyTextureView source;
    source.pixels = source_pixels.data();
    source.width = 4;
    source.height = 3;
    source.pitch = 4 * sizeof(std::uint16_t);
    re2dj::graphics::Rgb565SurfaceView destination;
    destination.pixels = destination_pixels.data();
    destination.width = 5;
    destination.height = 4;
    destination.pitch = 5 * sizeof(std::uint16_t);
    const re2dj::graphics::Rgb565Rectangle rectangle = {1, 1, 2, 2};
    RE2DJ_CHECK(context,
                re2dj::graphics::CopyRgb565Rectangle(destination,
                                                      2,
                                                      1,
                                                      source,
                                                      rectangle,
                                                      {}));
    RE2DJ_CHECK_EQ(context, destination_pixels[7], std::uint16_t{6});
    RE2DJ_CHECK_EQ(context, destination_pixels[8], std::uint16_t{7});
    RE2DJ_CHECK_EQ(context, destination_pixels[12], std::uint16_t{10});
    RE2DJ_CHECK_EQ(context, destination_pixels[13], std::uint16_t{11});

    std::fill(destination_pixels.begin(), destination_pixels.end(), std::uint16_t{99});
    key.enabled = true;
    key.low = 7;
    key.high = 10;
    RE2DJ_CHECK(context,
                re2dj::graphics::CopyRgb565Rectangle(destination,
                                                      2,
                                                      1,
                                                      source,
                                                      rectangle,
                                                      key));
    RE2DJ_CHECK_EQ(context, destination_pixels[7], std::uint16_t{6});
    RE2DJ_CHECK_EQ(context, destination_pixels[8], std::uint16_t{99});
    RE2DJ_CHECK_EQ(context, destination_pixels[12], std::uint16_t{99});
    RE2DJ_CHECK_EQ(context, destination_pixels[13], std::uint16_t{11});

    const re2dj::graphics::Rgb565Rectangle invalid = {3, 2, 2, 2};
    RE2DJ_CHECK(context,
                !re2dj::graphics::CopyRgb565Rectangle(destination,
                                                       0,
                                                       0,
                                                       source,
                                                       invalid,
                                                       {}));
}
