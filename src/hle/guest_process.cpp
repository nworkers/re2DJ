#include "re2dj/hle/guest_process.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>

namespace re2dj::hle
{
namespace
{

constexpr std::uint32_t kImageScnMemExecute = 0x20000000U;
constexpr std::uint32_t kImageScnMemRead = 0x40000000U;
constexpr std::uint32_t kImageScnMemWrite = 0x80000000U;
// PAGE_GUARD, PAGE_NOCACHE, PAGE_WRITECOMBINE.
constexpr std::uint32_t kPageModifiers = 0x700U;

std::uint64_t PageFloor(std::uint64_t value)
{
    return value & ~static_cast<std::uint64_t>(GuestProcess::kPageSize - 1);
}

std::uint64_t PageCeil(std::uint64_t value)
{
    return PageFloor(value + GuestProcess::kPageSize - 1);
}

bool IsBaseProtection(std::uint32_t protect)
{
    switch (protect)
    {
    case kPageNoAccess:
    case kPageReadOnly:
    case kPageReadWrite:
    case kPageWriteCopy:
    case kPageExecute:
    case kPageExecuteRead:
    case kPageExecuteReadWrite:
    case kPageExecuteWriteCopy:
        return true;
    default:
        return false;
    }
}

bool IsWriteCopy(std::uint32_t protect)
{
    return protect == kPageWriteCopy || protect == kPageExecuteWriteCopy;
}

// Validates a protection for a region kind: modifier bits are not modelled,
// and copy-on-write exists only for image pages.
GuestMemoryResult CheckProtection(std::uint32_t protect, bool image)
{
    if ((protect & kPageModifiers) != 0 && IsBaseProtection(protect & ~kPageModifiers))
    {
        return GuestMemoryResult::kUnsupported;
    }
    if (!IsBaseProtection(protect) || (!image && IsWriteCopy(protect)))
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    return GuestMemoryResult::kOk;
}

}  // namespace

void GuestProcess::SetMainImage(std::uint32_t base, std::string module_path)
{
    image_base_ = base;
    module_path_ = std::move(module_path);
}

std::uint32_t GuestProcess::OpenProcess(std::uint32_t process_id)
{
    if (process_id != kProcessId)
    {
        return 0;
    }
    const std::uint32_t handle = handles_.Allocate();
    process_handles_.push_back(handle);
    return handle;
}

bool GuestProcess::IsProcessHandle(std::uint32_t handle) const
{
    return handle == kCurrentProcessHandle ||
           std::find(process_handles_.begin(), process_handles_.end(), handle) !=
               process_handles_.end();
}

std::uint32_t GuestProcess::AllocateTls()
{
    for (std::uint32_t index = 0; index < kTlsSlots; ++index)
    {
        if (!tls_allocated_[index])
        {
            tls_allocated_[index] = true;
            return index;
        }
    }
    return kTlsOutOfIndexes;
}

bool GuestProcess::FreeTls(std::uint32_t index)
{
    if (index >= kTlsSlots || !tls_allocated_[index])
    {
        return false;
    }
    tls_allocated_[index] = false;
    return true;
}

GuestProcess::ChildProcess& GuestProcess::AddChildProcess(std::uint32_t host_child)
{
    ChildProcess child;
    child.host_child = host_child;
    child.process_id = next_child_id_;
    child.thread_id = next_child_id_ + 4;
    next_child_id_ += 8;
    child.process_handle = handles_.Allocate();
    child.thread_handle = handles_.Allocate();
    children_.push_back(child);
    return children_.back();
}

GuestProcess::ChildProcess* GuestProcess::FindChildProcess(std::uint32_t process_handle)
{
    if (process_handle == 0)
    {
        return nullptr;
    }
    for (ChildProcess& child : children_)
    {
        if (child.process_handle == process_handle)
        {
            return &child;
        }
    }
    return nullptr;
}

bool GuestProcess::CloseChildHandle(std::uint32_t handle)
{
    if (handle == 0)
    {
        return false;
    }
    for (ChildProcess& child : children_)
    {
        if (child.process_handle == handle)
        {
            child.process_handle = 0;
            return true;
        }
        if (child.thread_handle == handle)
        {
            child.thread_handle = 0;
            return true;
        }
    }
    return false;
}

bool GuestProcess::CloseProcessHandle(std::uint32_t handle)
{
    const auto found = std::find(process_handles_.begin(), process_handles_.end(), handle);
    if (found == process_handles_.end())
    {
        return false;
    }
    process_handles_.erase(found);
    return true;
}

void GuestProcess::SetHeapRegion(std::uint32_t base, std::uint32_t size)
{
    process_heap_ = GuestHeap(base, size);
}

std::uint32_t GuestProcess::Allocate(std::uint32_t size)
{
    return process_heap_.Allocate(size);
}

bool GuestProcess::Free(std::uint32_t address)
{
    return process_heap_.Free(address);
}

bool GuestProcess::BlockContains(std::uint32_t address, std::uint32_t size) const
{
    return process_heap_.BlockContains(address, size);
}

std::uint32_t GuestProcess::CreateHeap(std::uint32_t maximum)
{
    const std::uint32_t reserve = maximum == 0 ? kGrowableHeapReserve : maximum;
    std::uint32_t base = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> committed;
    if (VirtualAlloc(0, reserve, kMemCommit | kMemReserve, kPageReadWrite, &base, &committed) !=
        GuestMemoryResult::kOk)
    {
        return 0;
    }
    const Region* region = FindRegion(base);
    heaps_.emplace(base, GuestHeap(base, region->size));
    return base;
}

GuestHeap* GuestProcess::FindHeap(std::uint32_t handle)
{
    if (handle != 0 && handle == process_heap_.base())
    {
        return &process_heap_;
    }
    const auto heap = heaps_.find(handle);
    return heap == heaps_.end() ? nullptr : &heap->second;
}

bool GuestProcess::DestroyHeap(std::uint32_t handle)
{
    if (heaps_.erase(handle) != 1)
    {
        return false;
    }
    return VirtualFree(handle, 0, kMemRelease) == GuestMemoryResult::kOk;
}

std::uint32_t GuestProcess::CreateEvent(bool manual_reset, bool initial_state)
{
    const std::uint32_t handle = handles_.Allocate();
    events_.emplace(handle, GuestEvent{manual_reset, initial_state});
    return handle;
}

GuestEvent* GuestProcess::FindEvent(std::uint32_t handle)
{
    const auto event = events_.find(handle);
    return event == events_.end() ? nullptr : &event->second;
}

bool GuestProcess::CloseEvent(std::uint32_t handle)
{
    return events_.erase(handle) == 1;
}

std::uint32_t GuestProcess::CreateThread(std::uint32_t start, std::uint32_t parameter, std::uint32_t* thread_id)
{
    GuestThread thread;
    thread.id = next_thread_id_;
    thread.start = start;
    thread.parameter = parameter;
    next_thread_id_ += 4;
    threads_.emplace(thread.id, thread);
    const std::uint32_t handle = handles_.Allocate();
    thread_handles_.emplace(handle, thread.id);
    if (thread_id != nullptr)
    {
        *thread_id = thread.id;
    }
    return handle;
}

GuestThread* GuestProcess::FindThreadHandle(std::uint32_t handle)
{
    const auto named = thread_handles_.find(handle);
    return named == thread_handles_.end() ? nullptr : FindThread(named->second);
}

GuestThread* GuestProcess::FindThread(std::uint32_t thread_id)
{
    const auto thread = threads_.find(thread_id);
    return thread == threads_.end() ? nullptr : &thread->second;
}

bool GuestProcess::CloseThreadHandle(std::uint32_t handle)
{
    return thread_handles_.erase(handle) == 1;
}

void GuestProcess::FinishThread(std::uint32_t thread_id, std::uint32_t exit_code)
{
    GuestThread* thread = FindThread(thread_id);
    if (thread != nullptr)
    {
        thread->finished = true;
        thread->exit_code = exit_code;
    }
}

std::size_t GuestProcess::running_threads() const
{
    std::size_t running = 0;
    for (const auto& [id, thread] : threads_)
    {
        running += thread.finished ? 0 : 1;
    }
    return running;
}

std::uint32_t GuestProcess::ExchangeUnhandledExceptionFilter(std::uint32_t filter)
{
    const std::uint32_t previous = unhandled_exception_filter_;
    unhandled_exception_filter_ = filter;
    return previous;
}

std::uint32_t GuestProcess::ExchangeErrorMode(std::uint32_t mode)
{
    const std::uint32_t previous = error_mode_;
    error_mode_ = mode;
    return previous;
}

std::uint32_t GuestProcess::SetThreadTimer(std::uint32_t id,
                                           std::uint32_t elapse_ms,
                                           std::uint32_t procedure,
                                           std::uint32_t now_tick)
{
    // USER_TIMER_MINIMUM and USER_TIMER_MAXIMUM (winuser.h).
    constexpr std::uint32_t kMinimumElapse = 0x0000000AU;
    constexpr std::uint32_t kMaximumElapse = 0x7FFFFFFFU;
    const std::uint32_t elapse = std::clamp(elapse_ms, kMinimumElapse, kMaximumElapse);
    for (GuestTimer& timer : timers_)
    {
        if (id != 0 && timer.id == id)
        {
            timer.elapse_ms = elapse;
            timer.procedure = procedure;
            timer.base_tick = now_tick;
            return timer.id;
        }
    }
    timers_.push_back({next_timer_id_++, elapse, procedure, now_tick});
    return timers_.back().id;
}

GuestTimer* GuestProcess::DueTimer(std::uint32_t now_tick)
{
    for (GuestTimer& timer : timers_)
    {
        if (now_tick - timer.base_tick >= timer.elapse_ms)
        {
            return &timer;
        }
    }
    return nullptr;
}

const GuestTimer* GuestProcess::FindTimer(std::uint32_t id, std::uint32_t procedure) const
{
    for (const GuestTimer& timer : timers_)
    {
        if (timer.id == id && timer.procedure == procedure)
        {
            return &timer;
        }
    }
    return nullptr;
}

std::uint32_t GuestProcess::SectionProtection(std::uint32_t characteristics)
{
    const bool execute = (characteristics & kImageScnMemExecute) != 0;
    const bool read = (characteristics & kImageScnMemRead) != 0;
    const bool write = (characteristics & kImageScnMemWrite) != 0;
    // Writable image sections are mapped copy-on-write.
    if (write)
    {
        return execute ? kPageExecuteWriteCopy : kPageWriteCopy;
    }
    if (execute)
    {
        return read ? kPageExecuteRead : kPageExecute;
    }
    return read ? kPageReadOnly : kPageNoAccess;
}

void GuestProcess::AddImageRegion(std::uint32_t base,
                                  std::uint32_t size_of_image,
                                  std::span<const GuestImageSection> sections)
{
    Region region;
    region.base = base;
    region.size = static_cast<std::uint32_t>(PageCeil(size_of_image));
    region.image = true;
    const std::size_t pages = region.size / kPageSize;
    // The headers, and any page no section covers, stay read-only.
    region.protection.assign(pages, kPageReadOnly);
    region.committed.assign(pages, true);
    for (const GuestImageSection& section : sections)
    {
        const std::uint64_t first = PageFloor(section.virtual_address) / kPageSize;
        const std::uint64_t end =
            PageCeil(static_cast<std::uint64_t>(section.virtual_address) + section.virtual_size) /
            kPageSize;
        for (std::uint64_t page = first; page < end && page < pages; ++page)
        {
            region.protection[static_cast<std::size_t>(page)] =
                SectionProtection(section.characteristics);
        }
    }
    regions_[base] = std::move(region);
}

void GuestProcess::SetPrivateArena(std::uint32_t base, std::uint32_t size)
{
    arena_base_ = base;
    arena_size_ = size;
    heaps_.clear();
    for (auto region = regions_.begin(); region != regions_.end();)
    {
        region = region->second.image ? std::next(region) : regions_.erase(region);
    }
}

GuestProcess::Region* GuestProcess::FindRegion(std::uint32_t address)
{
    auto region = regions_.upper_bound(address);
    if (region == regions_.begin())
    {
        return nullptr;
    }
    --region;
    const std::uint64_t end = static_cast<std::uint64_t>(region->second.base) + region->second.size;
    return address < end ? &region->second : nullptr;
}

const GuestProcess::Region* GuestProcess::FindRegion(std::uint32_t address) const
{
    return const_cast<GuestProcess*>(this)->FindRegion(address);
}

GuestMemoryResult GuestProcess::VirtualAlloc(
    std::uint32_t address,
    std::uint32_t size,
    std::uint32_t type,
    std::uint32_t protect,
    std::uint32_t* allocated,
    std::vector<std::pair<std::uint32_t, std::uint32_t>>* newly_committed)
{
    if ((type & ~(kMemCommit | kMemReserve)) != 0)
    {
        return GuestMemoryResult::kUnsupported;
    }
    if (size == 0 || (type & (kMemCommit | kMemReserve)) == 0 || allocated == nullptr ||
        newly_committed == nullptr)
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    const GuestMemoryResult protection = CheckProtection(protect, false);
    if (protection != GuestMemoryResult::kOk)
    {
        return protection;
    }
    newly_committed->clear();

    if (address != 0)
    {
        // Reserving at a chosen address is not modelled; committing inside an
        // existing reservation is.
        if ((type & kMemReserve) != 0)
        {
            return GuestMemoryResult::kUnsupported;
        }
        const std::uint64_t begin = PageFloor(address);
        const std::uint64_t end = PageCeil(static_cast<std::uint64_t>(address) + size);
        Region* region = FindRegion(address);
        if (region == nullptr || region->image ||
            end > static_cast<std::uint64_t>(region->base) + region->size)
        {
            return GuestMemoryResult::kInvalidAddress;
        }
        for (std::uint64_t page = begin; page < end; page += kPageSize)
        {
            const auto index = static_cast<std::size_t>((page - region->base) / kPageSize);
            if (!region->committed[index])
            {
                region->committed[index] = true;
                newly_committed->emplace_back(static_cast<std::uint32_t>(page), kPageSize);
            }
            region->protection[index] = protect;
        }
        *allocated = static_cast<std::uint32_t>(begin);
        return GuestMemoryResult::kOk;
    }

    const std::uint64_t wanted = PageCeil(size);
    const std::uint64_t arena_end = static_cast<std::uint64_t>(arena_base_) + arena_size_;
    std::uint64_t candidate = (static_cast<std::uint64_t>(arena_base_) + kAllocationGranularity - 1) &
                              ~static_cast<std::uint64_t>(kAllocationGranularity - 1);
    // First fit at allocation granularity among the private regions.
    for (const auto& [base, region] : regions_)
    {
        if (region.image || base < arena_base_ || base >= arena_end)
        {
            continue;
        }
        if (candidate + wanted <= base)
        {
            break;
        }
        const std::uint64_t after = static_cast<std::uint64_t>(base) + region.size;
        candidate = (after + kAllocationGranularity - 1) &
                    ~static_cast<std::uint64_t>(kAllocationGranularity - 1);
    }
    if (arena_size_ == 0 || candidate + wanted > arena_end)
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    Region region;
    region.base = static_cast<std::uint32_t>(candidate);
    region.size = static_cast<std::uint32_t>(wanted);
    const bool commit = (type & kMemCommit) != 0;
    region.protection.assign(region.size / kPageSize, protect);
    region.committed.assign(region.size / kPageSize, commit);
    if (commit)
    {
        newly_committed->emplace_back(region.base, region.size);
    }
    *allocated = region.base;
    regions_[region.base] = std::move(region);
    return GuestMemoryResult::kOk;
}

GuestMemoryResult GuestProcess::VirtualFree(std::uint32_t address,
                                            std::uint32_t size,
                                            std::uint32_t type)
{
    if (type != kMemRelease && type != kMemDecommit)
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    Region* region = FindRegion(address);
    if (region == nullptr)
    {
        return GuestMemoryResult::kInvalidAddress;
    }
    if (region->image)
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    if (type == kMemRelease)
    {
        if (size != 0 || address != region->base)
        {
            return GuestMemoryResult::kInvalidParameter;
        }
        regions_.erase(region->base);
        return GuestMemoryResult::kOk;
    }
    if (size == 0 && address != region->base)
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    const std::uint64_t region_end = static_cast<std::uint64_t>(region->base) + region->size;
    const std::uint64_t begin = PageFloor(address);
    const std::uint64_t end =
        size == 0 ? region_end : PageCeil(static_cast<std::uint64_t>(address) + size);
    if (end > region_end)
    {
        return GuestMemoryResult::kInvalidAddress;
    }
    for (std::uint64_t page = begin; page < end; page += kPageSize)
    {
        region->committed[static_cast<std::size_t>((page - region->base) / kPageSize)] = false;
    }
    return GuestMemoryResult::kOk;
}

GuestMemoryResult GuestProcess::VirtualProtect(std::uint32_t address,
                                               std::uint32_t size,
                                               std::uint32_t protect,
                                               std::uint32_t* old_protect)
{
    if (size == 0 || old_protect == nullptr)
    {
        return GuestMemoryResult::kInvalidParameter;
    }
    Region* region = FindRegion(address);
    if (region == nullptr)
    {
        return GuestMemoryResult::kInvalidAddress;
    }
    const GuestMemoryResult protection = CheckProtection(protect, region->image);
    if (protection != GuestMemoryResult::kOk)
    {
        return protection;
    }
    const std::uint64_t begin = PageFloor(address);
    const std::uint64_t end = PageCeil(static_cast<std::uint64_t>(address) + size);
    if (end > static_cast<std::uint64_t>(region->base) + region->size)
    {
        return GuestMemoryResult::kInvalidAddress;
    }
    for (std::uint64_t page = begin; page < end; page += kPageSize)
    {
        if (!region->committed[static_cast<std::size_t>((page - region->base) / kPageSize)])
        {
            return GuestMemoryResult::kInvalidAddress;
        }
    }
    *old_protect = region->protection[static_cast<std::size_t>((begin - region->base) / kPageSize)];
    for (std::uint64_t page = begin; page < end; page += kPageSize)
    {
        region->protection[static_cast<std::size_t>((page - region->base) / kPageSize)] = protect;
    }
    return GuestMemoryResult::kOk;
}

std::uint32_t GuestProcess::PageProtection(std::uint32_t address) const
{
    const Region* region = FindRegion(address);
    if (region == nullptr)
    {
        return 0;
    }
    const std::size_t index = (address - region->base) / kPageSize;
    return region->committed[index] ? region->protection[index] : 0;
}

bool GuestProcess::Accessible(std::uint32_t address, std::uint32_t size) const
{
    const Region* region = FindRegion(address);
    if (region == nullptr)
    {
        return false;
    }
    const std::uint64_t end = static_cast<std::uint64_t>(address) + (size == 0 ? 1 : size);
    if (end > static_cast<std::uint64_t>(region->base) + region->size)
    {
        return false;
    }
    for (std::uint64_t page = PageFloor(address); page < end; page += kPageSize)
    {
        const auto index = static_cast<std::size_t>((page - region->base) / kPageSize);
        if (!region->committed[index] || region->protection[index] == kPageNoAccess)
        {
            return false;
        }
    }
    return true;
}

bool GuestProcess::PrivateCommitted(std::uint32_t address, std::uint32_t size) const
{
    const Region* region = FindRegion(address);
    if (region == nullptr || region->image)
    {
        return false;
    }
    const std::uint64_t end = static_cast<std::uint64_t>(address) + (size == 0 ? 1 : size);
    if (end > static_cast<std::uint64_t>(region->base) + region->size)
    {
        return false;
    }
    for (std::uint64_t page = PageFloor(address); page < end; page += kPageSize)
    {
        if (!region->committed[static_cast<std::size_t>((page - region->base) / kPageSize)])
        {
            return false;
        }
    }
    return true;
}

}  // namespace re2dj::hle
