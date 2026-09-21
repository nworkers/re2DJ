#include "re2dj/hle/import_event_loop.h"

namespace re2dj::hle
{
namespace
{

const runtime::ImportGate* FindGate(const runtime::LoadedPeImage& image,
                                    runtime::GuestAddress address)
{
    for (const runtime::ImportGate& gate : image.imports)
    {
        if (gate.address == address)
        {
            return &gate;
        }
    }
    return nullptr;
}

void SetError(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

}  // namespace

bool RunImportEventLoop(const runtime::LoadedPeImage& image,
                        const ImportDispatcher& dispatcher,
                        runtime::ExecutionBackend* backend,
                        std::uint32_t maximum_imports,
                        ImportLoopResult* result,
                        std::string* error)
{
    if (backend == nullptr || result == nullptr || maximum_imports == 0)
    {
        SetError(error, "invalid import event loop arguments");
        return false;
    }
    *result = {};
    for (;;)
    {
        runtime::ExecutionEvent event;
        if (!backend->WaitForEvent(&event, error))
        {
            return false;
        }
        if (event.kind != runtime::ExecutionEventKind::kImportGate)
        {
            result->event = event;
            result->terminal = event.kind == runtime::ExecutionEventKind::kProcessExit
                                   ? ImportLoopTerminal::kProcessExit
                                   : event.kind == runtime::ExecutionEventKind::kFault
                                         ? ImportLoopTerminal::kFault
                                         : event.kind == runtime::ExecutionEventKind::kThreadExit
                                               ? ImportLoopTerminal::kThreadExit
                                               : ImportLoopTerminal::kStopped;
            return true;
        }
        if (result->completed_imports == maximum_imports)
        {
            backend->RequestStop();
            SetError(error, "import event loop reached its import limit");
            return false;
        }
        const runtime::ImportGate* gate = FindGate(image, event.gate_address);
        if (gate == nullptr)
        {
            backend->RequestStop();
            SetError(error, "helper reported an unknown import gate address");
            return false;
        }
        if (!dispatcher.Dispatch(*gate, event, backend, error))
        {
            backend->RequestStop();
            return false;
        }
        ++result->completed_imports;
    }
}

}  // namespace re2dj::hle
