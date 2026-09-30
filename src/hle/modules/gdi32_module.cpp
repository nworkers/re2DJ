#include "re2dj/hle/modules/gdi32_module.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "facade_com.h"
#include "gdi_bitmaps.h"
#include "re2dj/hle/gdi_raster.h"
#include "re2dj/hle/guest_gdi.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/win32_errors.h"
#include "re2dj/hle/modules/resolve_only_modules.h"

namespace re2dj::hle::modules
{
namespace
{

// GetStockObject(i): the stock object's handle. Stock objects are shared and
// never freed, so the handle is a constant; an unknown index gives NULL
// without touching the last error. Measured on Windows 11 (WOW64).
bool GetStockObject(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        if (error != nullptr)
        {
            *error = result == nullptr ? "gdi32 result is null"
                                       : "gdi32 GetStockObject argument shape is invalid";
        }
        return false;
    }
    *result = {};
    result->eax = StockObjectHandle(call.arguments[0]);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// The guest process of a call of `count` arguments with its result cleared,
// or null with error set.
GuestProcess* RequireProcess(const ImportCall& call,
                             ImportReturn* result,
                             std::size_t count,
                             const char* name,
                             std::string* error)
{
    if (result == nullptr || call.arguments.size() != count)
    {
        com::Fail(error, std::string("gdi32 ") + name + (result == nullptr ? " result is null" : " argument shape is invalid"));
        return nullptr;
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        com::Fail(error, std::string("gdi32 ") + name + " needs the guest process");
    }
    return process;
}

bool Succeed(std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// Whether a handle is one of the stock objects GetStockObject gives.
bool IsStockObject(std::uint32_t handle)
{
    for (std::uint32_t index = 0; index < 20; ++index)
    {
        if (handle != 0 && StockObjectHandle(index) == handle)
        {
            return true;
        }
    }
    return false;
}

// CreateSolidBrush(color): a brush of the COLORREF as given, whatever its
// high byte, leaving the last error alone (measured on Windows 11).
bool CreateSolidBrush(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = RequireProcess(call, result, 1, "CreateSolidBrush", error);
    if (process == nullptr)
    {
        return false;
    }
    result->eax = process->gdi().AddBrush(call.arguments[0]);
    return Succeed(error);
}

// DeleteObject(hObject), as measured on Windows 11: TRUE for a stock object,
// which stays usable, and for an object of the process, which goes (a DC
// included); FALSE for 0 or an unknown handle. The last error never changes.
bool DeleteObject(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = RequireProcess(call, result, 1, "DeleteObject", error);
    if (process == nullptr)
    {
        return false;
    }
    // A DC or bitmap goes through the bitmap rules: a selected bitmap waits
    // for its DC, and bits the facade allocated are freed.
    const std::uint32_t handle = call.arguments[0];
    result->eax = IsStockObject(handle) || DeleteDcOrBitmap(*process, handle) || process->gdi().Delete(handle)
                      ? 1U
                      : 0U;
    return Succeed(error);
}

// SetTextColor(hdc, color): the previous color, the new one kept as given
// (0 at first); a handle that is no DC is CLR_INVALID with
// ERROR_INVALID_HANDLE. Otherwise the last error stays (measured).
bool SetTextColor(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = RequireProcess(call, result, 2, "SetTextColor", error);
    if (process == nullptr)
    {
        return false;
    }
    GuestDc* dc = process->gdi().FindDc(call.arguments[0]);
    if (dc == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        result->eax = 0xFFFFFFFFU;
        return Succeed(error);
    }
    result->eax = dc->text_color;
    dc->text_color = call.arguments[1];
    return Succeed(error);
}

// SetBkColor(hdc, color): the previous color, the new one kept as given
// (white at first); a handle that is no DC is CLR_INVALID with
// ERROR_INVALID_HANDLE. Otherwise the last error stays (measured). Windows
// answers a deleted DC with ERROR_INVALID_PARAMETER, which the facade does
// not tell apart from any other handle.
bool SetBkColor(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = RequireProcess(call, result, 2, "SetBkColor", error);
    if (process == nullptr)
    {
        return false;
    }
    GuestDc* dc = process->gdi().FindDc(call.arguments[0]);
    if (dc == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        result->eax = 0xFFFFFFFFU;
        return Succeed(error);
    }
    result->eax = dc->background_color;
    dc->background_color = call.arguments[1];
    return Succeed(error);
}

// SetBkMode(hdc, mode): the previous mode, the new one kept whatever its
// value (OPAQUE at first); a handle that is no DC is 0 with
// ERROR_INVALID_HANDLE. Otherwise the last error stays (measured).
bool SetBkMode(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = RequireProcess(call, result, 2, "SetBkMode", error);
    if (process == nullptr)
    {
        return false;
    }
    GuestDc* dc = process->gdi().FindDc(call.arguments[0]);
    if (dc == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return Succeed(error);
    }
    result->eax = dc->background_mode;
    dc->background_mode = call.arguments[1];
    return Succeed(error);
}

// BITMAPINFOHEADER (40 bytes).
struct BitmapInfoHeader
{
    std::uint32_t size = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t planes = 0;
    std::uint16_t bit_count = 0;
    std::uint32_t compression = 0;
    std::uint32_t size_image = 0;
    std::int32_t x_pixels_per_meter = 0;
    std::int32_t y_pixels_per_meter = 0;
    std::uint32_t colors_used = 0;
    std::uint32_t colors_important = 0;
};
static_assert(sizeof(BitmapInfoHeader) == 40);

constexpr std::uint32_t kBiRgb = 0;
constexpr std::uint32_t kBiBitfields = 3;
constexpr std::uint32_t kDibRgbColors = 0;
constexpr std::uint32_t kSrcCopy = 0x00CC0020U;

// StretchDIBits(hdc, xDest, yDest, DestWidth, DestHeight, xSrc, ySrc,
// SrcWidth, SrcHeight, lpBits, lpbmi, iUsage, rop) into the bitmap selected in
// a facade DC, as measured on Windows 11 into an RGB565 DIB section:
// - channels convert by the GDI rules (gdi_raster.h);
// - ySrc of a bottom-up DIB counts rows from its bottom;
// - stretching repeats the nearest source pixel;
// - destination pixels outside the bitmap are clipped;
// - the result is ySrc + SrcHeight (2, 1, and 2 for the measured (0, 2),
//   (0, 1), and (1, 1)), and the last error is untouched.
// SRCCOPY of 8-bit palettized, 16-bit (5-5-5 or bitfields), 24-bit and 32-bit
// DIBs, with positive extents inside the source, is what was measured;
// anything else stops. An 8-bit DIB's palette (biClrUsed entries, 256 when 0)
// converts exactly as the same colours in a 24-bit DIB, and an index past the
// palette gives black (design 426). A 32-bit BI_RGB DIB converts as the
// 24-bit one does, its top byte ignored (task 431).
bool StretchDIBits(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 13)
    {
        return com::Fail(error, result == nullptr ? "gdi32 result is null"
                                                  : "gdi32 StretchDIBits argument shape is invalid");
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        return com::Fail(error, "gdi32 StretchDIBits needs the guest process");
    }
    const auto arg = [&](std::size_t index) { return static_cast<std::int32_t>(call.arguments[index]); };
    const std::int32_t x_dest = arg(1);
    const std::int32_t y_dest = arg(2);
    const std::int32_t dest_width = arg(3);
    const std::int32_t dest_height = arg(4);
    const std::int32_t x_src = arg(5);
    const std::int32_t y_src = arg(6);
    const std::int32_t src_width = arg(7);
    const std::int32_t src_height = arg(8);
    const std::uint32_t bits = call.arguments[9];
    const std::uint32_t info = call.arguments[10];

    const GuestDc* dc = process->gdi().FindDc(call.arguments[0]);
    const GuestBitmap* target = dc == nullptr ? nullptr : process->gdi().FindBitmap(dc->bitmap);
    if (target == nullptr || target->bits_per_pixel != 16)
    {
        return com::Fail(error, "gdi32 StretchDIBits has no model of this DC's bitmap");
    }
    if (call.arguments[11] != kDibRgbColors || call.arguments[12] != kSrcCopy || bits == 0 || info == 0)
    {
        return com::Fail(error, "gdi32 StretchDIBits has no model of this usage or raster operation");
    }
    BitmapInfoHeader header;
    if (!com::ReadStruct(call, info, &header, error))
    {
        return false;
    }
    GdiPixelLayout source_layout;
    // An 8-bit DIB's colours as 24-bit pixels, indexed by the source byte.
    std::vector<std::uint32_t> palette;
    if (header.size == sizeof(BitmapInfoHeader) && header.bit_count == 8 && header.compression == kBiRgb)
    {
        const std::uint32_t entries = header.colors_used == 0 ? 256U : std::min(header.colors_used, 256U);
        std::vector<std::uint8_t> quads(static_cast<std::size_t>(entries) * 4);
        if (!com::ReadBytes(call, info + sizeof(BitmapInfoHeader), quads, error))
        {
            return false;
        }
        palette.assign(256, 0);
        for (std::uint32_t index = 0; index < entries; ++index)
        {
            // RGBQUAD: blue, green, red, reserved.
            palette[index] = static_cast<std::uint32_t>(quads[index * 4]) |
                             (static_cast<std::uint32_t>(quads[index * 4 + 1]) << 8) |
                             (static_cast<std::uint32_t>(quads[index * 4 + 2]) << 16);
        }
        source_layout.bits_per_pixel = 8;
    }
    else if (header.size == sizeof(BitmapInfoHeader) && header.bit_count == 16 && header.compression == kBiRgb)
    {
        source_layout = kGdiRgb555;
    }
    else if (header.size == sizeof(BitmapInfoHeader) && header.bit_count == 16 && header.compression == kBiBitfields)
    {
        source_layout.bits_per_pixel = 16;
        if (!com::ReadStruct(call, info + sizeof(BitmapInfoHeader), &source_layout.masks, error))
        {
            return false;
        }
    }
    else if (header.size == sizeof(BitmapInfoHeader) && header.bit_count == 24 && header.compression == kBiRgb)
    {
        source_layout = kGdiBgr888;
    }
    else if (header.size == sizeof(BitmapInfoHeader) && header.bit_count == 32 && header.compression == kBiRgb)
    {
        // Converted as the 24-bit pixels are, the top byte ignored, as
        // measured on Windows 11 (task 431).
        source_layout = kGdiXrgb8888;
    }
    else
    {
        return com::Fail(error, "gdi32 StretchDIBits has no model of a " + std::to_string(header.bit_count) +
                                    "-bit DIB with compression " + std::to_string(header.compression));
    }
    const bool bottom_up = header.height > 0;
    const std::int32_t source_rows = bottom_up ? header.height : -header.height;
    if (dest_width <= 0 || dest_height <= 0 || src_width <= 0 || src_height <= 0 || x_src < 0 || y_src < 0 ||
        x_src + src_width > header.width || y_src + src_height > source_rows || (!bottom_up && y_src != 0))
    {
        return com::Fail(error, "gdi32 StretchDIBits has no model of these extents");
    }

    const GdiPixelLayout target_layout = {16, target->masks};
    const std::uint32_t source_row_bytes =
        DibRowBytes(static_cast<std::uint32_t>(header.width), source_layout.bits_per_pixel);
    const std::uint32_t source_pixel_bytes = source_layout.bits_per_pixel / 8;
    // The destination columns inside the bitmap.
    const std::int32_t first_column = std::max(x_dest, 0);
    const std::int32_t end_column = std::min(x_dest + dest_width, static_cast<std::int32_t>(target->width));
    std::vector<std::uint8_t> source_row(source_row_bytes);
    std::vector<std::uint8_t> target_row;
    for (std::int32_t row = 0; row < dest_height && first_column < end_column; ++row)
    {
        const std::int32_t target_y = y_dest + row;
        if (target_y < 0 || target_y >= static_cast<std::int32_t>(target->height))
        {
            continue;
        }
        const std::uint32_t offset_in_rect = StretchSourceIndex(static_cast<std::uint32_t>(row),
                                                                static_cast<std::uint32_t>(dest_height),
                                                                static_cast<std::uint32_t>(src_height));
        // Memory row of the source: bottom-up rows count from the bottom, and
        // the rectangle's top row is its highest.
        const std::uint32_t source_memory_row =
            bottom_up ? static_cast<std::uint32_t>(y_src + src_height - 1) - offset_in_rect
                      : static_cast<std::uint32_t>(y_src) + offset_in_rect;
        if (!com::ReadBytes(call, bits + source_memory_row * source_row_bytes, source_row, error))
        {
            return false;
        }
        target_row.assign(static_cast<std::size_t>(end_column - first_column) * 2, 0);
        // A surface's true-color plane takes the source pixel at its own depth.
        std::uint32_t* const plane_row =
            target->true_color == nullptr ? nullptr : target->true_color->Row(static_cast<std::uint32_t>(target_y));
        for (std::int32_t column = first_column; column < end_column; ++column)
        {
            const std::uint32_t source_x =
                static_cast<std::uint32_t>(x_src) +
                StretchSourceIndex(static_cast<std::uint32_t>(column - x_dest), static_cast<std::uint32_t>(dest_width),
                                   static_cast<std::uint32_t>(src_width));
            const std::span<const std::uint8_t> source_pixel =
                std::span<const std::uint8_t>(source_row).subspan(source_x * source_pixel_bytes);
            const std::uint32_t pixel =
                palette.empty() ? ReadGdiPixel(source_pixel, source_layout.bits_per_pixel) : palette[source_pixel[0]];
            const GdiPixelLayout& layout = palette.empty() ? source_layout : kGdiBgr888;
            WriteGdiPixel(std::span<std::uint8_t>(target_row).subspan(static_cast<std::size_t>(column - first_column) * 2),
                          16, ConvertGdiPixel(pixel, layout, target_layout));
            if (plane_row != nullptr)
            {
                plane_row[column] = ConvertGdiPixel(pixel, layout, kGdiXrgb8888);
            }
        }
        const std::uint32_t target_memory_row =
            target->top_down ? static_cast<std::uint32_t>(target_y) : target->height - 1 - static_cast<std::uint32_t>(target_y);
        const std::uint32_t address =
            target->bits + target_memory_row * target->pitch + static_cast<std::uint32_t>(first_column) * 2;
        if (!com::WriteBytes(call, address, target_row, error))
        {
            return false;
        }
    }
    result->eax = static_cast<std::uint32_t>(y_src + src_height);
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

GuestExportDescriptor MakeExport(std::string name, std::uint32_t argument_count, ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

// wingdi.h signatures.
constexpr ResolveOnlyExport kGdi32ResolveOnly[] = {
    {"SelectPalette", 3}, {"CreatePalette", 1},
    {"BitBlt", 9},
    {"CreateDIBSection", 6},
    // EZ2DJ 1st's imports (Task 405).
    {"ExtTextOutA", 8},
};

}  // namespace

std::uint32_t StockObjectHandle(std::uint32_t index)
{
    // WHITE_BRUSH (0) through DC_PEN (19); index 9 is unused. The values are
    // those Windows 11 hands a 32-bit process.
    static constexpr std::array<std::uint32_t, 20> kStockObjects = {
        0x00900010U, 0x00900014U, 0x00900012U, 0x00900013U, 0x00900011U,
        0x00900015U, 0x00B00018U, 0x00B00017U, 0x00B00016U, 0x00000000U,
        0x018A0831U, 0x008A0024U, 0x008A0023U, 0x028A0021U, 0x008A0022U,
        0x0088000BU, 0x018A0832U, 0x000A0834U, 0x0190001CU, 0x00B00019U,
    };
    return index < kStockObjects.size() ? kStockObjects[index] : 0;
}

GuestModuleDescriptor MakeGdi32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "gdi32.dll";
    descriptor.aliases = {"gdi32"};
    descriptor.exports.push_back(MakeExport("GetStockObject", 1, &GetStockObject));
    descriptor.exports.push_back(MakeExport("StretchDIBits", 13, &StretchDIBits));
    descriptor.exports.push_back(MakeExport("CreateSolidBrush", 1, &CreateSolidBrush));
    descriptor.exports.push_back(MakeExport("DeleteObject", 1, &DeleteObject));
    descriptor.exports.push_back(MakeExport("SetTextColor", 2, &SetTextColor));
    descriptor.exports.push_back(MakeExport("SetBkMode", 2, &SetBkMode));
    descriptor.exports.push_back(MakeExport("SetBkColor", 2, &SetBkColor));
    // Bitmaps loaded from files (Task 421).
    descriptor.exports.push_back(MakeExport("GetObjectA", 3, &GetObjectA));
    descriptor.exports.push_back(MakeExport("CreateCompatibleDC", 1, &CreateCompatibleDC));
    descriptor.exports.push_back(MakeExport("SelectObject", 2, &SelectObject));
    descriptor.exports.push_back(MakeExport("StretchBlt", 11, &StretchBlt));
    descriptor.exports.push_back(MakeExport("DeleteDC", 1, &DeleteDC));
    AddResolveOnlyExports(&descriptor, kGdi32ResolveOnly);
    return descriptor;
}

}  // namespace re2dj::hle::modules
