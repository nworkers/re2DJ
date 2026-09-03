#ifndef RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_NULL_CONTEXT_OBJECT_STATE_H
#define RE2DJ_TOOLS_WINDOWS_X86_LAUNCHER_PROBE_NULL_CONTEXT_OBJECT_STATE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <windows.h>

namespace re2dj::tools::launcher_probe {

// Read-only inspection helpers for the EZ2DJ 4th null-context object. The
// launcher probe stops the child at the boundary that precedes the field read
// and uses these helpers to describe how the object looks at that moment and
// which callers reached it. Nothing here writes guest memory.

// One frame of a standard `push ebp; mov ebp, esp` chain.
struct CallerFrame
{
    std::uintptr_t frame = 0;
    std::uintptr_t saved_frame = 0;
    std::uintptr_t return_address = 0;
    bool return_in_image = false;
};

// One nonzero dword found inside the inspected object window.
struct ObjectWindowEntry
{
    std::uint32_t offset = 0;
    std::uint32_t value = 0;
    // "image", "stack", or "other" for the observed value range.
    const char* classification = "other";
};

struct ObjectWindowScan
{
    bool readable = false;
    std::uint32_t bytes_scanned = 0;
    std::uint32_t dwords_scanned = 0;
    std::uint32_t nonzero_count = 0;
    bool field_offset_scanned = false;
    std::uint32_t field_offset = 0;
    std::uint32_t field_value = 0;
    bool capped = false;
    std::vector<ObjectWindowEntry> entries;
};

// Value ranges used to classify observed dwords.
struct AddressRanges
{
    std::uintptr_t image_base = 0;
    std::uintptr_t image_end = 0;
    std::uintptr_t stack_base = 0;
    std::uintptr_t stack_end = 0;
};

// Walks at most `max_frames` frames starting at `frame_pointer`. Stops on an
// unreadable frame or a frame pointer that does not increase, so a corrupted
// chain cannot loop.
std::vector<CallerFrame> CollectCallerFrames(HANDLE process,
                                             std::uintptr_t frame_pointer,
                                             const AddressRanges& ranges,
                                             std::size_t max_frames);

// Reads `window_bytes` from `object_address` and summarizes the dwords. At most
// `max_entries` nonzero dwords are returned; the rest only raise `capped`.
ObjectWindowScan ScanObjectWindow(HANDLE process,
                                  std::uintptr_t object_address,
                                  std::uint32_t window_bytes,
                                  std::uint32_t field_offset,
                                  const AddressRanges& ranges,
                                  std::size_t max_entries);

}  // namespace re2dj::tools::launcher_probe

#endif
