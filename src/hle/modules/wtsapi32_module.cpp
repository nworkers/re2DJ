#include "re2dj/hle/modules/wtsapi32_module.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "re2dj/hle/guest_process.h"

namespace re2dj::hle::modules
{
namespace
{

void SetError(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

std::array<std::uint8_t, 4> DwordBytes(std::uint32_t value)
{
    return {static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16),
            static_cast<std::uint8_t>(value >> 24)};
}

// WTSQuerySessionInformationA(hServer, SessionId, WTSInfoClass, ppBuffer,
// pBytesReturned) for the current session's ID only. Any other request fails
// the handler instead of returning a guessed value.
bool WTSQuerySessionInformationA(const ImportCall& call,
                                 ImportReturn* result,
                                 std::string* error)
{
    if (result == nullptr || call.arguments.size() != 5)
    {
        SetError(error, result == nullptr ? "wtsapi32 result is null"
                                          : "wtsapi32 WTSQuerySessionInformationA argument shape is invalid");
        return false;
    }
    *result = {};
    const std::uint32_t server = call.arguments[0];
    const std::uint32_t session = call.arguments[1];
    const std::uint32_t info_class = call.arguments[2];
    const std::uint32_t buffer_slot = call.arguments[3];
    const std::uint32_t bytes_slot = call.arguments[4];
    if (server != kWtsCurrentServerHandle ||
        (session != kWtsCurrentSession && session != kWtsGuestSessionId) ||
        info_class != kWtsSessionId || buffer_slot == 0 || bytes_slot == 0)
    {
        SetError(error, "wtsapi32 WTSQuerySessionInformationA answers only the current session's ID");
        return false;
    }
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        SetError(error, "wtsapi32 WTSQuerySessionInformationA needs the guest process");
        return false;
    }
    const std::uint32_t block = process->Allocate(4);
    if (block == 0)
    {
        SetError(error, "wtsapi32 WTSQuerySessionInformationA found no guest heap room");
        return false;
    }
    std::string memory_error;
    if (!call.services->WriteGuestBytes(runtime::GuestAddress(block),
                                        DwordBytes(kWtsGuestSessionId),
                                        &memory_error) ||
        !call.services->WriteGuestBytes(runtime::GuestAddress(buffer_slot),
                                        DwordBytes(block),
                                        &memory_error) ||
        !call.services->WriteGuestBytes(runtime::GuestAddress(bytes_slot),
                                        DwordBytes(4),
                                        &memory_error))
    {
        process->Free(block);
        if (error != nullptr)
        {
            *error = "wtsapi32 WTSQuerySessionInformationA: " + memory_error;
        }
        return false;
    }
    result->eax = 1;
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

// WTSFreeMemory(pMemory) returns nothing; freeing an unknown block fails the
// handler, since Windows would corrupt its heap there.
bool WTSFreeMemory(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (result == nullptr || call.arguments.size() != 1)
    {
        SetError(error, result == nullptr ? "wtsapi32 result is null"
                                          : "wtsapi32 WTSFreeMemory argument shape is invalid");
        return false;
    }
    *result = {};
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr || !process->Free(call.arguments[0]))
    {
        SetError(error, "wtsapi32 WTSFreeMemory got no live guest heap block");
        return false;
    }
    if (error != nullptr)
    {
        error->clear();
    }
    return true;
}

GuestExportDescriptor MakeExport(std::string name,
                                 std::uint32_t argument_count,
                                 ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

}  // namespace

GuestModuleDescriptor MakeWtsapi32ModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "wtsapi32.dll";
    descriptor.aliases = {"wtsapi32"};
    descriptor.exports.push_back(
        MakeExport("WTSQuerySessionInformationA", 5, &WTSQuerySessionInformationA));
    descriptor.exports.push_back(MakeExport("WTSFreeMemory", 1, &WTSFreeMemory));
    return descriptor;
}

}  // namespace re2dj::hle::modules
