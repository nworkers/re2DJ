#ifndef RE2DJ_HLE_GUEST_GDI_H_
#define RE2DJ_HLE_GUEST_GDI_H_

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "re2dj/graphics/true_color.h"

// The GDI objects the Linux facade models: device contexts, the bitmaps
// selected into them, and solid brushes. A bitmap's pixels live in guest memory: a DirectDraw
// surface's own pixels, or a DIB section's. GDI drawing then reads and
// writes guest memory as the guest's own code does.
namespace re2dj::hle
{

// A bitmap as GDI sees it: its size, depth, row layout, and where its pixels
// are. 16-bit bitmaps carry their channel masks; 8-bit ones a color table.
struct GuestBitmap
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bits_per_pixel = 0;
    // Bytes per row, rows running top to bottom from bits when top_down.
    std::uint32_t pitch = 0;
    std::uint32_t bits = 0;
    bool top_down = true;
    std::array<std::uint32_t, 3> masks{};
    // 8-bit only: 0x00RRGGBB entries.
    std::vector<std::uint32_t> color_table;
    // The facade allocated bits (a DIB section) and frees them with the
    // bitmap; a surface's bits belong to the surface.
    bool owns_bits = false;
    // DeleteObject came while a DC had the bitmap selected: it goes when
    // deselected, as on Windows.
    bool delete_pending = false;
    // A device-dependent bitmap (CreateDIBitmap): its pixels are the
    // facade's own, and GetObjectA reports no bits, as Windows does.
    bool device_dependent = false;
    // A DirectDraw surface's true-color plane, shared with the surface, when
    // it has one: GDI drawing writes the same pixels there at 8 bits per
    // channel, so what reaches the surface at 24 bits keeps them (see
    // graphics/true_color.h). Rows run top-down whatever top_down says.
    std::shared_ptr<graphics::TrueColorPlane> true_color;
};

// A device context and what is selected into it.
struct GuestDc
{
    std::uint32_t bitmap = 0;
    std::uint32_t brush = 0;
    std::uint32_t palette = 0;
    std::uint32_t text_color = 0;
    // What OPAQUE text paints its cell with; white until changed.
    std::uint32_t background_color = 0x00FFFFFFU;
    // TRANSPARENT (1) or OPAQUE (2).
    std::uint32_t background_mode = 2;
    // The window this is the display DC of (GetDC), or 0 for a memory DC.
    // A window DC has no bitmap: drawing through it is not modelled.
    std::uint32_t window = 0;
    // GetDC gave the window DC out and ReleaseDC has not taken it back.
    bool held = false;
};

class GuestGdi
{
public:
    // New objects' handles, apart from USER handles (from 0x00010010) and
    // the stock objects (0x0088xxxx to 0x028Axxxx).
    static constexpr std::uint32_t kFirstHandle = 0x0A000010U;
    // The 1x1 monochrome bitmap a new memory DC starts with (Windows 11,
    // 32-bit process). Any number of DCs may hold it.
    static constexpr std::uint32_t kDefaultBitmap = 0x0085000FU;

    std::uint32_t AddDc(GuestDc dc = {});
    // Keeps a DC under a handle given elsewhere: a window's display DC,
    // whose handle USER allotted with the window.
    void PutDc(std::uint32_t handle, GuestDc dc);
    GuestDc* FindDc(std::uint32_t handle);
    std::uint32_t AddBitmap(GuestBitmap bitmap);
    GuestBitmap* FindBitmap(std::uint32_t handle);
    // A DC with the bitmap selected, or 0.
    std::uint32_t DcSelecting(std::uint32_t bitmap) const;
    // A solid brush of a COLORREF, kept as the guest gave it.
    std::uint32_t AddBrush(std::uint32_t color);
    const std::uint32_t* FindBrush(std::uint32_t handle) const;
    // Forgets a DC, bitmap, or brush; false when the handle is none of them.
    bool Delete(std::uint32_t handle);

private:
    std::uint32_t next_handle_ = kFirstHandle;
    std::map<std::uint32_t, GuestDc> dcs_;
    std::map<std::uint32_t, GuestBitmap> bitmaps_;
    std::map<std::uint32_t, std::uint32_t> brushes_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_GDI_H_
