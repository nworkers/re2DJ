#include "re2dj/platform/linux/original_runner.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <signal.h>

#if defined(__i386__)
#include "native_dynamic_thunk.h"
#include "native_in_process_runner.h"
#endif

namespace re2dj::platform::linux
{
namespace
{

#if defined(__i386__)
constexpr std::size_t kMaximumObservedPathLength = 260;

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
    if (stream.bad() || bytes->empty())
    {
        *error = "cannot read the selected guest executable";
        return false;
    }
    return true;
}

struct CreateFileContext
{
    std::uint32_t image_base = 0;
    std::uint32_t image_size = 0;
    std::uint32_t get_version_thunk = 0;
    std::uint32_t create_file_thunk = 0;
    bool module_resolved = false;
    bool get_version_resolved = false;
    bool create_file_resolved = false;
    bool create_file_called = false;
    std::uint32_t return_address = 0;
    OriginalCreateFileObservation observation;
};

bool ImageContains(const CreateFileContext& context,
                   std::uint32_t address,
                   std::size_t size)
{
    if (address < context.image_base)
    {
        return false;
    }
    const std::uint32_t offset = address - context.image_base;
    return offset <= context.image_size && size <= context.image_size - offset;
}

bool TextEquals(const CreateFileContext& context,
                std::uint32_t address,
                const char* expected)
{
    const std::size_t length = std::strlen(expected) + 1;
    return ImageContains(context, address, length) &&
           std::memcmp(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
                       expected,
                       length) == 0;
}

bool StackContains(const NativeImportGateEvent& event,
                   std::uint32_t address,
                   std::size_t size)
{
    return address >= event.guest_stack_limit && address <= event.guest_stack_base &&
           size <= event.guest_stack_base - address;
}

bool CopyGuestString(const CreateFileContext& context,
                     const NativeImportGateEvent& event,
                     std::uint32_t address,
                     std::string* value)
{
    if (value == nullptr)
    {
        return false;
    }
    std::size_t available = 0;
    if (ImageContains(context, address, 1))
    {
        available = static_cast<std::size_t>(
            context.image_size - (address - context.image_base));
    }
    else if (StackContains(event, address, 1))
    {
        available = static_cast<std::size_t>(event.guest_stack_base - address);
    }
    else
    {
        return false;
    }
    const std::size_t limit = (std::min)(available, kMaximumObservedPathLength + 1);
    const char* text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
    const void* terminator = std::memchr(text, '\0', limit);
    if (terminator == nullptr)
    {
        return false;
    }
    value->assign(text, static_cast<const char*>(terminator));
    return true;
}

void CopyFaultObservation(const NativeInProcessRunResult& source,
                          OriginalRunResult* destination)
{
    if (destination == nullptr || source.fault.status_code == 0)
    {
        return;
    }
    OriginalFaultObservation& observation = destination->fault_observation;
    observation.observed = true;
    observation.fault_address = runtime::GuestAddress(source.fault.fault_address);
    observation.signal_code = source.fault.signal_code;
    observation.cpu_error_code = source.fault.cpu_error_code;
    observation.eax = source.fault.eax;
    observation.ebx = source.fault.ebx;
    observation.ecx = source.fault.ecx;
    observation.edx = source.fault.edx;
    observation.esi = source.fault.esi;
    observation.edi = source.fault.edi;
    observation.ebp = source.fault.ebp;
    observation.eflags = source.fault.eflags;
    observation.instruction_window_observed =
        source.fault_observation.instruction_window_observed;
    observation.instruction_window_address =
        runtime::GuestAddress(source.fault_observation.instruction_window_address);
    observation.instruction_window = source.fault_observation.instruction_window;
    observation.stack_words_observed = source.fault_observation.stack_words_observed;
    observation.stack_words = source.fault_observation.stack_words;
    observation.stack_word_count = source.fault_observation.stack_word_count;
}
#endif

}  // namespace

bool RunOriginalInProcessCreateFileCall(const std::filesystem::path& executable_path,
                                        const exe::PeImageInfo& image_info,
                                        OriginalRunResult* result,
                                        std::string* error)
{
#if !defined(__i386__)
    static_cast<void>(executable_path);
    static_cast<void>(image_info);
    static_cast<void>(result);
    if (error != nullptr)
    {
        *error = "Linux in-process CreateFileA diagnostic requires an i386 host";
    }
    return false;
#else
    if (result == nullptr || error == nullptr)
    {
        return false;
    }

    std::vector<std::uint8_t> file_bytes;
    if (!ReadExecutable(executable_path, &file_bytes, error))
    {
        return false;
    }

    constexpr std::uint32_t kKernel32Module = 0x7F000001;
    constexpr std::uint32_t kGetVersionDynamicGate = 0xF1000001;
    constexpr std::uint32_t kCreateFileDynamicGate = 0xF1000002;
    NativeDynamicThunk get_version_thunk;
    NativeDynamicThunk create_file_thunk;
    if (!CreateNativeDynamicThunk(kGetVersionDynamicGate, &get_version_thunk, error) ||
        !CreateNativeDynamicThunk(kCreateFileDynamicGate, &create_file_thunk, error))
    {
        ReleaseNativeDynamicThunk(&get_version_thunk);
        ReleaseNativeDynamicThunk(&create_file_thunk);
        return false;
    }
    struct ThunkCleanup
    {
        NativeDynamicThunk* first;
        NativeDynamicThunk* second;
        ~ThunkCleanup()
        {
            ReleaseNativeDynamicThunk(first);
            ReleaseNativeDynamicThunk(second);
        }
    } thunk_cleanup{&get_version_thunk, &create_file_thunk};

    CreateFileContext context;
    context.image_base = static_cast<std::uint32_t>(image_info.image_base);
    context.image_size = image_info.size_of_image;
    context.get_version_thunk = get_version_thunk.address;
    context.create_file_thunk = create_file_thunk.address;

    auto handler = [](const NativeImportGateEvent& event,
                      NativeImportGateResult* output,
                      void* opaque) {
        auto* state = static_cast<CreateFileContext*>(opaque);
        const auto* stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(event.stack_pointer));
        if (output == nullptr || state == nullptr ||
            !StackContains(event,
                           event.stack_pointer,
                           8 * sizeof(std::uint32_t)))
        {
            return false;
        }
        if (!state->module_resolved && TextEquals(*state, stack[1], "kernel32"))
        {
            state->module_resolved = true;
            output->eax = kKernel32Module;
            output->stack_bytes_to_pop = 4;
            return true;
        }
        if (state->module_resolved && !state->get_version_resolved &&
            stack[1] == kKernel32Module && TextEquals(*state, stack[2], "GetVersion"))
        {
            state->get_version_resolved = true;
            output->eax = state->get_version_thunk;
            output->stack_bytes_to_pop = 8;
            return true;
        }
        if (state->get_version_resolved && !state->create_file_resolved &&
            stack[1] == kKernel32Module && TextEquals(*state, stack[2], "CreateFileA"))
        {
            state->create_file_resolved = true;
            output->eax = state->create_file_thunk;
            output->stack_bytes_to_pop = 8;
            return true;
        }
        if (event.gate_address == kGetVersionDynamicGate)
        {
            output->eax = 0;
            output->stack_bytes_to_pop = 0;
            return true;
        }
        if (state->create_file_resolved && !state->create_file_called &&
            event.gate_address == kCreateFileDynamicGate)
        {
            if (!ImageContains(*state, stack[0], 1))
            {
                return false;
            }
            state->create_file_called = true;
            state->return_address = stack[0];
            state->observation.observed = true;
            state->observation.file_name_address = runtime::GuestAddress(stack[1]);
            state->observation.file_name_observed =
                CopyGuestString(*state, event, stack[1], &state->observation.file_name);
            state->observation.desired_access = stack[2];
            state->observation.share_mode = stack[3];
            state->observation.security_attributes = runtime::GuestAddress(stack[4]);
            state->observation.creation_disposition = stack[5];
            state->observation.flags_and_attributes = stack[6];
            state->observation.template_file = runtime::GuestAddress(stack[7]);
            *reinterpret_cast<std::uint8_t*>(
                static_cast<std::uintptr_t>(state->return_address)) = 0xCC;
            output->eax = (std::numeric_limits<std::uint32_t>::max)();
            output->stack_bytes_to_pop = 7 * sizeof(std::uint32_t);
            return true;
        }
        return false;
    };

    NativeInProcessRunResult run;
    *result = {};
    const bool interrupted = !RunNativePeInProcess(file_bytes,
                                                    image_info,
                                                    context.image_base,
                                                    handler,
                                                    &context,
                                                    &run,
                                                    error);
    if (context.create_file_called && run.fault.status_code == SIGTRAP &&
        run.fault.instruction_pointer == context.return_address + 1)
    {
        result->boundary = OriginalRunBoundary::kCreateFileCalled;
        result->import_return_address = context.return_address;
    }
    else if (interrupted && context.create_file_resolved && run.fault.status_code != 0)
    {
        result->boundary = OriginalRunBoundary::kCreateFileCallNotReached;
    }
    else
    {
        *error = "CreateFileA continuation did not reach a confirmed diagnostic boundary";
        return false;
    }

    result->load_base = runtime::GuestAddress(context.image_base);
    result->entry_point = runtime::GuestAddress(context.image_base + image_info.entry_point_rva);
    result->instruction_pointer = runtime::GuestAddress(run.fault.instruction_pointer);
    result->status_code = run.fault.status_code;
    result->create_file_observation = context.observation;
    CopyFaultObservation(run, result);
    error->clear();
    return true;
#endif
}

}  // namespace re2dj::platform::linux
