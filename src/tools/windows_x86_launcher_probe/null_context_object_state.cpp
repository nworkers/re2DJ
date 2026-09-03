#include "null_context_object_state.h"

#include <array>

namespace re2dj::tools::launcher_probe {
namespace {

bool ReadRemoteDword(HANDLE process, std::uintptr_t address, std::uint32_t* out)
{
    std::uint32_t value = 0;
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(address),
                          &value,
                          sizeof(value),
                          &copied) == FALSE ||
        copied != sizeof(value))
    {
        return false;
    }
    *out = value;
    return true;
}

bool InRange(std::uintptr_t value, std::uintptr_t begin, std::uintptr_t end)
{
    return begin != end && value >= begin && value < end;
}

const char* ClassifyValue(std::uint32_t value, const AddressRanges& ranges)
{
    const std::uintptr_t address = static_cast<std::uintptr_t>(value);
    if (InRange(address, ranges.image_base, ranges.image_end))
    {
        return "image";
    }
    if (InRange(address, ranges.stack_base, ranges.stack_end))
    {
        return "stack";
    }
    return "other";
}

}  // namespace

std::vector<CallerFrame> CollectCallerFrames(HANDLE process,
                                             std::uintptr_t frame_pointer,
                                             const AddressRanges& ranges,
                                             std::size_t max_frames)
{
    std::vector<CallerFrame> frames;
    std::uintptr_t current = frame_pointer;
    while (frames.size() < max_frames && current != 0)
    {
        std::uint32_t saved_frame = 0;
        std::uint32_t return_address = 0;
        if (!ReadRemoteDword(process, current, &saved_frame) ||
            !ReadRemoteDword(process, current + sizeof(std::uint32_t), &return_address))
        {
            break;
        }
        CallerFrame frame;
        frame.frame = current;
        frame.saved_frame = static_cast<std::uintptr_t>(saved_frame);
        frame.return_address = static_cast<std::uintptr_t>(return_address);
        frame.return_in_image =
            InRange(frame.return_address, ranges.image_base, ranges.image_end);
        frames.push_back(frame);
        // A stack grows toward lower addresses, so a valid caller frame always
        // sits above the current one. Anything else ends the walk.
        if (frame.saved_frame <= current)
        {
            break;
        }
        current = frame.saved_frame;
    }
    return frames;
}

ObjectWindowScan ScanObjectWindow(HANDLE process,
                                  std::uintptr_t object_address,
                                  std::uint32_t window_bytes,
                                  std::uint32_t field_offset,
                                  const AddressRanges& ranges,
                                  std::size_t max_entries)
{
    ObjectWindowScan scan;
    scan.field_offset = field_offset;
    const std::uint32_t aligned_bytes = window_bytes & ~static_cast<std::uint32_t>(3);
    if (aligned_bytes == 0)
    {
        return scan;
    }
    std::vector<std::uint32_t> window(aligned_bytes / sizeof(std::uint32_t), 0);
    SIZE_T copied = 0;
    if (ReadProcessMemory(process,
                          reinterpret_cast<const void*>(object_address),
                          window.data(),
                          aligned_bytes,
                          &copied) == FALSE ||
        copied != aligned_bytes)
    {
        return scan;
    }
    scan.readable = true;
    scan.bytes_scanned = aligned_bytes;
    scan.dwords_scanned = static_cast<std::uint32_t>(window.size());
    for (std::size_t index = 0; index < window.size(); ++index)
    {
        const std::uint32_t offset =
            static_cast<std::uint32_t>(index * sizeof(std::uint32_t));
        const std::uint32_t value = window[index];
        if (offset == field_offset)
        {
            scan.field_offset_scanned = true;
            scan.field_value = value;
        }
        if (value == 0)
        {
            continue;
        }
        ++scan.nonzero_count;
        if (scan.entries.size() < max_entries)
        {
            ObjectWindowEntry entry;
            entry.offset = offset;
            entry.value = value;
            entry.classification = ClassifyValue(value, ranges);
            scan.entries.push_back(entry);
        }
        else
        {
            scan.capped = true;
        }
    }
    return scan;
}

}  // namespace re2dj::tools::launcher_probe
