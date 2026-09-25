#ifndef RE2DJ_HLE_GUEST_PROCESS_H_
#define RE2DJ_HLE_GUEST_PROCESS_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "re2dj/hle/guest_com.h"
#include "re2dj/hle/guest_handles.h"
#include "re2dj/hle/guest_user.h"
#include "re2dj/hle/guest_heap.h"

namespace re2dj::hle
{

// winnt.h memory values the virtual-memory exports take.
inline constexpr std::uint32_t kMemCommit = 0x00001000U;
inline constexpr std::uint32_t kMemReserve = 0x00002000U;
inline constexpr std::uint32_t kMemDecommit = 0x00004000U;
inline constexpr std::uint32_t kMemRelease = 0x00008000U;
inline constexpr std::uint32_t kPageNoAccess = 0x01U;
inline constexpr std::uint32_t kPageReadOnly = 0x02U;
inline constexpr std::uint32_t kPageReadWrite = 0x04U;
inline constexpr std::uint32_t kPageWriteCopy = 0x08U;
inline constexpr std::uint32_t kPageExecute = 0x10U;
inline constexpr std::uint32_t kPageExecuteRead = 0x20U;
inline constexpr std::uint32_t kPageExecuteReadWrite = 0x40U;
inline constexpr std::uint32_t kPageExecuteWriteCopy = 0x80U;

// Outcome of a virtual-memory request. kUnsupported marks a request Windows
// would answer but this model does not, so the caller stops instead of
// guessing.
enum class GuestMemoryResult : std::uint8_t
{
    kOk,
    kInvalidParameter,
    kInvalidAddress,
    kUnsupported,
};

// A thread timer from SetTimer with no window. It fires only when the
// thread's message loop dispatches WM_TIMER, which is not modelled yet, so it
// is recorded and never delivered.
struct GuestTimer
{
    std::uint32_t id = 0;
    std::uint32_t elapse_ms = 0;
    std::uint32_t procedure = 0;
};

// An unnamed event object from CreateEvent.
struct GuestEvent
{
    bool manual_reset = false;
    bool signaled = false;
};

// One section of a mapped PE image, relative to the image base.
struct GuestImageSection
{
    std::uint32_t virtual_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t characteristics = 0;
};

// Per-process state the guest observes through kernel32 and friends. The
// platform provides the heap and private-memory regions; this class keeps
// only the bookkeeping. Recorded page protections are reported to the guest
// but not applied to host memory.
class GuestProcess
{
public:
    // A multiple of four like Windows process IDs and identical across runs.
    static constexpr std::uint32_t kProcessId = 0x00000F00U;
    // GetCurrentProcess's pseudo-handle, (HANDLE)-1.
    static constexpr std::uint32_t kCurrentProcessHandle = 0xFFFFFFFFU;
    static constexpr std::uint32_t kHeapAlignment = GuestHeap::kAlignment;
    // The reservation a growable HeapCreate heap (maximum size 0) receives.
    static constexpr std::uint32_t kGrowableHeapReserve = 128U * 1024U * 1024U;
    static constexpr std::uint32_t kPageSize = 0x1000U;
    static constexpr std::uint32_t kAllocationGranularity = 0x10000U;

    GuestProcess() = default;

    // GetCurrentThreadId's value: a multiple of four like Windows thread IDs,
    // next to the process ID.
    static constexpr std::uint32_t kThreadId = 0x00000F04U;

    // The main image: GetModuleHandleA(NULL) and GetModuleFileNameA(NULL)
    // answer with these, and the command line quotes the path.
    void SetMainImage(std::uint32_t base, std::string module_path);
    std::uint32_t image_base() const { return image_base_; }
    const std::string& module_path() const { return module_path_; }
    // The guest address of the command line string, 0 until the first
    // GetCommandLineA places it on the process heap.
    std::uint32_t command_line() const { return command_line_; }
    void set_command_line(std::uint32_t address) { command_line_ = address; }

    // The single handle space devices and processes share.
    GuestHandleAllocator& handles() { return handles_; }

    // OpenProcess: a new handle for this process's ID, 0 for any other.
    std::uint32_t OpenProcess(std::uint32_t process_id);
    // True for the pseudo-handle or an open process handle.
    bool IsProcessHandle(std::uint32_t handle) const;
    bool CloseProcessHandle(std::uint32_t handle);

    // The process heap's guest-addressable region: GetProcessHeap's heap and
    // the one LocalAlloc and other HLE blocks come from. A second call replaces
    // the region and drops every block.
    void SetHeapRegion(std::uint32_t base, std::uint32_t size);
    std::uint32_t process_heap() const { return process_heap_.base(); }

    // A block of at least size bytes (one byte for zero), or 0 when the region
    // has no room. Blocks start on kHeapAlignment.
    std::uint32_t Allocate(std::uint32_t size);
    // False for an address that is not the start of a live block.
    bool Free(std::uint32_t address);
    // True when [address, address + size) lies within one live block.
    bool BlockContains(std::uint32_t address, std::uint32_t size) const;
    std::size_t live_blocks() const { return process_heap_.live_blocks(); }

    // HeapCreate: a heap reserved as private memory from the arena, maximum
    // bytes or kGrowableHeapReserve for 0. The handle is the region's base, as
    // a Win32 heap handle is. Returns 0 when the arena has no room.
    std::uint32_t CreateHeap(std::uint32_t maximum);
    // The heap a handle names, the process heap included; null otherwise.
    GuestHeap* FindHeap(std::uint32_t handle);
    // HeapDestroy: releases a created heap; the process heap cannot go.
    bool DestroyHeap(std::uint32_t handle);

    // CreateEvent: a new event handle from the shared handle space.
    std::uint32_t CreateEvent(bool manual_reset, bool initial_state);
    // The event a handle names, or null.
    GuestEvent* FindEvent(std::uint32_t handle);
    bool CloseEvent(std::uint32_t handle);

    // SetUnhandledExceptionFilter: stores the filter and returns the previous
    // one. Nothing consults it yet: an unhandled guest exception ends the run.
    std::uint32_t ExchangeUnhandledExceptionFilter(std::uint32_t filter);

    // SetErrorMode: stores the new mode and returns the previous one.
    std::uint32_t ExchangeErrorMode(std::uint32_t mode);
    std::uint32_t error_mode() const { return error_mode_; }

    // SetTimer(NULL, id, elapse, procedure): replaces the timer named by id
    // when it exists, otherwise creates one with a new ID. Returns the ID.
    // Windows clamps elapse to [USER_TIMER_MINIMUM, USER_TIMER_MAXIMUM].
    std::uint32_t SetThreadTimer(std::uint32_t id, std::uint32_t elapse_ms, std::uint32_t procedure);
    const std::vector<GuestTimer>& timers() const { return timers_; }

    // The process's USER objects: icons, cursors, and window classes.
    GuestUser& user() { return user_; }
    // The facade's COM objects, such as DirectDraw's.
    GuestComObjects& com() { return com_; }

    // Records a mapped image with the protections the Windows loader gives:
    // read-only headers, then each section by its characteristics.
    void AddImageRegion(std::uint32_t base,
                        std::uint32_t size_of_image,
                        std::span<const GuestImageSection> sections);
    // The loader's page protection for a section's characteristics.
    static std::uint32_t SectionProtection(std::uint32_t characteristics);

    // The guest-addressable arena VirtualAlloc reserves from.
    void SetPrivateArena(std::uint32_t base, std::uint32_t size);

    // VirtualAlloc. On kOk, *allocated is the page-aligned start and
    // newly_committed lists the (address, size) ranges that became committed,
    // which the caller must zero.
    GuestMemoryResult VirtualAlloc(std::uint32_t address,
                                   std::uint32_t size,
                                   std::uint32_t type,
                                   std::uint32_t protect,
                                   std::uint32_t* allocated,
                                   std::vector<std::pair<std::uint32_t, std::uint32_t>>* newly_committed);
    GuestMemoryResult VirtualFree(std::uint32_t address, std::uint32_t size, std::uint32_t type);
    GuestMemoryResult VirtualProtect(std::uint32_t address,
                                     std::uint32_t size,
                                     std::uint32_t protect,
                                     std::uint32_t* old_protect);
    // The recorded protection of the page holding address, or 0 when the page
    // is not committed in any region.
    std::uint32_t PageProtection(std::uint32_t address) const;
    // True when every page of [address, address + size) is committed private
    // memory.
    bool PrivateCommitted(std::uint32_t address, std::uint32_t size) const;
    // True when every page of [address, address + size) lies in one region and
    // is committed and accessible (not PAGE_NOACCESS), image or private.
    bool Accessible(std::uint32_t address, std::uint32_t size) const;

private:
    struct Region
    {
        std::uint32_t base = 0;
        std::uint32_t size = 0;
        bool image = false;
        std::vector<std::uint32_t> protection;
        std::vector<bool> committed;
    };

    Region* FindRegion(std::uint32_t address);
    const Region* FindRegion(std::uint32_t address) const;

    GuestHandleAllocator handles_;
    std::uint32_t image_base_ = 0;
    std::string module_path_;
    std::uint32_t command_line_ = 0;
    std::vector<std::uint32_t> process_handles_;
    GuestHeap process_heap_;
    // HeapCreate heaps by handle.
    std::map<std::uint32_t, GuestHeap> heaps_;
    std::uint32_t error_mode_ = 0;
    std::uint32_t unhandled_exception_filter_ = 0;
    std::uint32_t arena_base_ = 0;
    std::uint32_t arena_size_ = 0;
    std::vector<GuestTimer> timers_;
    std::map<std::uint32_t, GuestEvent> events_;
    std::uint32_t next_timer_id_ = 1;
    GuestUser user_;
    GuestComObjects com_;
    // Image and private regions by base address.
    std::map<std::uint32_t, Region> regions_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_GUEST_PROCESS_H_
