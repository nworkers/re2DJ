#include "gdi_text.h"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include "facade_com.h"
#include "re2dj/hle/guest_font.h"
#include "re2dj/hle/guest_gdi.h"
#include "re2dj/hle/win32_errors.h"

namespace re2dj::hle::modules
{
namespace
{

constexpr std::uint32_t kDtCenter = 0x01;
constexpr std::uint32_t kDtRight = 0x02;
constexpr std::uint32_t kDtVCenter = 0x04;
constexpr std::uint32_t kDtBottom = 0x08;
constexpr std::uint32_t kDtNoClip = 0x100;
constexpr std::uint32_t kOpaque = 2;

bool Done(std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// The pixel layout GDI writes a bitmap of this depth in: 16 bits by the
// bitmap's masks (RGB555 without them), 24 bits as BGR bytes.
bool BitmapLayout(const GuestBitmap& bitmap, GdiPixelLayout* layout)
{
    if (bitmap.bits_per_pixel == 24)
    {
        *layout = kGdiBgr888;
        return true;
    }
    if (bitmap.bits_per_pixel != 16)
    {
        return false;
    }
    const bool has_masks = bitmap.masks[0] != 0 || bitmap.masks[1] != 0 || bitmap.masks[2] != 0;
    *layout = has_masks ? GdiPixelLayout{16, bitmap.masks} : kGdiRgb555;
    return true;
}

}  // namespace

std::int32_t HalfDown(std::int32_t value)
{
    return value >= 0 ? value / 2 : -((-value + 1) / 2);
}

bool DrawTextPixels(const ImportCall& call,
                    GuestProcess& process,
                    std::uint32_t dc_handle,
                    std::string_view text,
                    const GdiRect& rect,
                    std::uint32_t format,
                    std::string* error)
{
    const GuestDc* dc = process.gdi().FindDc(dc_handle);
    const GuestBitmap* bitmap = dc == nullptr ? nullptr : process.gdi().FindBitmap(dc->bitmap);
    GdiPixelLayout layout;
    if (bitmap == nullptr || !BitmapLayout(*bitmap, &layout))
    {
        return com::Fail(error, com::CallName(call) + " has no model of this DC's bitmap");
    }
    const bool opaque = dc->background_mode == kOpaque;
    if ((dc->text_color >> 24) != 0 || (opaque && (dc->background_color >> 24) != 0))
    {
        return com::Fail(error, com::CallName(call) + " with a palette color is not modelled");
    }
    const auto text_width = static_cast<std::int32_t>(text.size()) * kGuestFontCharWidth;
    std::int32_t x = rect.left;
    if ((format & kDtCenter) != 0)
    {
        x = rect.left + HalfDown(rect.right - rect.left - text_width);
    }
    else if ((format & kDtRight) != 0)
    {
        x = rect.right - text_width;
    }
    std::int32_t y = rect.top;
    if ((format & kDtVCenter) != 0)
    {
        y = rect.top + HalfDown(rect.bottom - rect.top - kGuestFontHeight);
    }
    else if ((format & kDtBottom) != 0)
    {
        y = rect.bottom - kGuestFontHeight;
    }
    GdiRect clip = FillArea((format & kDtNoClip) != 0 ? GdiRect{0, 0, static_cast<std::int32_t>(bitmap->width),
                                                                  static_cast<std::int32_t>(bitmap->height)}
                                                         : rect,
                            bitmap->width, bitmap->height);
    clip.left = std::max(clip.left, x);
    clip.top = std::max(clip.top, y);
    clip.right = std::min(clip.right, x + text_width);
    clip.bottom = std::min(clip.bottom, y + kGuestFontHeight);
    if (clip.empty())
    {
        return true;
    }
    const std::uint32_t bytes_per_pixel = layout.bits_per_pixel / 8;
    const std::uint32_t text_pixel = ConvertGdiPixel(dc->text_color, kGdiColorref, layout);
    const std::uint32_t background_pixel = ConvertGdiPixel(dc->background_color, kGdiColorref, layout);
    // A surface's true-color plane takes both colours at 24 bits.
    const std::uint32_t text_true_color = ConvertGdiPixel(dc->text_color, kGdiColorref, kGdiXrgb8888);
    const std::uint32_t background_true_color = ConvertGdiPixel(dc->background_color, kGdiColorref, kGdiXrgb8888);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(clip.right - clip.left) * bytes_per_pixel);
    for (std::int32_t py = clip.top; py < clip.bottom; ++py)
    {
        std::uint32_t* const plane_row =
            bitmap->true_color == nullptr ? nullptr : bitmap->true_color->Row(static_cast<std::uint32_t>(py));
        const std::uint32_t line =
            bitmap->top_down ? static_cast<std::uint32_t>(py) : bitmap->height - 1 - static_cast<std::uint32_t>(py);
        const std::uint32_t address =
            bitmap->bits + line * bitmap->pitch + static_cast<std::uint32_t>(clip.left) * bytes_per_pixel;
        if (!com::ReadBytes(call, address, row, error))
        {
            return false;
        }
        for (std::int32_t px = clip.left; px < clip.right; ++px)
        {
            const std::int32_t column = px - x;
            const auto* glyph = GuestFontGlyph(static_cast<unsigned char>(text[static_cast<std::size_t>(column / kGuestFontCharWidth)]));
            const bool set = ((*glyph)[static_cast<std::size_t>(py - y)] & (0x80U >> (column % kGuestFontCharWidth))) != 0;
            const auto span =
                std::span<std::uint8_t>(row).subspan(static_cast<std::size_t>(px - clip.left) * bytes_per_pixel);
            if (set)
            {
                WriteGdiPixel(span, layout.bits_per_pixel, text_pixel);
                if (plane_row != nullptr)
                {
                    plane_row[px] = text_true_color;
                }
            }
            else if (opaque)
            {
                WriteGdiPixel(span, layout.bits_per_pixel, background_pixel);
                if (plane_row != nullptr)
                {
                    plane_row[px] = background_true_color;
                }
            }
        }
        if (!com::WriteBytes(call, address, row, error))
        {
            return false;
        }
    }
    return true;
}

bool ExtTextOutA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 8)
    {
        return com::Fail(error, com::CallName(call) + (result == nullptr ? " result is null" : " argument shape is invalid"));
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return com::Fail(error, com::CallName(call) + " needs the guest process");
    }
    if (call.arguments[3] != 0 || call.arguments[4] != 0 || call.arguments[7] != 0)
    {
        return com::Fail(error, com::CallName(call) + " with options, a rectangle, or character spacing is not modelled");
    }
    if (process->gdi().FindDc(call.arguments[0]) == nullptr)
    {
        return Done(error);
    }
    const std::uint32_t count = call.arguments[6];
    if (count == 0)
    {
        result->eax = 1;
        return Done(error);
    }
    if (call.arguments[5] == 0)
    {
        return com::Fail(error, com::CallName(call) + " of a null string with a count is not modelled");
    }
    std::vector<std::uint8_t> bytes(count);
    if (!com::ReadBytes(call, call.arguments[5], bytes, error))
    {
        return false;
    }
    const std::string text(bytes.begin(), bytes.end());
    for (const char character : text)
    {
        if (GuestFontGlyph(static_cast<unsigned char>(character)) == nullptr)
        {
            return com::Fail(error, com::CallName(call) + " of a character outside printable ASCII is not modelled");
        }
    }
    const auto x = static_cast<std::int32_t>(call.arguments[1]);
    const auto y = static_cast<std::int32_t>(call.arguments[2]);
    if (!DrawTextPixels(call, *process, call.arguments[0], text, GdiRect{x, y, x, y}, kDtNoClip, error))
    {
        return false;
    }
    result->eax = 1;
    call.services->SetLastError(kWin32ErrorSuccess);
    return Done(error);
}

}  // namespace re2dj::hle::modules
