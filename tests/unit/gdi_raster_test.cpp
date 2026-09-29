#include "re2dj/hle/gdi_raster.h"

#include <array>
#include <cstdint>

#include "test_support.h"

namespace
{

namespace hle = re2dj::hle;

// The conversions Windows 11 GDI made into an RGB565 DIB section: 5-5-5 and
// 24-bit sources, as measured.
void CheckConversions(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiChannel(0x1F, 5, 6), 0x3FU);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiChannel(16, 5, 6), 33U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiChannel(132, 8, 5), 16U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x7FFFU, hle::kGdiRgb555, hle::kGdiRgb565), 0xFFFFU);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x03E0U, hle::kGdiRgb555, hle::kGdiRgb565), 0x07E0U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x0421U, hle::kGdiRgb555, hle::kGdiRgb565), 0x0841U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x4210U, hle::kGdiRgb555, hle::kGdiRgb565), 0x8430U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x3DEFU, hle::kGdiRgb555, hle::kGdiRgb565), 0x7BCFU);
    // 24-bit pixels are B, G, R in memory: red 0xFF0000 as read.
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0xFF0000U, hle::kGdiBgr888, hle::kGdiRgb565), 0xF800U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x848484U, hle::kGdiBgr888, hle::kGdiRgb565), 0x8430U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x010307U, hle::kGdiBgr888, hle::kGdiRgb565), 0U);
}

// A true-color plane's pixels: 24-bit sources and COLORREFs as given, 5-5-5
// widened by repeating high bits; narrowing any of them to 5-6-5 gives what
// GDI writes into the RGB565 pixels.
void CheckTrueColorConversions(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x7F8081U, hle::kGdiBgr888, hle::kGdiXrgb8888), 0x7F8081U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x00563412U, hle::kGdiColorref, hle::kGdiXrgb8888), 0x123456U);
    RE2DJ_CHECK_EQ(context, hle::ConvertGdiPixel(0x4210U, hle::kGdiRgb555, hle::kGdiXrgb8888), 0x848484U);
    bool narrows_alike = true;
    for (std::uint32_t pixel = 0; pixel < 0x8000U; ++pixel)
    {
        const std::uint32_t plane = hle::ConvertGdiPixel(pixel, hle::kGdiRgb555, hle::kGdiXrgb8888);
        narrows_alike &= hle::ConvertGdiPixel(plane, hle::kGdiXrgb8888, hle::kGdiRgb565) ==
                         hle::ConvertGdiPixel(pixel, hle::kGdiRgb555, hle::kGdiRgb565);
    }
    RE2DJ_CHECK(context, narrows_alike);
}

void CheckLayoutHelpers(re2dj::test::Context& context)
{
    std::array<std::uint8_t, 4> bytes{};
    hle::WriteGdiPixel(bytes, 24, 0x00ABCDEFU);
    RE2DJ_CHECK_EQ(context, bytes[0], 0xEF);
    RE2DJ_CHECK_EQ(context, hle::ReadGdiPixel(bytes, 24), 0x00ABCDEFU);
    RE2DJ_CHECK_EQ(context, hle::DibRowBytes(4, 24), 12U);
    RE2DJ_CHECK_EQ(context, hle::DibRowBytes(3, 16), 8U);
    // Stretching 2 to 4 repeats each source pixel.
    RE2DJ_CHECK_EQ(context, hle::StretchSourceIndex(1, 4, 2), 0U);
    RE2DJ_CHECK_EQ(context, hle::StretchSourceIndex(2, 4, 2), 1U);
    RE2DJ_CHECK_EQ(context, hle::StretchSourceIndex(3, 4, 2), 1U);
}

// FillRect's area: ordered, then clipped to the bitmap.
void CheckFillArea(re2dj::test::Context& context)
{
    using re2dj::hle::GdiRect;
    const GdiRect inverted = re2dj::hle::FillArea(GdiRect{3, 3, 1, 1}, 8, 4);
    RE2DJ_CHECK(context, inverted.left == 1 && inverted.top == 1 && inverted.right == 3 && inverted.bottom == 3);
    const GdiRect clipped = re2dj::hle::FillArea(GdiRect{-2, -1, 20, 20}, 8, 4);
    RE2DJ_CHECK(context, clipped.left == 0 && clipped.top == 0 && clipped.right == 8 && clipped.bottom == 4);
    RE2DJ_CHECK(context, re2dj::hle::FillArea(GdiRect{2, 1, 2, 3}, 8, 4).empty());
    RE2DJ_CHECK(context, re2dj::hle::FillArea(GdiRect{9, 0, 12, 2}, 8, 4).empty());
    // COLORREFs narrow as measured: 0x848484 is 0x8430 in 5-6-5.
    RE2DJ_CHECK_EQ(context,
                   re2dj::hle::ConvertGdiPixel(0x00848484U, re2dj::hle::kGdiColorref, re2dj::hle::kGdiRgb565),
                   0x8430U);
    RE2DJ_CHECK_EQ(context,
                   re2dj::hle::ConvertGdiPixel(0x000000FFU, re2dj::hle::kGdiColorref, re2dj::hle::kGdiRgb565),
                   0xF800U);
}

}  // namespace

void RunGdiRasterTests(re2dj::test::Context& context)
{
    CheckConversions(context);
    CheckTrueColorConversions(context);
    CheckLayoutHelpers(context);
    CheckFillArea(context);
}
