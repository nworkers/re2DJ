#ifndef RE2DJ_TESTS_UNIT_MEMORY_SERVICES_H_
#define RE2DJ_TESTS_UNIT_MEMORY_SERVICES_H_

#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "re2dj/hle/guest_devices.h"
#include "re2dj/hle/guest_files.h"
#include "re2dj/hle/guest_process.h"
#include "re2dj/hle/import_dispatcher.h"
#include "re2dj/hle/modules/guest_module.h"

#include "test_support.h"

namespace re2dj::test
{

// Guest memory at one fixed range holding the heap and the VirtualAlloc
// arena, a device set sharing the guest process's handles, a last-error slot,
// and one known module: just enough of a host for facade exports.
class MemoryServices final : public hle::ImportCallServices
{
public:
    static constexpr std::uint32_t kBase = 0x00010000U;
    static constexpr std::uint32_t kSize = 0x40000U;
    static constexpr std::uint32_t kHeapBase = kBase + 0x800U;
    static constexpr std::uint32_t kHeapSize = 0x800U;
    // The VirtualAlloc arena, starting on a 64 KiB boundary.
    static constexpr std::uint32_t kArenaBase = kBase + 0x10000U;
    static constexpr std::uint32_t kArenaSize = 0x30000U;
    // Where CallGuest places a call's data, one kGuestCallDataSize slot per
    // nesting level.
    static constexpr std::uint32_t kGuestCallData = kBase + 0x100U;
    static constexpr std::uint32_t kGuestCallDataSize = 0x100U;
    static constexpr std::uint32_t kGuestCallDepth = 6;
    // The module FindGuestModule reports for kKnownModule.
    static constexpr std::uint32_t kModule = 0x6E000000U;
    static constexpr std::string_view kKnownModule = "advapi32.dll";

    explicit MemoryServices(hle::GuestDeviceConfig config = {})
        : memory_(kSize, 0), devices_(std::move(config))
    {
        process_.SetHeapRegion(kHeapBase, kHeapSize);
        process_.SetPrivateArena(kArenaBase, kArenaSize);
        devices_.SetHandleAllocator(&process_.handles());
    }

    void Put(std::uint32_t address, std::string_view text)
    {
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            memory_[address - kBase + index] = static_cast<std::uint8_t>(text[index]);
        }
        memory_[address - kBase + text.size()] = 0;
    }
    void PutU32(std::uint32_t address, std::uint32_t value)
    {
        std::memcpy(memory_.data() + (address - kBase), &value, sizeof(value));
    }
    std::uint8_t& Byte(std::uint32_t address) { return memory_[address - kBase]; }
    std::uint32_t U32(std::uint32_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, memory_.data() + (address - kBase), sizeof(value));
        return value;
    }

    bool ReadGuestString(runtime::GuestAddress address,
                         std::string* value,
                         std::string* error) const override
    {
        value->clear();
        for (std::uint32_t at = address.value(); Contains(at, 1); ++at)
        {
            const char character = static_cast<char>(memory_[at - kBase]);
            if (character == '\0')
            {
                return !value->empty();
            }
            value->push_back(character);
        }
        *error = "outside test memory";
        return false;
    }
    // A second module whose exports the test lists by name, such as a facade
    // module whose COM methods fill vtables.
    static constexpr std::uint32_t kExtraModule = 0x6F000000U;
    std::string extra_module;
    std::map<std::string, std::uint32_t, std::less<>> extra_exports;

    runtime::GuestAddress FindGuestModule(std::string_view name) const override
    {
        if (!extra_module.empty() && name == extra_module)
        {
            return runtime::GuestAddress(kExtraModule);
        }
        return runtime::GuestAddress(name == kKnownModule ? kModule : 0U);
    }
    runtime::GuestAddress FindGuestExport(runtime::GuestAddress module, std::string_view name) const override
    {
        const auto found = extra_exports.find(name);
        return module.value() == kExtraModule && found != extra_exports.end()
                   ? runtime::GuestAddress(found->second)
                   : runtime::GuestAddress();
    }
    runtime::GuestAddress FindGuestExport(runtime::GuestAddress, std::uint16_t) const override
    {
        return {};
    }
    bool IsGuestModule(runtime::GuestAddress handle) const override
    {
        return handle.value() == kModule;
    }
    std::string GuestModuleName(runtime::GuestAddress handle) const override
    {
        return handle.value() == kModule ? std::string(kKnownModule) : std::string();
    }
    bool ReadGuestBytes(runtime::GuestAddress address,
                        std::span<std::uint8_t> bytes,
                        std::string* error) const override
    {
        if (!Contains(address.value(), bytes.size()))
        {
            *error = "outside test memory";
            return false;
        }
        std::memcpy(bytes.data(), memory_.data() + (address.value() - kBase), bytes.size());
        return true;
    }
    bool WriteGuestBytes(runtime::GuestAddress address,
                         std::span<const std::uint8_t> bytes,
                         std::string* error) const override
    {
        if (!Contains(address.value(), bytes.size()))
        {
            *error = "outside test memory";
            return false;
        }
        std::memcpy(memory_.data() + (address.value() - kBase), bytes.data(), bytes.size());
        return true;
    }
    hle::GuestDeviceSet* Devices() const override { return &devices_; }
    // The host presentation the test provides; none by default.
    hle::HostPresentation* presentation = nullptr;
    hle::HostPresentation* Presentation() const override { return presentation; }
    hle::GuestProcess* Process() const override { return &process_; }
    // Guest files the test provides; none by default.
    void SetFiles(hle::GuestFiles* files) { files_ = files; }
    hle::GuestFiles* Files() const override { return files_; }
    // A fixed clock the test sets; absent until it does.
    void SetClock(hle::GuestClockReading reading)
    {
        clock_ = reading;
        clock_set_ = true;
    }
    bool ReadClock(hle::GuestClockReading* reading) const override
    {
        if (!clock_set_)
        {
            return false;
        }
        *reading = clock_;
        return true;
    }
    void SetLastError(std::uint32_t value) const override { last_error_ = value; }
    std::uint32_t LastError() const override { return last_error_; }

    // Stands in for the guest's functions: every CallGuest is kept in
    // guest_calls (arguments as the guest saw them) and answered by
    // guest_function, 0 when none is set. The data goes to test memory.
    std::function<std::uint32_t(const std::vector<std::uint32_t>& arguments)> guest_function;
    mutable std::vector<std::vector<std::uint32_t>> guest_calls;
    bool CallGuest(hle::GuestCall* call, std::uint32_t* result, std::string* error) const override
    {
        if (call->data.size() > kGuestCallDataSize || depth_ >= kGuestCallDepth)
        {
            *error = "test guest call is too large";
            return false;
        }
        std::vector<std::uint32_t> arguments = call->arguments;
        const std::uint32_t data_address = kGuestCallData + depth_ * kGuestCallDataSize;
        if (!call->data.empty())
        {
            std::memcpy(memory_.data() + (data_address - kBase), call->data.data(), call->data.size());
            arguments[static_cast<std::size_t>(call->data_argument)] = data_address;
        }
        guest_calls.push_back(arguments);
        ++depth_;
        *result = guest_function ? guest_function(arguments) : 0;
        --depth_;
        if (!call->data.empty())
        {
            std::memcpy(call->data.data(), memory_.data() + (data_address - kBase), call->data.size());
        }
        return true;
    }

private:
    bool Contains(std::uint32_t address, std::size_t size) const
    {
        return address >= kBase && address - kBase <= memory_.size() &&
               size <= memory_.size() - (address - kBase);
    }

    mutable std::vector<std::uint8_t> memory_;
    mutable hle::GuestDeviceSet devices_;
    mutable hle::GuestProcess process_;
    mutable std::uint32_t last_error_ = 0;
    mutable std::uint32_t depth_ = 0;
    hle::GuestFiles* files_ = nullptr;
    hle::GuestClockReading clock_;
    bool clock_set_ = false;
};

// Calls the named export of descriptor with the given arguments. handled
// receives the handler's result and error its message.
inline hle::ImportReturn CallModuleExport(Context& context,
                                          const MemoryServices& services,
                                          const hle::modules::GuestModuleDescriptor& descriptor,
                                          std::string_view name,
                                          std::initializer_list<std::uint32_t> arguments,
                                          bool* handled = nullptr,
                                          std::string* error = nullptr)
{
    hle::ImportReturn result;
    for (const auto& export_descriptor : descriptor.exports)
    {
        if (export_descriptor.name != name)
        {
            continue;
        }
        runtime::ImportGate gate;
        gate.module = descriptor.name;
        gate.name = export_descriptor.name;
        const std::vector<std::uint32_t> values(arguments);
        const hle::ImportCall call{gate, values, &services};
        std::string message;
        const bool ok = export_descriptor.handler(call, &result, &message);
        if (handled != nullptr)
        {
            *handled = ok;
        }
        else
        {
            RE2DJ_CHECK(context, ok);
        }
        if (error != nullptr)
        {
            *error = message;
        }
        return result;
    }
    RE2DJ_CHECK(context, false);
    return result;
}

}  // namespace re2dj::test

#endif  // RE2DJ_TESTS_UNIT_MEMORY_SERVICES_H_
