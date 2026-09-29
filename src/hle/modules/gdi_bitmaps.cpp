#include "gdi_bitmaps.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <vector>

#include "facade_com.h"
#include "re2dj/hle/bitmap_file.h"
#include "re2dj/hle/gdi_raster.h"
#include "re2dj/hle/guest_files.h"
#include "re2dj/hle/guest_gdi.h"
#include "re2dj/hle/modules/gdi32_module.h"
#include "re2dj/hle/win32_errors.h"

namespace re2dj::hle::modules
{
namespace
{

constexpr std::uint32_t kImageBitmap = 0;
constexpr std::uint32_t kLrLoadFromFile = 0x00000010U;
constexpr std::uint32_t kLrCreateDibSection = 0x00002000U;
constexpr std::uint32_t kSrcCopy = 0x00CC0020U;
constexpr std::uint32_t kBitmapStructSize = 24;
constexpr std::uint32_t kDibSectionStructSize = 84;

// BITMAP (wingdi.h), 24 bytes in a 32-bit process.
struct BitmapStruct
{
    std::int32_t type = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t width_bytes = 0;
    std::uint16_t planes = 1;
    std::uint16_t bits_per_pixel = 0;
    std::uint32_t bits = 0;
};
static_assert(sizeof(BitmapStruct) == kBitmapStructSize);

// The guest process of a call of `count` arguments with its result cleared,
// or null with error set.
GuestProcess* Require(const ImportCall& call, ImportReturn* result, std::size_t count, std::string* error)
{
    if (result == nullptr || call.arguments.size() != count)
    {
        com::Fail(error, com::CallName(call) + (result == nullptr ? " result is null" : " argument shape is invalid"));
        return nullptr;
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        com::Fail(error, com::CallName(call) + " needs the guest process");
    }
    return process;
}

bool Done(std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// Whether a handle is one GetStockObject gives.
bool IsStockHandle(std::uint32_t handle)
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

// Frees a bitmap and the bits the facade allocated for it.
void ReleaseBitmap(GuestProcess& process, std::uint32_t handle)
{
    const GuestBitmap* bitmap = process.gdi().FindBitmap(handle);
    if (bitmap != nullptr && bitmap->owns_bits && bitmap->bits != 0)
    {
        process.VirtualFree(bitmap->bits, 0, kMemRelease);
    }
    process.gdi().Delete(handle);
}

// A DC let go of bitmap: one whose DeleteObject waited on it goes now.
void Deselected(GuestProcess& process, std::uint32_t bitmap)
{
    const GuestBitmap* found = process.gdi().FindBitmap(bitmap);
    if (found != nullptr && found->delete_pending && process.gdi().DcSelecting(bitmap) == 0)
    {
        ReleaseBitmap(process, bitmap);
    }
}

// Reads a whole guest file; false stops the call with error set, and an open
// failure comes back as *open_error with true.
bool ReadWholeFile(const ImportCall& call,
                   const std::string& name,
                   std::vector<std::uint8_t>* bytes,
                   std::uint32_t* open_error,
                   std::string* error)
{
    GuestFiles* files = call.services->Files();
    if (files == nullptr)
    {
        return com::Fail(error, com::CallName(call) + " has no guest files for " + name);
    }
    const GuestFiles::OpenResult opened = files->Open(name, true, false, kOpenExisting);
    if (opened.outside_root)
    {
        return com::Fail(error, com::CallName(call) + " of a file outside the guest root is not modelled: " + name);
    }
    *open_error = opened.error;
    if (opened.handle == 0)
    {
        return true;
    }
    std::uint64_t size = 0;
    const bool read = files->Size(opened.handle, &size) == kWin32ErrorSuccess &&
                      files->Read(opened.handle, static_cast<std::uint32_t>(size), bytes) == kWin32ErrorSuccess;
    files->Close(opened.handle);
    if (!read)
    {
        return com::Fail(error, com::CallName(call) + " cannot read " + name);
    }
    return true;
}

// The pixel at column x of a source row, as a kGdiBgr888 value.
std::uint32_t SourcePixel(const GuestBitmap& source, std::span<const std::uint8_t> row, std::uint32_t x)
{
    if (source.bits_per_pixel == 24)
    {
        return ReadGdiPixel(row.subspan(static_cast<std::size_t>(x) * 3), 24);
    }
    const std::uint8_t index = row[x];
    return index < source.color_table.size() ? source.color_table[index] : 0;
}

}  // namespace

// LoadImageA(hInst, name, type, cx, cy, fuLoad) for a bitmap file loaded as a
// DIB section, as measured on Windows 11: the bits keep the file's bottom-up
// rows with padding zeroed, in fresh RW pages, and the last error becomes 0.
// A file that cannot open gives NULL with ERROR_FILE_NOT_FOUND (also for a
// missing directory), one that is no BMP NULL with 0.
bool LoadImageA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = Require(call, result, 6, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[0] != 0 || call.arguments[2] != kImageBitmap || call.arguments[3] != 0 ||
        call.arguments[4] != 0 || call.arguments[5] != (kLrLoadFromFile | kLrCreateDibSection) ||
        call.arguments[1] == 0)
    {
        return com::Fail(error, com::CallName(call) + " is modelled only for a bitmap file loaded as a DIB section");
    }
    std::string name;
    std::string read_error;
    if (!call.services->ReadGuestString(runtime::GuestAddress(call.arguments[1]), &name, &read_error))
    {
        return com::Fail(error, com::CallName(call) + " cannot read its file name: " + read_error);
    }
    std::vector<std::uint8_t> bytes;
    std::uint32_t open_error = 0;
    if (!ReadWholeFile(call, name, &bytes, &open_error, error))
    {
        return false;
    }
    if (open_error != kWin32ErrorSuccess)
    {
        call.services->SetLastError(open_error == kWin32ErrorPathNotFound ? kWin32ErrorFileNotFound : open_error);
        return Done(error);
    }
    BitmapFileImage image;
    std::string reason;
    const BitmapFileOutcome outcome = ParseBitmapFile(bytes, &image, &reason);
    if (outcome == BitmapFileOutcome::kNotModelled)
    {
        return com::Fail(error, com::CallName(call) + " has no model of " + reason + ": " + name);
    }
    if (outcome == BitmapFileOutcome::kNotBitmap)
    {
        call.services->SetLastError(kWin32ErrorSuccess);
        return Done(error);
    }

    std::uint32_t bits = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
    if (process->VirtualAlloc(0, static_cast<std::uint32_t>(image.rows.size()), kMemCommit | kMemReserve,
                              kPageReadWrite, &bits, &committed) != GuestMemoryResult::kOk)
    {
        return com::Fail(error, com::CallName(call) + " has no guest memory for " + name);
    }
    for (const auto& [address, length] : committed)
    {
        const std::vector<std::uint8_t> zeros(length, 0);
        if (!com::WriteBytes(call, address, zeros, error))
        {
            process->VirtualFree(bits, 0, kMemRelease);
            return false;
        }
    }
    if (!com::WriteBytes(call, bits, image.rows, error))
    {
        process->VirtualFree(bits, 0, kMemRelease);
        return false;
    }
    GuestBitmap bitmap;
    bitmap.width = image.width;
    bitmap.height = image.height;
    bitmap.bits_per_pixel = image.bits_per_pixel;
    bitmap.pitch = image.row_bytes;
    bitmap.bits = bits;
    bitmap.top_down = false;
    bitmap.color_table = std::move(image.color_table);
    bitmap.owns_bits = true;
    result->eax = process->gdi().AddBitmap(std::move(bitmap));
    call.services->SetLastError(kWin32ErrorSuccess);
    return Done(error);
}

// GetObjectA(h, c, pv) of a bitmap, as measured: a NULL buffer or c from 24
// gives the 24-byte BITMAP, a smaller c gives 0; the last error stays. The
// 84-byte DIBSECTION was not measured field by field and stops.
bool GetObjectA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = Require(call, result, 3, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t handle = call.arguments[0];
    const std::uint32_t size = call.arguments[1];
    const std::uint32_t buffer = call.arguments[2];
    BitmapStruct bitmap;
    if (handle == GuestGdi::kDefaultBitmap)
    {
        bitmap.width = 1;
        bitmap.height = 1;
        bitmap.width_bytes = 2;
        bitmap.bits_per_pixel = 1;
    }
    else if (const GuestBitmap* found = process->gdi().FindBitmap(handle); found != nullptr)
    {
        bitmap.width = static_cast<std::int32_t>(found->width);
        bitmap.height = static_cast<std::int32_t>(found->height);
        bitmap.width_bytes = static_cast<std::int32_t>(found->pitch);
        bitmap.bits_per_pixel = static_cast<std::uint16_t>(found->bits_per_pixel);
        bitmap.bits = found->bits;
    }
    else
    {
        return com::Fail(error, com::CallName(call) + " has no model of this object");
    }
    if (buffer == 0)
    {
        result->eax = kBitmapStructSize;
        return Done(error);
    }
    if (size >= kDibSectionStructSize)
    {
        return com::Fail(error, com::CallName(call) + " of a DIBSECTION is not modelled");
    }
    if (size < kBitmapStructSize)
    {
        return Done(error);
    }
    if (!com::WriteStruct(call, buffer, bitmap, error))
    {
        return false;
    }
    result->eax = kBitmapStructSize;
    return Done(error);
}

// CreateCompatibleDC(NULL): a memory DC holding the default 1x1 bitmap; the
// last error stays (measured). A DC compatible with another is not modelled.
bool CreateCompatibleDC(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = Require(call, result, 1, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[0] != 0)
    {
        return com::Fail(error, com::CallName(call) + " of a DC is not modelled");
    }
    GuestDc dc;
    dc.bitmap = GuestGdi::kDefaultBitmap;
    result->eax = process->gdi().AddDc(dc);
    return Done(error);
}

// SelectObject(hdc, h) of a bitmap, as measured: the previous bitmap back; 0
// for a bitmap another DC holds, for an unknown object (last error kept), and
// for a handle that is no DC (ERROR_INVALID_HANDLE). Other objects stop.
bool SelectObject(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = Require(call, result, 2, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestGdi& gdi = process->gdi();
    GuestDc* dc = gdi.FindDc(call.arguments[0]);
    const std::uint32_t object = call.arguments[1];
    if (dc == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return Done(error);
    }
    if (object != GuestGdi::kDefaultBitmap && gdi.FindBitmap(object) == nullptr)
    {
        if (gdi.FindBrush(object) != nullptr || IsStockHandle(object) || gdi.FindDc(object) != nullptr)
        {
            return com::Fail(error, com::CallName(call) + " of a non-bitmap object is not modelled");
        }
        return Done(error);
    }
    const std::uint32_t holder = object == GuestGdi::kDefaultBitmap ? 0 : gdi.DcSelecting(object);
    if (holder != 0 && holder != call.arguments[0])
    {
        return Done(error);
    }
    const std::uint32_t previous = dc->bitmap;
    dc->bitmap = object;
    result->eax = previous;
    if (previous != object)
    {
        Deselected(*process, previous);
    }
    return Done(error);
}

// StretchBlt(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc,
// hSrc, rop) as a 1:1 SRCCOPY from an 8- or 24-bit bitmap onto a 16-bit one,
// as measured on Windows 11: rows keep their screen order, channels convert
// by the GDI rules (8 bits through the color table), destination pixels
// outside the bitmap are clipped, and the result is TRUE with the last error
// kept. A handle that is no DC gives FALSE with ERROR_INVALID_HANDLE. The
// enlarging and shrinking mappings are unresolved, so those stop.
bool StretchBlt(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = Require(call, result, 11, error);
    if (process == nullptr)
    {
        return false;
    }
    GuestGdi& gdi = process->gdi();
    const GuestDc* target_dc = gdi.FindDc(call.arguments[0]);
    const GuestDc* source_dc = gdi.FindDc(call.arguments[5]);
    if (target_dc == nullptr || source_dc == nullptr)
    {
        call.services->SetLastError(kWin32ErrorInvalidHandle);
        return Done(error);
    }
    const auto arg = [&](std::size_t index) { return static_cast<std::int32_t>(call.arguments[index]); };
    const std::int32_t x_dest = arg(1);
    const std::int32_t y_dest = arg(2);
    const std::int32_t width = arg(3);
    const std::int32_t height = arg(4);
    const std::int32_t x_src = arg(6);
    const std::int32_t y_src = arg(7);
    if (call.arguments[10] != kSrcCopy)
    {
        return com::Fail(error, com::CallName(call) + " has no model of this raster operation");
    }
    if (arg(8) != width || arg(9) != height || width <= 0 || height <= 0)
    {
        return com::Fail(error, com::CallName(call) + " has no model of stretching " + std::to_string(arg(8)) + "x" +
                                    std::to_string(arg(9)) + " to " + std::to_string(width) + "x" +
                                    std::to_string(height));
    }
    const GuestBitmap* source = gdi.FindBitmap(source_dc->bitmap);
    const GuestBitmap* target = gdi.FindBitmap(target_dc->bitmap);
    if (source == nullptr || target == nullptr || target->bits_per_pixel != 16 ||
        (source->bits_per_pixel != 24 && source->bits_per_pixel != 8))
    {
        return com::Fail(error, com::CallName(call) + " has no model of these bitmaps");
    }
    if (x_src < 0 || y_src < 0 || x_src + width > static_cast<std::int32_t>(source->width) ||
        y_src + height > static_cast<std::int32_t>(source->height))
    {
        return com::Fail(error, com::CallName(call) + " has no model of a source rectangle outside its bitmap");
    }

    const GdiPixelLayout target_layout = {16, target->masks};
    const std::int32_t first_column = std::max(x_dest, 0);
    const std::int32_t end_column = std::min(x_dest + width, static_cast<std::int32_t>(target->width));
    std::vector<std::uint8_t> source_row(source->pitch);
    std::vector<std::uint8_t> target_row;
    for (std::int32_t row = 0; row < height && first_column < end_column; ++row)
    {
        const std::int32_t target_y = y_dest + row;
        if (target_y < 0 || target_y >= static_cast<std::int32_t>(target->height))
        {
            continue;
        }
        const auto source_y = static_cast<std::uint32_t>(y_src + row);
        const std::uint32_t source_memory_row = source->top_down ? source_y : source->height - 1 - source_y;
        if (!com::ReadBytes(call, source->bits + source_memory_row * source->pitch, source_row, error))
        {
            return false;
        }
        target_row.assign(static_cast<std::size_t>(end_column - first_column) * 2, 0);
        // A surface's true-color plane takes the source pixel at 24 bits.
        std::uint32_t* const plane_row =
            target->true_color == nullptr ? nullptr : target->true_color->Row(static_cast<std::uint32_t>(target_y));
        for (std::int32_t column = first_column; column < end_column; ++column)
        {
            const auto source_x = static_cast<std::uint32_t>(x_src + column - x_dest);
            const std::uint32_t pixel = SourcePixel(*source, source_row, source_x);
            WriteGdiPixel(std::span<std::uint8_t>(target_row).subspan(static_cast<std::size_t>(column - first_column) * 2),
                          16, ConvertGdiPixel(pixel, kGdiBgr888, target_layout));
            if (plane_row != nullptr)
            {
                plane_row[column] = ConvertGdiPixel(pixel, kGdiBgr888, kGdiXrgb8888);
            }
        }
        const std::uint32_t target_memory_row = target->top_down ? static_cast<std::uint32_t>(target_y)
                                                                 : target->height - 1 - static_cast<std::uint32_t>(target_y);
        if (!com::WriteBytes(call, target->bits + target_memory_row * target->pitch +
                                       static_cast<std::uint32_t>(first_column) * 2,
                             target_row, error))
        {
            return false;
        }
    }
    result->eax = 1;
    return Done(error);
}

// DeleteDC(hdc): TRUE, its bitmap deselected; FALSE for NULL or a handle that
// is no DC, with the last error kept (measured).
bool DeleteDC(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = Require(call, result, 1, error);
    if (process == nullptr)
    {
        return false;
    }
    if (process->gdi().FindDc(call.arguments[0]) != nullptr)
    {
        result->eax = DeleteDcOrBitmap(*process, call.arguments[0]) ? 1U : 0U;
    }
    return Done(error);
}

bool DeleteDcOrBitmap(GuestProcess& process, std::uint32_t handle)
{
    GuestGdi& gdi = process.gdi();
    if (const GuestDc* dc = gdi.FindDc(handle); dc != nullptr)
    {
        const std::uint32_t bitmap = dc->bitmap;
        gdi.Delete(handle);
        Deselected(process, bitmap);
        return true;
    }
    GuestBitmap* bitmap = gdi.FindBitmap(handle);
    if (bitmap == nullptr)
    {
        return false;
    }
    if (gdi.DcSelecting(handle) != 0)
    {
        bitmap->delete_pending = true;
        return true;
    }
    ReleaseBitmap(process, handle);
    return true;
}

}  // namespace re2dj::hle::modules
