#include "native_dynamic_thunk.h"
#include "native_host_services.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "native_import_bridge.h"
#include "native_low_memory.h"

namespace re2dj::platform::native
{
namespace
{

constexpr std::uint32_t kThunkBytes = 19;

void WriteU32(std::uint8_t* bytes, std::uint32_t value)
{
    for (std::size_t index = 0; index < sizeof(value); ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

bool FitsSigned32(std::intptr_t value)
{
    return value >= (std::numeric_limits<std::int32_t>::min)() &&
           value <= (std::numeric_limits<std::int32_t>::max)();
}

}  // namespace

bool CreateNativeDynamicThunk(std::uint32_t gate_address,
                              NativeDynamicThunk* thunk,
                              std::string* error)
{
    if (thunk == nullptr || error == nullptr || thunk->memory != nullptr || gate_address == 0)
    {
        if (error != nullptr)
        {
            *error = "invalid native dynamic thunk arguments";
        }
        return false;
    }

    // The guest calls the thunk, so it must be guest-addressable.
    NativeLowMemory mapping;
    if (!MapNativeLowMemory(kThunkBytes, HostProtection::kReadWrite, &mapping, error))
    {
        *error = "cannot allocate native dynamic thunk: " + *error;
        return false;
    }
    void* memory = mapping.memory;

    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(memory);
    const std::uintptr_t bridge = NativeImportGateBridgeAddress();
    const std::uintptr_t cleanup = NativeImportGateCleanupAddress();
    const std::intptr_t call_displacement =
        static_cast<std::intptr_t>(bridge) - static_cast<std::intptr_t>(address + 10);
    if (address > (std::numeric_limits<std::uint32_t>::max)() ||
        cleanup > (std::numeric_limits<std::uint32_t>::max)() ||
        !FitsSigned32(call_displacement))
    {
        HostUnmap(memory, kThunkBytes);
        *error = "native dynamic thunk address is outside the i386 ABI range";
        return false;
    }

    auto* bytes = static_cast<std::uint8_t*>(memory);
    bytes[0] = 0x68;
    WriteU32(bytes + 1, gate_address);
    bytes[5] = 0xE8;
    WriteU32(bytes + 6, static_cast<std::uint32_t>(call_displacement));
    bytes[10] = 0x59;
    bytes[11] = 0x03;
    bytes[12] = 0x25;
    WriteU32(bytes + 13, static_cast<std::uint32_t>(cleanup));
    bytes[17] = 0xFF;
    bytes[18] = 0xE1;

    if (!HostProtect(memory, kThunkBytes, HostProtection::kReadExecute))
    {
        HostUnmap(memory, kThunkBytes);
        *error = "cannot protect native dynamic thunk";
        return false;
    }
    HostFlushCode(memory, kThunkBytes);
    thunk->memory = memory;
    thunk->address = static_cast<std::uint32_t>(address);
    thunk->size = kThunkBytes;
    error->clear();
    return true;
}

void ReleaseNativeDynamicThunk(NativeDynamicThunk* thunk)
{
    if (thunk != nullptr && thunk->memory != nullptr)
    {
        HostUnmap(thunk->memory, thunk->size);
        *thunk = {};
    }
}

}  // namespace re2dj::platform::native
