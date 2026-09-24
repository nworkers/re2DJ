#include "../native_guest_module_set.h"

#include <sys/mman.h>

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>

#include "re2dj/hle/modules/kernel32_module.h"
#include "re2dj/hle/modules/user32_module.h"

namespace
{

using GetVersionFunction = std::uint32_t (__attribute__((stdcall))*)();
using GetActiveWindowFunction = GetVersionFunction;
using CreateFileFunction = std::uint32_t (__attribute__((stdcall))*)(
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t);

struct DispatchContext
{
    const re2dj::platform::linux::NativeGuestModuleSet* modules = nullptr;
    std::string error;
    std::uint32_t call_count = 0;
    std::uint32_t last_cleanup = 0;
};

bool Dispatch(const re2dj::platform::linux::NativeImportGateEvent& event,
              re2dj::platform::linux::NativeImportGateResult* result,
              void* opaque)
{
    auto* context = static_cast<DispatchContext*>(opaque);
    if (context == nullptr || context->modules == nullptr ||
        !context->modules->Dispatch(event, result, &context->error))
    {
        return false;
    }
    ++context->call_count;
    context->last_cleanup = result->stack_bytes_to_pop;
    return true;
}

std::string PermissionsAt(std::uint32_t address)
{
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line))
    {
        unsigned long begin = 0;
        unsigned long end = 0;
        char permissions[5] = {};
        if (std::sscanf(line.c_str(), "%lx-%lx %4s", &begin, &end, permissions) == 3 &&
            address >= begin && address < end)
        {
            return permissions;
        }
    }
    return {};
}

bool HasWritableExecutablePage(std::uint32_t base, std::uint32_t size)
{
    const std::uint64_t image_begin = base;
    const std::uint64_t image_end = image_begin + size;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line))
    {
        unsigned long begin = 0;
        unsigned long end = 0;
        char permissions[5] = {};
        if (std::sscanf(line.c_str(), "%lx-%lx %4s", &begin, &end, permissions) != 3)
        {
            continue;
        }
        const bool overlaps = static_cast<std::uint64_t>(begin) < image_end &&
                              image_begin < static_cast<std::uint64_t>(end);
        if (overlaps && permissions[1] == 'w' && permissions[2] == 'x')
        {
            return true;
        }
    }
    return false;
}

bool Fail(const std::string& message)
{
    std::fprintf(stderr, "native guest module probe: %s\n", message.c_str());
    return false;
}

bool RunProbe()
{
    using re2dj::platform::linux::NativeGuestModuleSet;
    using re2dj::runtime::GuestAddress;
    using re2dj::runtime::ImportGateTable;

    constexpr std::uint32_t kCollisionSize = 0x10000U;
    void* collision = mmap(
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(
            re2dj::platform::linux::kDefaultNativeGuestModuleBase)),
        kCollisionSize,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);
    if (collision == MAP_FAILED ||
        reinterpret_cast<std::uintptr_t>(collision) !=
            re2dj::platform::linux::kDefaultNativeGuestModuleBase)
    {
        if (collision != MAP_FAILED)
        {
            munmap(collision, kCollisionSize);
        }
        return Fail("cannot reserve the facade collision fixture");
    }

    NativeGuestModuleSet modules;
    ImportGateTable gates(GuestAddress(0xF1000000U), 16);
    std::string error;
    if (!modules.Add(re2dj::hle::modules::MakeKernel32ModuleDescriptor(),
                     &gates,
                     re2dj::platform::linux::NativeImportGateBridgeAddress(),
                     re2dj::platform::linux::NativeImportGateCleanupAddress(),
                     re2dj::platform::linux::kDefaultNativeGuestModuleBase,
                     &error))
    {
        munmap(collision, kCollisionSize);
        return Fail(error);
    }

    const auto* module = modules.registry().FindModule("KERNEL32");
    if (module == nullptr ||
        module->base.value() == re2dj::platform::linux::kDefaultNativeGuestModuleBase)
    {
        munmap(collision, kCollisionSize);
        return Fail("registry did not preserve the mapped kernel32 base");
    }
    munmap(collision, kCollisionSize);
    const auto* get_version = modules.registry().FindExport(module->base, "GetVersion");
    const auto* create_file = modules.registry().FindExport(module->base, "CreateFileA");
    if (get_version == nullptr || create_file == nullptr ||
        get_version->thunk_address.value() < module->base.value() ||
        create_file->thunk_address.value() < module->base.value() ||
        get_version->thunk_address.value() - module->base.value() >= module->image_size ||
        create_file->thunk_address.value() - module->base.value() >= module->image_size)
    {
        return Fail("registry export does not lie inside the mapped facade");
    }

    const std::string header_permissions = PermissionsAt(module->base.value());
    const std::string export_permissions = PermissionsAt(module->base.value() + 0x1000U);
    const std::string code_permissions = PermissionsAt(get_version->thunk_address.value());
    if (header_permissions.size() < 3 || export_permissions.size() < 3 ||
        code_permissions.size() < 3 || header_permissions[0] != 'r' ||
        header_permissions[1] == 'w' || header_permissions[2] == 'x' ||
        export_permissions[0] != 'r' || export_permissions[1] == 'w' ||
        export_permissions[2] == 'x' || code_permissions[0] != 'r' ||
        code_permissions[1] == 'w' || code_permissions[2] != 'x' ||
        HasWritableExecutablePage(module->base.value(), module->image_size))
    {
        return Fail("facade page protections are not R/RX without W+X");
    }

    DispatchContext dispatch_context{&modules, {}, 0, 0};
    if (!re2dj::platform::linux::ConfigureNativeImportGateHandler(
            &Dispatch, &dispatch_context))
    {
        return Fail("cannot configure native import bridge handler");
    }
    struct HandlerCleanup
    {
        ~HandlerCleanup()
        {
            re2dj::platform::linux::ClearNativeImportGateHandler();
        }
    } handler_cleanup;

    std::uint32_t stack_pointer = 0;
    __asm__ volatile("movl %%esp, %0" : "=r"(stack_pointer));
    constexpr std::uint32_t kStackWindow = 0x10000U;
    const std::uint32_t stack_limit = stack_pointer > kStackWindow
                                          ? stack_pointer - kStackWindow
                                          : 0;
    const std::uint32_t stack_base =
        stack_pointer < (std::numeric_limits<std::uint32_t>::max)() - kStackWindow
            ? stack_pointer + kStackWindow
            : (std::numeric_limits<std::uint32_t>::max)();
    re2dj::platform::linux::ConfigureNativeImportGateStackRange(stack_limit, stack_base);

    const auto get_version_function = reinterpret_cast<GetVersionFunction>(
        static_cast<std::uintptr_t>(get_version->thunk_address.value()));
    const auto create_file_function = reinterpret_cast<CreateFileFunction>(
        static_cast<std::uintptr_t>(create_file->thunk_address.value()));

    std::uint32_t before = 0;
    std::uint32_t after = 0;
    __asm__ volatile("movl %%esp, %0" : "=r"(before));
    const std::uint32_t version = get_version_function();
    __asm__ volatile("movl %%esp, %0" : "=r"(after));
    if (version != re2dj::hle::modules::kKernel32GuestVersion || before != after ||
        dispatch_context.call_count != 1 ||
        dispatch_context.last_cleanup != 0)
    {
        return Fail("GetVersion facade call violated its bridge ABI");
    }

    __asm__ volatile("movl %%esp, %0" : "=r"(before));
    const std::uint32_t file = create_file_function(1, 2, 3, 4, 5, 6, 7);
    __asm__ volatile("movl %%esp, %0" : "=r"(after));
    if (file != (std::numeric_limits<std::uint32_t>::max)() || before != after ||
        dispatch_context.call_count != 2 || dispatch_context.last_cleanup != 28)
    {
        return Fail("CreateFileA facade call violated its bridge ABI");
    }

    if (!modules.Add(re2dj::hle::modules::MakeUser32ModuleDescriptor(),
                     &gates,
                     re2dj::platform::linux::NativeImportGateBridgeAddress(),
                     re2dj::platform::linux::NativeImportGateCleanupAddress(),
                     re2dj::platform::linux::kDefaultNativeGuestModuleBase,
                     &error))
    {
        return Fail(error);
    }
    const auto* user32 = modules.registry().FindModule("USER32");
    const auto* kernel32 = modules.registry().FindModule("kernel32");
    if (user32 == nullptr || kernel32 == nullptr || user32 == kernel32 ||
        (user32->base.value() < kernel32->base.value() + kernel32->image_size &&
         kernel32->base.value() < user32->base.value() + user32->image_size))
    {
        return Fail("user32 facade is not a separate, non-overlapping module");
    }
    const auto* get_active_window =
        modules.registry().FindExport(user32->base, "GetActiveWindow");
    if (get_active_window == nullptr ||
        get_active_window->thunk_address.value() < user32->base.value() ||
        get_active_window->thunk_address.value() - user32->base.value() >= user32->image_size ||
        modules.registry().FindExport(kernel32->base, "GetActiveWindow") != nullptr)
    {
        return Fail("GetActiveWindow does not lie inside the user32 facade alone");
    }

    const auto get_active_window_function = reinterpret_cast<GetActiveWindowFunction>(
        static_cast<std::uintptr_t>(get_active_window->thunk_address.value()));
    __asm__ volatile("movl %%esp, %0" : "=r"(before));
    const std::uint32_t window = get_active_window_function();
    __asm__ volatile("movl %%esp, %0" : "=r"(after));
    if (window != 0 || before != after || dispatch_context.call_count != 3 ||
        dispatch_context.last_cleanup != 0)
    {
        return Fail("GetActiveWindow facade call violated its bridge ABI");
    }
    return true;
}

}  // namespace

int main()
{
    return RunProbe() ? 0 : 1;
}
