#ifndef RE2DJ_HLE_MODULES_GDI_BITMAPS_H_
#define RE2DJ_HLE_MODULES_GDI_BITMAPS_H_

// Bitmaps loaded from files and copied onto surfaces: user32's LoadImageA
// and the gdi32 calls a DDCopyBitmap-style loader makes with the result.
// Internal to src/hle/modules. See docs/design/20260928-421-bitmap-files.md.

#include <cstdint>
#include <string>

#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/import_dispatcher.h"

namespace re2dj::hle::modules
{

bool LoadImageA(const ImportCall& call, ImportReturn* result, std::string* error);
bool GetObjectA(const ImportCall& call, ImportReturn* result, std::string* error);
bool CreateCompatibleDC(const ImportCall& call, ImportReturn* result, std::string* error);
bool SelectObject(const ImportCall& call, ImportReturn* result, std::string* error);
bool StretchBlt(const ImportCall& call, ImportReturn* result, std::string* error);
bool DeleteDC(const ImportCall& call, ImportReturn* result, std::string* error);
// Bitmaps the guest builds in memory (task 434): a DIB section of its own,
// and a device-dependent bitmap made from a DIB.
bool CreateDIBSection(const ImportCall& call, ImportReturn* result, std::string* error);
bool CreateDIBitmap(const ImportCall& call, ImportReturn* result, std::string* error);

// DeleteObject of a DC or bitmap: true when handle was one. A DC goes and its
// bitmap is deselected; a bitmap a DC still holds goes once deselected, and
// otherwise at once, with any bits the facade allocated for it.
bool DeleteDcOrBitmap(GuestProcess& process, std::uint32_t handle);

}  // namespace re2dj::hle::modules

#endif  // RE2DJ_HLE_MODULES_GDI_BITMAPS_H_
