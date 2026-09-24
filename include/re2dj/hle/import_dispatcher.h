#ifndef RE2DJ_HLE_IMPORT_DISPATCHER_H_
#define RE2DJ_HLE_IMPORT_DISPATCHER_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/runtime/execution_backend.h"

namespace re2dj::hle
{

class ImportCallServices
{
public:
    virtual ~ImportCallServices() = default;

    virtual bool ReadGuestString(runtime::GuestAddress address,
                                 std::string* value,
                                 std::string* error) const = 0;
    virtual runtime::GuestAddress FindGuestModule(std::string_view name) const = 0;
    virtual runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                                  std::string_view name) const = 0;
    virtual runtime::GuestAddress FindGuestExport(runtime::GuestAddress module,
                                                  std::uint16_t ordinal) const = 0;
};

enum class CallingConvention : std::uint8_t
{
    kStdcall,
    kCdecl,
};

struct ImportCall
{
    const runtime::ImportGate& gate;
    std::span<const std::uint32_t> arguments;
    const ImportCallServices* services = nullptr;
};

struct ImportReturn
{
    std::uint32_t eax = 0;
    std::uint32_t edx = 0;
    // The call does not return to the guest: the guest process ends with
    // exit_code, as kernel32!ExitProcess does. Each backend decides how.
    bool exit_process = false;
    std::uint32_t exit_code = 0;
};

using ImportHandler = bool (*)(const ImportCall& call,
                               ImportReturn* result,
                               std::string* error);

struct ImportBinding
{
    std::string module;
    std::string name;
    std::uint16_t ordinal = 0;
    bool by_ordinal = false;
    CallingConvention calling_convention = CallingConvention::kStdcall;
    std::uint32_t argument_count = 0;
    ImportHandler handler = nullptr;
};

class ImportDispatcher
{
public:
    static constexpr std::uint32_t kMaximumArgumentCount = 64;

    bool Register(ImportBinding binding, std::string* error);

    bool Dispatch(const runtime::ImportGate& gate,
                  const runtime::ExecutionEvent& event,
                  runtime::ExecutionBackend* backend,
                  std::string* error) const;

private:
    const ImportBinding* Find(const runtime::ImportGate& gate) const;

    std::vector<ImportBinding> bindings_;
};

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_IMPORT_DISPATCHER_H_
