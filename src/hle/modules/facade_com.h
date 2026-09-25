#ifndef RE2DJ_HLE_MODULES_FACADE_COM_H_
#define RE2DJ_HLE_MODULES_FACADE_COM_H_

// Helpers shared by facade modules whose exports include COM methods: the
// method table form, this-pointer checks, IUnknown's reference counting, and
// guest-memory results and callbacks. Internal to src/hle/modules.

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "re2dj/directx/abi.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/import_dispatcher.h"
#include "re2dj/hle/modules/guest_module.h"

namespace re2dj::hle::modules::com
{

// A COM method: its name, stdcall argument count including this, and handler.
struct Method
{
    std::string_view name;
    std::uint32_t argument_count;
    ImportHandler handler;
};

bool Fail(std::string* error, std::string text);
// Sets eax and clears the error; always true.
bool Succeed(ImportReturn* result, std::uint32_t value, std::string* error);
// The call's name for messages, such as "ddraw.dll!IDirectDraw7::GetCaps".
std::string CallName(const ImportCall& call);
std::string FormatGuid(const re2dj::directx::Guid& guid);

std::array<std::uint8_t, 4> Word(std::uint32_t value);
// A NUL-terminated copy of text at the end of bytes.
void AppendText(std::vector<std::uint8_t>* bytes, std::string_view text);

template <typename T>
std::vector<std::uint8_t> Bytes(const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    std::vector<std::uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    return bytes;
}

bool WriteBytes(const ImportCall& call, std::uint32_t address, std::span<const std::uint8_t> bytes, std::string* error);
bool WriteWord(const ImportCall& call, std::uint32_t address, std::uint32_t value, std::string* error);

template <typename T>
bool WriteStruct(const ImportCall& call, std::uint32_t address, const T& value, std::string* error)
{
    return WriteBytes(call, address, Bytes(value), error);
}

bool ReadGuid(const ImportCall& call, std::uint32_t address, re2dj::directx::Guid* guid, std::string* error);

// Places bytes in a process-heap block for the length of a callback, as a
// DLL's own static strings or stack structures would be; 0 on failure.
std::uint32_t PlaceTemporary(const ImportCall& call, GuestProcess& process, std::span<const std::uint8_t> bytes);

// Calls a guest callback with one structure as its first argument and the
// rest following; its answer comes back in *answer.
bool CallWithData(const ImportCall& call,
                  std::uint32_t callback,
                  std::vector<std::uint8_t> data,
                  std::span<const std::uint32_t> other_arguments,
                  std::uint32_t* answer,
                  std::string* error);

template <typename T>
bool CallWithStruct(const ImportCall& call,
                    std::uint32_t callback,
                    const T& value,
                    std::span<const std::uint32_t> other_arguments,
                    std::uint32_t* answer,
                    std::string* error)
{
    return CallWithData(call, callback, Bytes(value), other_arguments, answer, error);
}

// The process, once the call's shape and its this pointer check out: the
// argument count is right and this is a facade object of the kind given (0
// for any kind). Otherwise null, with error set.
GuestProcess* MethodProcess(const ImportCall& call,
                            ImportReturn* result,
                            std::size_t argument_count,
                            std::uint32_t kind,
                            std::string* error);

// IUnknown::AddRef and Release for any facade object.
bool AddRef(const ImportCall& call, ImportReturn* result, std::string* error);
bool Release(const ImportCall& call, ImportReturn* result, std::string* error);

std::vector<std::string_view> MethodNames(std::span<const Method> methods);

// Creates an object of the interface whose methods are the module's exports
// named "<interface>::<method>"; 0 with error set on failure.
std::uint32_t CreateObject(const ImportCall& call,
                           GuestProcess& process,
                           std::string_view module,
                           std::string_view interface_name,
                           std::span<const Method> methods,
                           GuestComObject object,
                           std::string* error);

GuestExportDescriptor MakeExport(std::string name, std::uint32_t argument_count, ImportHandler handler);
// Adds the interface's methods to the module as "<interface>::<method>".
void AddMethods(GuestModuleDescriptor* descriptor, std::string_view interface_name, std::span<const Method> methods);

}  // namespace re2dj::hle::modules::com

#endif  // RE2DJ_HLE_MODULES_FACADE_COM_H_
