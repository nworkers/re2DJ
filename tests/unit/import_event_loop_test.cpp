#include "re2dj/hle/import_event_loop.h"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "test_support.h"

namespace
{

class EventLoopBackend final : public re2dj::runtime::ExecutionBackend
{
public:
    explicit EventLoopBackend(std::vector<re2dj::runtime::ExecutionEvent> events)
        : events_(std::move(events))
    {
    }

    bool PrepareImage(std::span<const std::uint8_t>, const re2dj::exe::PeImageInfo&,
                      re2dj::runtime::GuestAddress, re2dj::runtime::LoadedPeImage*,
                      std::string*) override { return false; }
    bool Start(std::string*) override { return false; }
    bool WaitForEvent(re2dj::runtime::ExecutionEvent* event, std::string*) override
    {
        if (event == nullptr || next_event_ == events_.size()) return false;
        *event = events_[next_event_++];
        return true;
    }
    bool ReadMemory(re2dj::runtime::GuestAddress, std::span<std::uint8_t> bytes,
                    std::string*) override
    {
        if (bytes.size() != 4) return false;
        bytes[0] = 10; bytes[1] = 0; bytes[2] = 0; bytes[3] = 0;
        return true;
    }
    bool WriteMemory(re2dj::runtime::GuestAddress, std::span<const std::uint8_t>,
                     std::string*) override { return false; }
    bool AllocateGuestMemory(std::uint32_t, re2dj::runtime::GuestMemoryAccess,
                             re2dj::runtime::GuestAddress*, std::uint32_t*,
                             std::string*) override { return false; }
    bool ProtectGuestMemory(re2dj::runtime::GuestAddress, std::uint32_t,
                            re2dj::runtime::GuestMemoryAccess,
                            re2dj::runtime::GuestMemoryAccess*, std::string*) override { return false; }
    bool FreeGuestMemory(re2dj::runtime::GuestAddress, std::string*) override { return false; }
    bool CompleteImport(const re2dj::runtime::ImportCompletion& completion,
                        std::string*) override { completions_.push_back(completion); return true; }
    void RequestStop() override { stopped_ = true; }
    std::size_t completion_count() const { return completions_.size(); }
    bool stopped() const { return stopped_; }

private:
    std::vector<re2dj::runtime::ExecutionEvent> events_;
    std::vector<re2dj::runtime::ImportCompletion> completions_;
    std::size_t next_event_ = 0;
    bool stopped_ = false;
};

bool ReturnArgument(const re2dj::hle::ImportCall& call, re2dj::hle::ImportReturn* result,
                    std::string*)
{
    if (result == nullptr || call.arguments.size() != 1) return false;
    result->eax = call.arguments[0] + 1;
    return true;
}

re2dj::runtime::ExecutionEvent ImportEvent(std::uint64_t id, std::uint32_t address)
{
    re2dj::runtime::ExecutionEvent event;
    event.kind = re2dj::runtime::ExecutionEventKind::kImportGate;
    event.event_id = id;
    event.stack_pointer = re2dj::runtime::GuestAddress(0x2000);
    event.gate_address = re2dj::runtime::GuestAddress(address);
    return event;
}

}  // namespace

void RunImportEventLoopTests(re2dj::test::Context& context)
{
    re2dj::runtime::LoadedPeImage image;
    image.imports = {{"probe.dll", "First", 0, false,
                      re2dj::runtime::GuestAddress(0xF0000000)},
                     {"probe.dll", "Second", 0, false,
                      re2dj::runtime::GuestAddress(0xF0000010)}};
    re2dj::hle::ImportDispatcher dispatcher;
    std::string error;
    for (const char* name : {"First", "Second"})
    {
        re2dj::hle::ImportBinding binding;
        binding.module = "probe.dll";
        binding.name = name;
        binding.argument_count = 1;
        binding.handler = ReturnArgument;
        RE2DJ_CHECK(context, dispatcher.Register(std::move(binding), &error));
    }
    re2dj::runtime::ExecutionEvent exit_event;
    exit_event.kind = re2dj::runtime::ExecutionEventKind::kProcessExit;
    exit_event.status_code = 51;
    EventLoopBackend backend({ImportEvent(1, 0xF0000000), ImportEvent(2, 0xF0000010), exit_event});
    re2dj::hle::ImportLoopResult result;
    RE2DJ_CHECK(context, re2dj::hle::RunImportEventLoop(image, dispatcher, &backend, 2, &result, &error));
    RE2DJ_CHECK_EQ(context, result.completed_imports, std::uint32_t{2});
    RE2DJ_CHECK(context, result.terminal == re2dj::hle::ImportLoopTerminal::kProcessExit);
    RE2DJ_CHECK_EQ(context, result.event.status_code, std::uint32_t{51});
    RE2DJ_CHECK_EQ(context, backend.completion_count(), std::size_t{2});
    RE2DJ_CHECK(context, !backend.stopped());

    EventLoopBackend missing_backend({ImportEvent(1, 0xF0000000)});
    re2dj::hle::ImportDispatcher missing_dispatcher;
    RE2DJ_CHECK(context, !re2dj::hle::RunImportEventLoop(image, missing_dispatcher,
                                                          &missing_backend, 2, &result, &error));
    RE2DJ_CHECK(context, missing_backend.stopped());
}
