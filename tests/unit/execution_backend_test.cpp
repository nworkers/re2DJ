#include "re2dj/runtime/execution_backend.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "test_support.h"

namespace
{

class FakeExecutionBackend final : public re2dj::runtime::ExecutionBackend
{
public:
    bool PrepareImage(std::span<const std::uint8_t> file_bytes,
                      const re2dj::exe::PeImageInfo& info,
                      re2dj::runtime::GuestAddress requested_base,
                      re2dj::runtime::LoadedPeImage* loaded,
                      std::string* error) override
    {
        if (file_bytes.empty() || loaded == nullptr)
        {
            if (error != nullptr)
            {
                *error = "invalid fake image";
            }
            return false;
        }
        loaded->load_base = requested_base;
        loaded->entry_point = requested_base + info.entry_point_rva;
        prepared_ = true;
        return true;
    }

    bool Start(std::string* error) override
    {
        if (!prepared_)
        {
            if (error != nullptr)
            {
                *error = "fake image is not prepared";
            }
            return false;
        }
        started_ = true;
        return true;
    }

    bool WaitForEvent(re2dj::runtime::ExecutionEvent* event, std::string*) override
    {
        if (!started_ || event == nullptr)
        {
            return false;
        }
        event->kind = re2dj::runtime::ExecutionEventKind::kImportGate;
        event->event_id = 7;
        event->thread_id = 2;
        event->gate_address = re2dj::runtime::GuestAddress(0xF0000010);
        return true;
    }

    bool ReadMemory(re2dj::runtime::GuestAddress address,
                    std::span<std::uint8_t> bytes,
                    std::string*) override
    {
        if (address.value() != 0x1000 || bytes.size() != memory_.size())
        {
            return false;
        }
        std::copy(memory_.begin(), memory_.end(), bytes.begin());
        return true;
    }

    bool WriteMemory(re2dj::runtime::GuestAddress address,
                     std::span<const std::uint8_t> bytes,
                     std::string*) override
    {
        if (address.value() != 0x1000 || bytes.size() != memory_.size())
        {
            return false;
        }
        std::copy(bytes.begin(), bytes.end(), memory_.begin());
        return true;
    }

    bool CompleteImport(const re2dj::runtime::ImportCompletion& completion,
                        std::string*) override
    {
        completed_ = completion.event_id == 7 && completion.eax == 42 &&
                     completion.stack_bytes_to_pop == 4 &&
                     completion.action ==
                         re2dj::runtime::ImportCompletionAction::kContinue;
        return completed_;
    }

    void RequestStop() override
    {
        stopped_ = true;
    }

    bool completed() const
    {
        return completed_;
    }

    bool stopped() const
    {
        return stopped_;
    }

private:
    bool prepared_ = false;
    bool started_ = false;
    bool completed_ = false;
    bool stopped_ = false;
    std::array<std::uint8_t, 4> memory_ = {};
};

}  // namespace

void RunExecutionBackendTests(re2dj::test::Context& context)
{
    FakeExecutionBackend backend;
    re2dj::exe::PeImageInfo info;
    info.entry_point_rva = 0x1000;
    const std::vector<std::uint8_t> bytes = {1};
    re2dj::runtime::LoadedPeImage loaded;
    std::string error;
    RE2DJ_CHECK(context,
                backend.PrepareImage(bytes,
                                     info,
                                     re2dj::runtime::GuestAddress(0x00400000),
                                     &loaded,
                                     &error));
    RE2DJ_CHECK_EQ(context, loaded.entry_point.value(), std::uint32_t{0x00401000});
    RE2DJ_CHECK(context, backend.Start(&error));

    re2dj::runtime::ExecutionEvent event;
    RE2DJ_CHECK(context, backend.WaitForEvent(&event, &error));
    RE2DJ_CHECK(context,
                event.kind == re2dj::runtime::ExecutionEventKind::kImportGate);
    RE2DJ_CHECK_EQ(context, event.event_id, std::uint64_t{7});
    RE2DJ_CHECK_EQ(context, event.thread_id, std::uint32_t{2});

    const std::array<std::uint8_t, 4> written = {1, 2, 3, 4};
    std::array<std::uint8_t, 4> read = {};
    RE2DJ_CHECK(context,
                backend.WriteMemory(re2dj::runtime::GuestAddress(0x1000),
                                    written,
                                    &error));
    RE2DJ_CHECK(context,
                backend.ReadMemory(re2dj::runtime::GuestAddress(0x1000),
                                   read,
                                   &error));
    RE2DJ_CHECK_EQ(context, read, written);

    re2dj::runtime::ImportCompletion completion;
    completion.event_id = event.event_id;
    completion.eax = 42;
    completion.stack_bytes_to_pop = 4;
    RE2DJ_CHECK(context, backend.CompleteImport(completion, &error));
    RE2DJ_CHECK(context, backend.completed());
    backend.RequestStop();
    RE2DJ_CHECK(context, backend.stopped());
}
