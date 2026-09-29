#include "re2dj/graphics/true_color.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/graphics/color_depth.h"
#include "re2dj/target/target_profile.h"
#include "test_support.h"

namespace
{

namespace graphics = re2dj::graphics;

std::array<std::uint8_t, 2> Bytes(std::uint16_t pixel)
{
    return {static_cast<std::uint8_t>(pixel), static_cast<std::uint8_t>(pixel >> 8)};
}

// Widening and narrowing: every RGB565 value survives the round trip, the
// widened values are the ones the backend has always uploaded, and narrowing
// drops low bits as GDI does.
void CheckConversions(re2dj::test::Context& context)
{
    bool round_trips = true;
    for (std::uint32_t value = 0; value <= 0xFFFFU; ++value)
    {
        const auto pixel = static_cast<std::uint16_t>(value);
        round_trips &= graphics::NarrowToRgb565(graphics::WidenRgb565(pixel)) == pixel;
    }
    RE2DJ_CHECK(context, round_trips);
    RE2DJ_CHECK_EQ(context, graphics::WidenRgb565(0x0000), 0x000000U);
    RE2DJ_CHECK_EQ(context, graphics::WidenRgb565(0xFFFF), 0xFFFFFFU);
    // Red 16 of 31 is 16 * 255 / 31 = 131; green 1 of 63 is 4; blue 30 is 246.
    RE2DJ_CHECK_EQ(context, graphics::WidenRgb565(static_cast<std::uint16_t>((16 << 11) | (1 << 5) | 30)),
                   0x8304F6U);
    RE2DJ_CHECK_EQ(context, graphics::NarrowToRgb565(0x00FF0000U), 0xF800U);
    RE2DJ_CHECK_EQ(context, graphics::NarrowToRgb565(0x0007030BU), 0x0001U);
    // The unused high byte of a 32bpp DIB pixel does not matter.
    RE2DJ_CHECK_EQ(context, graphics::NarrowToRgb565(0xFF123456U), graphics::NarrowToRgb565(0x00123456U));
}

// Reconciling keeps a plane pixel that still narrows to the RGB565 pixel and
// widens the rest.
void CheckReconcile(re2dj::test::Context& context)
{
    std::array<std::uint32_t, 3> plane = {0x00808183U, 0x00808183U, 0x00FF0000U};
    const std::uint16_t same = graphics::NarrowToRgb565(0x00808183U);
    std::vector<std::uint8_t> rgb565;
    for (const std::uint16_t pixel : {same, static_cast<std::uint16_t>(0x001F), static_cast<std::uint16_t>(0xF800)})
    {
        const auto bytes = Bytes(pixel);
        rgb565.insert(rgb565.end(), bytes.begin(), bytes.end());
    }
    graphics::ReconcileTrueColorRow(plane.data(), rgb565.data(), 3);
    RE2DJ_CHECK_EQ(context, plane[0], 0x00808183U);
    RE2DJ_CHECK_EQ(context, plane[1], graphics::WidenRgb565(0x001F));
    RE2DJ_CHECK_EQ(context, plane[2], 0x00FF0000U);

    std::array<std::uint8_t, 6> narrowed{};
    graphics::NarrowTrueColorRow(plane.data(), narrowed.data(), 3);
    RE2DJ_CHECK(context, std::vector<std::uint8_t>(narrowed.begin(), narrowed.end()) == rgb565);
}

// A plane copy: keyed source pixels, decided on RGB565, are skipped; the
// rest take the source plane, or the widened RGB565 without one.
void CheckCopy(re2dj::test::Context& context)
{
    const std::array<std::uint16_t, 4> source_pixels = {0x0000, 0x1234, 0xF81F, 0xFFFF};
    const graphics::TrueColorPlane blank(4, 1);
    graphics::TrueColorPlane source_plane(4, 1);
    const std::array<std::uint32_t, 4> source_colors = {0x00000001U, 0x00112233U, 0x00FF07FFU, 0x00FEFEFEU};
    for (std::uint32_t x = 0; x < 4; ++x)
    {
        source_plane.Row(0)[x] = source_colors[x];
    }
    graphics::LegacyTextureView source;
    source.pixels = source_pixels.data();
    source.width = 4;
    source.height = 1;
    source.pitch = sizeof(source_pixels);
    graphics::Rgb565ColorKey key;
    key.enabled = true;
    key.low = 0xF81F;
    key.high = 0xF81F;

    graphics::TrueColorPlane destination(6, 2);
    std::fill_n(destination.Row(1), 6, 0x00ABCDEFU);
    const graphics::TrueColorView source_view = source_plane.View();
    RE2DJ_CHECK(context, graphics::CopyTrueColorRectangle(destination.View(), 1, 1, &source_view, source,
                                                          {0, 0, 4, 1}, key));
    RE2DJ_CHECK_EQ(context, destination.Row(1)[0], 0x00ABCDEFU);
    RE2DJ_CHECK_EQ(context, destination.Row(1)[1], 0x00000001U);
    RE2DJ_CHECK_EQ(context, destination.Row(1)[2], 0x00112233U);
    RE2DJ_CHECK_EQ(context, destination.Row(1)[3], 0x00ABCDEFU);
    RE2DJ_CHECK_EQ(context, destination.Row(1)[4], 0x00FEFEFEU);
    RE2DJ_CHECK_EQ(context, destination.Row(1)[5], 0x00ABCDEFU);

    key.enabled = false;
    RE2DJ_CHECK(context, graphics::CopyTrueColorRectangle(destination.View(), 0, 0, nullptr, source, {1, 0, 3, 1},
                                                          key));
    RE2DJ_CHECK_EQ(context, destination.Row(0)[0], graphics::WidenRgb565(0x1234));
    RE2DJ_CHECK_EQ(context, destination.Row(0)[2], 0x00FFFFFFU);

    // A rectangle that does not fit changes nothing.
    const std::uint32_t before = destination.Row(0)[5];
    RE2DJ_CHECK(context, !graphics::CopyTrueColorRectangle(destination.View(), 4, 0, nullptr, source, {0, 0, 4, 1},
                                                           key));
    RE2DJ_CHECK_EQ(context, destination.Row(0)[5], before);
    RE2DJ_CHECK(context, blank.Row(0)[0] == 0U);

    // Within one plane the source is read before anything is written.
    graphics::TrueColorPlane shifted(4, 1);
    for (std::uint32_t x = 0; x < 4; ++x)
    {
        shifted.Row(0)[x] = source_colors[x];
    }
    const graphics::TrueColorView shifted_view = shifted.View();
    RE2DJ_CHECK(context, graphics::CopyTrueColorRectangle(shifted.View(), 1, 0, &shifted_view, source, {0, 0, 3, 1},
                                                          key));
    RE2DJ_CHECK_EQ(context, shifted.Row(0)[1], source_colors[0]);
    RE2DJ_CHECK_EQ(context, shifted.Row(0)[2], source_colors[1]);
    RE2DJ_CHECK_EQ(context, shifted.Row(0)[3], source_colors[2]);
}

// Rectangles over host surfaces: widen, narrow, reconcile and fill touch only
// their area.
void CheckRectangles(re2dj::test::Context& context)
{
    std::vector<std::uint16_t> pixels = {0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006};
    graphics::Rgb565SurfaceView rgb565;
    rgb565.pixels = pixels.data();
    rgb565.width = 3;
    rgb565.height = 2;
    rgb565.pitch = 6;
    graphics::TrueColorPlane plane(3, 2);
    RE2DJ_CHECK(context, graphics::WidenRgb565Rectangle(rgb565, plane.View(), {1, 1, 2, 1}));
    RE2DJ_CHECK_EQ(context, plane.Row(0)[1], 0U);
    RE2DJ_CHECK_EQ(context, plane.Row(1)[1], graphics::WidenRgb565(0x0005));
    RE2DJ_CHECK(context, graphics::FillTrueColorRectangle(plane.View(), {0, 0, 2, 1}, 0xFF102030U));
    RE2DJ_CHECK_EQ(context, plane.Row(0)[1], 0x00102030U);
    RE2DJ_CHECK_EQ(context, plane.Row(0)[2], 0U);
    RE2DJ_CHECK(context, graphics::NarrowTrueColorRectangle(plane.View(), rgb565, {0, 0, 1, 1}));
    RE2DJ_CHECK_EQ(context, pixels[0], graphics::NarrowToRgb565(0x00102030U));
    RE2DJ_CHECK_EQ(context, pixels[1], 0x0002U);
    pixels[1] = 0x0842;
    RE2DJ_CHECK(context, graphics::ReconcileTrueColorRectangle(plane.View(), rgb565, {0, 0, 3, 1}));
    RE2DJ_CHECK_EQ(context, plane.Row(0)[0], 0x00102030U);
    RE2DJ_CHECK_EQ(context, plane.Row(0)[1], graphics::WidenRgb565(0x0842));
    RE2DJ_CHECK(context, !graphics::FillTrueColorRectangle(plane.View(), {2, 0, 2, 1}, 0));
}

// The option: its two spellings, the process switch, and the default no
// profile overrides.
void CheckColorDepthOption(re2dj::test::Context& context)
{
    graphics::ColorDepth depth = graphics::ColorDepth::k16;
    RE2DJ_CHECK(context, graphics::ParseColorDepthName("32", &depth) && depth == graphics::ColorDepth::k32);
    RE2DJ_CHECK(context, graphics::ParseColorDepthName("16", &depth) && depth == graphics::ColorDepth::k16);
    RE2DJ_CHECK(context, !graphics::ParseColorDepthName("24", &depth));
    RE2DJ_CHECK(context, !graphics::ParseColorDepthName("", &depth));
    RE2DJ_CHECK(context, std::string(graphics::ColorDepthName(graphics::ColorDepth::k32)) == "32");
    RE2DJ_CHECK(context, std::string(graphics::ColorDepthName(graphics::ColorDepth::k16)) == "16");

    RE2DJ_CHECK(context, !graphics::TrueColorSelected());
    graphics::SelectColorDepth(graphics::ColorDepth::k32);
    RE2DJ_CHECK(context, graphics::TrueColorSelected());
    graphics::SelectColorDepth(graphics::ColorDepth::k16);
    RE2DJ_CHECK(context, graphics::SelectedColorDepth() == graphics::ColorDepth::k16);

    bool every_default_16 = true;
    for (const re2dj::target::BuiltInTargetProfile& profile : re2dj::target::GetBuiltInTargetProfiles())
    {
        every_default_16 &= profile.profile.run_defaults.color_depth == graphics::ColorDepth::k16;
    }
    RE2DJ_CHECK(context, every_default_16);
}

}  // namespace

void RunTrueColorTests(re2dj::test::Context& context)
{
    CheckConversions(context);
    CheckReconcile(context);
    CheckCopy(context);
    CheckRectangles(context);
    CheckColorDepthOption(context);
}
