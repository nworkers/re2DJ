#ifndef RE2DJ_HLE_IMPORT_EVENT_LOOP_H_
#define RE2DJ_HLE_IMPORT_EVENT_LOOP_H_

#include <cstdint>
#include <string>

#include "re2dj/hle/import_dispatcher.h"

namespace re2dj::hle
{

enum class ImportLoopTerminal
{
    kProcessExit,
    kFault,
    kThreadExit,
    kStopped,
};

struct ImportLoopResult
{
    ImportLoopTerminal terminal = ImportLoopTerminal::kStopped;
    runtime::ExecutionEvent event;
    std::uint32_t completed_imports = 0;
};

bool RunImportEventLoop(const runtime::LoadedPeImage& image,
                        const ImportDispatcher& dispatcher,
                        runtime::ExecutionBackend* backend,
                        std::uint32_t maximum_imports,
                        ImportLoopResult* result,
                        std::string* error);

}  // namespace re2dj::hle

#endif  // RE2DJ_HLE_IMPORT_EVENT_LOOP_H_
