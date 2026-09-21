#include "re2dj/hle/import_dispatcher.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "test_support.h"

namespace
{

class DispatcherBackend final : public re2dj::runtime::ExecutionBackend
{
public:
    bool PrepareImage(std::span<const std::uint8_t>,
                      const re2dj::exe::PeImageInfo&,
                      re2dj::runtime::GuestAddress,
                      re2dj::runtime::LoadedPeImage*,
                      std::string*) override
    {
        return false;
    }

    bool Start(std::string*) override
    {
        return false;
    }

    bool WaitForEvent(re2dj::runtime::ExecutionEvent*, std::string*) override
    {
        return false;
    }

    bool ReadMemory(re2dj::runtime::GuestAddress address,
                    std::span<std::uint8_t> bytes,
                    std::string* error) override
    {
        if (!read_succeeds_ || address != argument_address_ || bytes.size() != memory_.size())
        {
            if (error != nullptr)
            {
                *error = "dispatcher fake read failed";
            }
            return false;
        }
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            bytes[index] = memory_[index];
        }
        ++read_count_;
        return true;
    }

    bool WriteMemory(re2dj::runtime::GuestAddress,
                     std::span<const std::uint8_t>,
                     std::string*) override
    {
        return false;
    }

    bool AllocateGuestMemory(std::uint32_t,
                             re2dj::runtime::GuestMemoryAccess,
                             re2dj::runtime::GuestAddress*,
                             std::uint32_t*,
                             std::string*) override
    {
        return false;
    }

    bool ProtectGuestMemory(re2dj::runtime::GuestAddress,
                            std::uint32_t,
                            re2dj::runtime::GuestMemoryAccess,
                            re2dj::runtime::GuestMemoryAccess*,
                            std::string*) override
    {
        return false;
    }

    bool FreeGuestMemory(re2dj::runtime::GuestAddress, std::string*) override
    {
        return false;
    }

    bool CompleteImport(const re2dj::runtime::ImportCompletion& completion,
                        std::string*) override
    {
        completion_ = completion;
        ++completion_count_;
        return complete_succeeds_;
    }

    void RequestStop() override {}

    void SetArguments(re2dj::runtime::GuestAddress stack_pointer,
                      std::span<const std::uint32_t> arguments)
    {
        argument_address_ = stack_pointer + 4;
        memory_.resize(arguments.size() * sizeof(std::uint32_t));
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::uint32_t value = arguments[index];
            for (std::size_t byte_index = 0; byte_index < sizeof(value); ++byte_index)
            {
                memory_[index * sizeof(value) + byte_index] =
                    static_cast<std::uint8_t>(value >> (byte_index * 8));
            }
        }
    }

    void set_read_succeeds(bool value)
    {
        read_succeeds_ = value;
    }

    int read_count() const
    {
        return read_count_;
    }

    int completion_count() const
    {
        return completion_count_;
    }

    const re2dj::runtime::ImportCompletion& completion() const
    {
        return completion_;
    }

private:
    re2dj::runtime::GuestAddress argument_address_;
    std::vector<std::uint8_t> memory_;
    re2dj::runtime::ImportCompletion completion_;
    bool read_succeeds_ = true;
    bool complete_succeeds_ = true;
    int read_count_ = 0;
    int completion_count_ = 0;
};

bool ReturnFirstArgument(const re2dj::hle::ImportCall& call,
                         re2dj::hle::ImportReturn* result,
                         std::string* error)
{
    if (result == nullptr || call.arguments.empty())
    {
        if (error != nullptr)
        {
            *error = "synthetic handler needs one argument";
        }
        return false;
    }
    result->eax = call.arguments[0] + 1;
    result->edx = call.gate.by_ordinal ? 1 : 0;
    return true;
}

bool FailHandler(const re2dj::hle::ImportCall&,
                 re2dj::hle::ImportReturn*,
                 std::string* error)
{
    if (error != nullptr)
    {
        *error = "synthetic handler failure";
    }
    return false;
}

re2dj::runtime::ExecutionEvent MakeEvent(re2dj::runtime::GuestAddress stack_pointer,
                                         re2dj::runtime::GuestAddress gate_address)
{
    re2dj::runtime::ExecutionEvent event;
    event.kind = re2dj::runtime::ExecutionEventKind::kImportGate;
    event.event_id = 9;
    event.thread_id = 3;
    event.stack_pointer = stack_pointer;
    event.gate_address = gate_address;
    return event;
}

}  // namespace

void RunImportDispatcherTests(re2dj::test::Context& context)
{
    re2dj::hle::ImportDispatcher dispatcher;
    std::string error;

    re2dj::hle::ImportBinding named;
    named.module = "KERNEL32.DLL";
    named.name = "ProbeName";
    named.argument_count = 2;
    named.handler = ReturnFirstArgument;
    RE2DJ_CHECK(context, dispatcher.Register(named, &error));

    re2dj::hle::ImportBinding ordinal;
    ordinal.module = "kernel32.dll";
    ordinal.ordinal = 7;
    ordinal.by_ordinal = true;
    ordinal.calling_convention = re2dj::hle::CallingConvention::kCdecl;
    ordinal.argument_count = 1;
    ordinal.handler = ReturnFirstArgument;
    RE2DJ_CHECK(context, dispatcher.Register(ordinal, &error));
    RE2DJ_CHECK(context, !dispatcher.Register(named, &error));

    re2dj::hle::ImportBinding too_many = named;
    too_many.name = "TooMany";
    too_many.argument_count = re2dj::hle::ImportDispatcher::kMaximumArgumentCount + 1;
    RE2DJ_CHECK(context, !dispatcher.Register(too_many, &error));

    re2dj::hle::ImportBinding invalid_convention = named;
    invalid_convention.name = "InvalidConvention";
    invalid_convention.calling_convention =
        static_cast<re2dj::hle::CallingConvention>(99);
    RE2DJ_CHECK(context, !dispatcher.Register(invalid_convention, &error));

    DispatcherBackend backend;
    const re2dj::runtime::GuestAddress stack_pointer(0x2000);
    const re2dj::runtime::GuestAddress named_gate(0xF0000000);
    backend.SetArguments(stack_pointer, std::array<std::uint32_t, 2>{41, 99});

    re2dj::runtime::ImportGate named_gate_metadata;
    named_gate_metadata.module = "kernel32.dll";
    named_gate_metadata.name = "ProbeName";
    named_gate_metadata.address = named_gate;
    RE2DJ_CHECK(context,
                dispatcher.Dispatch(named_gate_metadata,
                                    MakeEvent(stack_pointer, named_gate),
                                    &backend,
                                    &error));
    RE2DJ_CHECK_EQ(context, backend.read_count(), 1);
    RE2DJ_CHECK_EQ(context, backend.completion_count(), 1);
    RE2DJ_CHECK_EQ(context, backend.completion().event_id, std::uint64_t{9});
    RE2DJ_CHECK_EQ(context, backend.completion().eax, std::uint32_t{42});
    RE2DJ_CHECK_EQ(context, backend.completion().edx, std::uint32_t{0});
    RE2DJ_CHECK_EQ(context, backend.completion().stack_bytes_to_pop, std::uint32_t{8});

    const re2dj::runtime::GuestAddress ordinal_gate(0xF0000010);
    backend.SetArguments(stack_pointer, std::array<std::uint32_t, 1>{42});
    re2dj::runtime::ImportGate ordinal_gate_metadata;
    ordinal_gate_metadata.module = "KeRnEl32.DlL";
    ordinal_gate_metadata.ordinal = 7;
    ordinal_gate_metadata.by_ordinal = true;
    ordinal_gate_metadata.address = ordinal_gate;
    RE2DJ_CHECK(context,
                dispatcher.Dispatch(ordinal_gate_metadata,
                                    MakeEvent(stack_pointer, ordinal_gate),
                                    &backend,
                                    &error));
    RE2DJ_CHECK_EQ(context, backend.completion_count(), 2);
    RE2DJ_CHECK_EQ(context, backend.completion().eax, std::uint32_t{43});
    RE2DJ_CHECK_EQ(context, backend.completion().edx, std::uint32_t{1});
    RE2DJ_CHECK_EQ(context, backend.completion().stack_bytes_to_pop, std::uint32_t{0});

    re2dj::runtime::ImportGate unknown = named_gate_metadata;
    unknown.name = "Missing";
    RE2DJ_CHECK(context,
                !dispatcher.Dispatch(unknown,
                                     MakeEvent(stack_pointer, named_gate),
                                     &backend,
                                     &error));
    RE2DJ_CHECK_EQ(context, backend.completion_count(), 2);

    backend.set_read_succeeds(false);
    RE2DJ_CHECK(context,
                !dispatcher.Dispatch(named_gate_metadata,
                                     MakeEvent(stack_pointer, named_gate),
                                     &backend,
                                     &error));
    RE2DJ_CHECK_EQ(context, backend.completion_count(), 2);
    backend.set_read_succeeds(true);

    RE2DJ_CHECK(context,
                !dispatcher.Dispatch(named_gate_metadata,
                                     MakeEvent(re2dj::runtime::GuestAddress(0xFFFFFFFF), named_gate),
                                     &backend,
                                     &error));
    RE2DJ_CHECK_EQ(context, backend.completion_count(), 2);

    re2dj::hle::ImportDispatcher failing_dispatcher;
    re2dj::hle::ImportBinding failing = named;
    failing.handler = FailHandler;
    RE2DJ_CHECK(context, failing_dispatcher.Register(failing, &error));
    backend.SetArguments(stack_pointer, std::array<std::uint32_t, 2>{41, 99});
    RE2DJ_CHECK(context,
                !failing_dispatcher.Dispatch(named_gate_metadata,
                                             MakeEvent(stack_pointer, named_gate),
                                             &backend,
                                             &error));
    RE2DJ_CHECK_EQ(context, backend.completion_count(), 2);
}
