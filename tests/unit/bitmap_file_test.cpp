#include "re2dj/hle/bitmap_file.h"

#include <cstdint>
#include <string>
#include <vector>

#include "bitmap_file_bytes.h"
#include "test_support.h"

void RunBitmapFileTests(re2dj::test::Context& context)
{
    using re2dj::hle::BitmapFileImage;
    using re2dj::hle::BitmapFileOutcome;
    using re2dj::hle::ParseBitmapFile;

    // 24 bits, 3x2 bottom-up: the rows stay bottom-up, and the file's padding
    // bytes (AA BB CC) become zero, as Windows 11 loads them.
    {
        BitmapFileImage image;
        std::string reason;
        RE2DJ_CHECK(context, ParseBitmapFile(re2dj::test::Bitmap24File(), &image, &reason) ==
                                 BitmapFileOutcome::kRead);
        RE2DJ_CHECK_EQ(context, image.width, 3U);
        RE2DJ_CHECK_EQ(context, image.height, 2U);
        RE2DJ_CHECK_EQ(context, image.bits_per_pixel, 24U);
        RE2DJ_CHECK_EQ(context, image.row_bytes, 12U);
        RE2DJ_CHECK(context, image.color_table.empty());
        const std::vector<std::uint8_t> rows = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0, 0, 0,
                                                0x1E, 0x14, 0x0A, 0x3C, 0x32, 0x28, 0x5A, 0x50, 0x46, 0, 0, 0};
        RE2DJ_CHECK(context, image.rows == rows);
    }

    // 8 bits, 2x2, biClrUsed 0: a 256-entry table as 0x00RRGGBB.
    {
        BitmapFileImage image;
        std::string reason;
        RE2DJ_CHECK(context, ParseBitmapFile(re2dj::test::Bitmap8File(), &image, &reason) ==
                                 BitmapFileOutcome::kRead);
        RE2DJ_CHECK_EQ(context, image.bits_per_pixel, 8U);
        RE2DJ_CHECK_EQ(context, image.row_bytes, 4U);
        RE2DJ_CHECK_EQ(context, image.color_table.size(), std::size_t{256});
        RE2DJ_CHECK_EQ(context, image.color_table[1], 0x0001FE03U);
        RE2DJ_CHECK_EQ(context, image.color_table[200], 0x00C83758U);
        const std::vector<std::uint8_t> rows = {1, 2, 0, 0, 200, 255, 0, 0};
        RE2DJ_CHECK(context, image.rows == rows);
    }

    // A biClrUsed table is as long as it says.
    {
        std::vector<std::uint8_t> bytes = re2dj::test::Bitmap8File(4);
        BitmapFileImage image;
        std::string reason;
        RE2DJ_CHECK(context, ParseBitmapFile(bytes, &image, &reason) == BitmapFileOutcome::kRead);
        RE2DJ_CHECK_EQ(context, image.color_table.size(), std::size_t{4});
    }

    // Anything not starting with "BM" is no bitmap.
    {
        const std::string text = "XX not a bitmap at all, just some bytes to fill the header area.......";
        const std::vector<std::uint8_t> bytes(text.begin(), text.end());
        BitmapFileImage image;
        std::string reason;
        RE2DJ_CHECK(context, ParseBitmapFile(bytes, &image, &reason) == BitmapFileOutcome::kNotBitmap);
        RE2DJ_CHECK(context, ParseBitmapFile({}, &image, &reason) == BitmapFileOutcome::kNotBitmap);
    }

    // Formats outside the assets' are not modelled: top-down, 16 bits,
    // compressed, and a file cut short.
    {
        BitmapFileImage image;
        std::string reason;
        std::vector<std::uint8_t> top_down = re2dj::test::Bitmap24File();
        re2dj::test::PutLe32(top_down, 22, static_cast<std::uint32_t>(-2));
        RE2DJ_CHECK(context, ParseBitmapFile(top_down, &image, &reason) == BitmapFileOutcome::kNotModelled);
        RE2DJ_CHECK(context, !reason.empty());
        std::vector<std::uint8_t> sixteen = re2dj::test::Bitmap24File();
        sixteen[28] = 16;
        RE2DJ_CHECK(context, ParseBitmapFile(sixteen, &image, &reason) == BitmapFileOutcome::kNotModelled);
        std::vector<std::uint8_t> compressed = re2dj::test::Bitmap24File();
        compressed[30] = 1;
        RE2DJ_CHECK(context, ParseBitmapFile(compressed, &image, &reason) == BitmapFileOutcome::kNotModelled);
        std::vector<std::uint8_t> cut = re2dj::test::Bitmap24File();
        cut.resize(cut.size() - 1);
        RE2DJ_CHECK(context, ParseBitmapFile(cut, &image, &reason) == BitmapFileOutcome::kNotModelled);
    }
}
