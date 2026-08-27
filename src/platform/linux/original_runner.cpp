#include "re2dj/platform/linux/original_runner.h"

#include <fstream>
#include <iterator>
#include <vector>

#include "re2dj/platform/linux/native_helper_backend.h"
#include "re2dj/runtime/execution_backend.h"
#include "re2dj/runtime/pe_loader.h"

namespace re2dj::platform::linux
{
namespace
{

const runtime::ImportGate* FindImport(const runtime::LoadedPeImage& image,
                                      runtime::GuestAddress gate_address)
{
    for (const runtime::ImportGate& gate : image.imports)
    {
        if (gate.address == gate_address)
        {
            return &gate;
        }
    }
    return nullptr;
}

bool ReadExecutable(const std::filesystem::path& path,
                    std::vector<std::uint8_t>* bytes,
                    std::string* error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        *error = "cannot open the selected guest executable";
        return false;
    }
    bytes->assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (!stream.eof() || bytes->empty())
    {
        *error = "cannot read the selected guest executable";
        return false;
    }
    return true;
}

}  // namespace

bool RunOriginalUntilBoundary(const std::filesystem::path& executable_path,
                              const exe::PeImageInfo& image_info,
                              const std::filesystem::path& helper_path,
                              OriginalRunResult* result,
                              std::string* error)
{
    if (executable_path.empty() || helper_path.empty() || result == nullptr || error == nullptr)
    {
        if (error != nullptr)
        {
            *error = "invalid Linux original-run arguments";
        }
        return false;
    }

    *result = OriginalRunResult{};

    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error))
    {
        return false;
    }

    NativeHelperBackend backend(helper_path);
    runtime::LoadedPeImage loaded;
    if (!backend.PrepareImage(file_bytes, image_info, runtime::GuestAddress(), &loaded, error) ||
        !backend.Start(error))
    {
        return false;
    }

    runtime::ExecutionEvent event;
    if (!backend.WaitForEvent(&event, error))
    {
        return false;
    }

    result->load_base = loaded.load_base;
    result->entry_point = loaded.entry_point;
    result->instruction_pointer = event.instruction_pointer;
    result->stack_pointer = event.stack_pointer;
    result->gate_address = event.gate_address;
    result->status_code = event.status_code;

    switch (event.kind)
    {
    case runtime::ExecutionEventKind::kImportGate:
    {
        result->boundary = OriginalRunBoundary::kImportGate;
        const runtime::ImportGate* gate = FindImport(loaded, event.gate_address);
        if (gate == nullptr)
        {
            backend.RequestStop();
            *error = "helper reported an unknown import gate address";
            return false;
        }
        result->module = gate->module;
        result->name = gate->name;
        result->by_ordinal = gate->by_ordinal;
        result->ordinal = gate->ordinal;
        backend.RequestStop();
        break;
    }
    case runtime::ExecutionEventKind::kProcessExit:
        result->boundary = OriginalRunBoundary::kProcessExit;
        break;
    case runtime::ExecutionEventKind::kFault:
        result->boundary = OriginalRunBoundary::kFault;
        backend.RequestStop();
        break;
    case runtime::ExecutionEventKind::kThreadExit:
    case runtime::ExecutionEventKind::kStopped:
        result->boundary = OriginalRunBoundary::kStopped;
        backend.RequestStop();
        break;
    }

    error->clear();
    return true;
}

}  // namespace re2dj::platform::linux
