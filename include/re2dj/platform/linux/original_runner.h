#ifndef RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_
#define RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/address_space.h"

namespace re2dj::platform::linux
{

constexpr std::size_t kOriginalFaultInstructionWindowBytes = 80;
constexpr std::size_t kOriginalInstructionTraceMaximumFrames = 128;

enum class OriginalRunBoundary
{
    kImportGate,
    kProcessExit,
    kFault,
    kStopped,
    kFirstImportCompleted,
    kFirstResolverObserved,
    kGetVersionCalled,
    kGetVersionCallNotReached,
    kCreateFileCalled,
    kCreateFileCallNotReached,
};

struct OriginalFaultObservation
{
    bool observed = false;
    runtime::GuestAddress fault_address;
    std::uint32_t signal_code = 0;
    std::uint32_t cpu_error_code = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
    bool instruction_window_observed = false;
    runtime::GuestAddress instruction_window_address;
    std::array<std::uint8_t, kOriginalFaultInstructionWindowBytes> instruction_window = {};
    bool stack_words_observed = false;
    std::array<std::uint32_t, 4> stack_words = {};
    std::uint32_t stack_word_count = 0;
};

struct OriginalInstructionTraceFrame
{
    runtime::GuestAddress instruction_pointer;
    runtime::GuestAddress stack_pointer;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
};

struct OriginalInstructionTrace
{
    bool armed = false;
    bool started = false;
    bool limit_reached = false;
    runtime::GuestAddress breakpoint;
    std::uint32_t frame_count = 0;
    std::array<OriginalInstructionTraceFrame, kOriginalInstructionTraceMaximumFrames> frames = {};
};

struct OriginalCreateFileObservation
{
    bool observed = false;
    bool file_name_observed = false;
    runtime::GuestAddress file_name_address;
    std::string file_name;
    std::uint32_t desired_access = 0;
    std::uint32_t share_mode = 0;
    runtime::GuestAddress security_attributes;
    std::uint32_t creation_disposition = 0;
    std::uint32_t flags_and_attributes = 0;
    runtime::GuestAddress template_file;
};

struct OriginalRunResult
{
    OriginalRunBoundary boundary = OriginalRunBoundary::kStopped;
    runtime::GuestAddress load_base;
    runtime::GuestAddress entry_point;
    runtime::GuestAddress instruction_pointer;
    runtime::GuestAddress stack_pointer;
    runtime::GuestAddress gate_address;
    std::uint32_t status_code = 0;
    bool import_stack_observed = false;
    std::uint32_t import_return_address = 0;
    std::uint32_t import_first_argument = 0;
    bool import_first_argument_text_observed = false;
    std::string import_first_argument_text;
    bool unhandled_dynamic_request_observed = false;
    std::string unhandled_dynamic_request;
    std::string module;
    std::string name;
    bool by_ordinal = false;
    std::uint16_t ordinal = 0;
    OriginalFaultObservation fault_observation;
    OriginalInstructionTrace instruction_trace;
    OriginalCreateFileObservation create_file_observation;
};

bool RunOriginalUntilBoundary(const std::filesystem::path& executable_path,
                              const exe::PeImageInfo& image_info,
                              const std::filesystem::path& helper_path,
                              OriginalRunResult* result,
                              std::string* error);

bool RunOriginalInProcessFirstImport(const std::filesystem::path& executable_path,
                                     const exe::PeImageInfo& image_info,
                                     OriginalRunResult* result,
                                     std::string* error);

bool RunOriginalInProcessFirstResolver(const std::filesystem::path& executable_path,
                                       const exe::PeImageInfo& image_info,
                                       OriginalRunResult* result,
                                       std::string* error);

bool RunOriginalInProcessGetVersionCall(const std::filesystem::path& executable_path,
                                        const exe::PeImageInfo& image_info,
                                        OriginalRunResult* result,
                                        std::string* error);

bool RunOriginalInProcessCreateFileCall(const std::filesystem::path& executable_path,
                                        const exe::PeImageInfo& image_info,
                                        OriginalRunResult* result,
                                        std::string* error);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_
