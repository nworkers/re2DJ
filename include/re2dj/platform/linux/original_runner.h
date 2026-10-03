#ifndef RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_
#define RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hle/guest_child_process.h"
#include "re2dj/hle/guest_devices.h"
#include "re2dj/hle/guest_files.h"
#include "re2dj/hle/host_audio.h"
#include "re2dj/hle/host_presentation.h"
#include "re2dj/input/io_bindings.h"
#include "re2dj/input/legacy_io_trap.h"
#include "re2dj/runtime/address_space.h"

namespace re2dj::platform::linux
{

constexpr std::size_t kOriginalFaultInstructionWindowBytes = 80;
constexpr std::size_t kOriginalInstructionTraceMaximumFrames = 128;
// The continuation keeps the first and the last calls of a run, so both its
// start and what led to its stop stay visible.
constexpr std::size_t kOriginalApiCallLogHead = 128;
constexpr std::size_t kOriginalApiCallLogTail = 128;
constexpr std::size_t kOriginalApiCallMaximumArguments = 13;
// By default the API log records this many calls in full; later ones are
// left out, since a product run goes on until its window is closed. The last
// calls before a stop are kept in memory and reported with the result.
constexpr std::uint32_t kOriginalApiLogFullCalls = 32768;

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
    kContinuationUnhandledImport,
    kContinuationUnresolvedLookup,
    kContinuationFault,
    kContinuationCallLimit,
    kContinuationHostClosed,
};

struct OriginalApiCall
{
    std::uint32_t sequence = 0;
    // The calling guest thread's ID, or 0 for the main thread.
    std::uint32_t thread_id = 0;
    std::string name;
    std::uint32_t return_address = 0;
    // Arguments are known only for facade exports, whose descriptors declare them.
    std::uint32_t argument_count = 0;
    std::array<std::uint32_t, kOriginalApiCallMaximumArguments> arguments = {};
    // Raw ANSI bytes of the export's string arguments, which may be CP949.
    bool text_observed = false;
    std::string text;
    // A second string argument, such as a MessageBoxA caption.
    bool second_text_observed = false;
    std::string second_text;
    bool handled = false;
    std::uint32_t eax = 0;
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
    bool seh_frame_observed = false;
    runtime::GuestAddress fs_base;
    runtime::GuestAddress seh_frame_address;
    std::uint32_t seh_next = 0;
    runtime::GuestAddress seh_handler;
    bool seh_handler_window_observed = false;
    std::array<std::uint8_t, 64> seh_handler_window = {};
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

struct OriginalResolverIdentity
{
    bool prepared = false;
    // Addresses the registry holds after the facade is prepared.
    runtime::GuestAddress registry_kernel32_base;
    runtime::GuestAddress registry_get_version;
    runtime::GuestAddress registry_create_file;
    // Addresses the guest actually received from the run-time resolver. Zero
    // means the guest never asked, which is distinct from a mismatch.
    runtime::GuestAddress kernel32_base;
    runtime::GuestAddress get_version_address;
    runtime::GuestAddress create_file_address;
    // Value read back from the rebound static IAT slot for CreateFileA.
    runtime::GuestAddress static_create_file_slot;
    bool static_imports_rebound = false;
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
    // First GetProcAddress request the facade registry could not resolve.
    bool unhandled_dynamic_request_observed = false;
    std::string unhandled_dynamic_request;
    // First static import gate outside the facade that the guest called.
    bool unhandled_import_observed = false;
    std::string unhandled_import;
    std::string module;
    std::string name;
    bool by_ordinal = false;
    std::uint16_t ordinal = 0;
    OriginalFaultObservation fault_observation;
    OriginalInstructionTrace instruction_trace;
    OriginalCreateFileObservation create_file_observation;
    OriginalResolverIdentity resolver_identity;
    // SEH dispatch diagnostic: number of guest exceptions dispatched to guest SEH.
    std::uint32_t seh_dispatch_count = 0;
    runtime::GuestAddress last_seh_handler;
    runtime::GuestAddress last_seh_resumed_eip;
    // Continuation diagnostic: the first kOriginalApiCallLogHead and the last
    // kOriginalApiCallLogTail calls in order; sequence numbers show any gap.
    std::vector<OriginalApiCall> api_calls;
    std::uint32_t api_call_count = 0;
    // What the continuation stopped on: the import, the lookup, or the last call.
    std::string continuation_stop_detail;
    // Requests the guest devices answered during the continuation, by kind.
    hle::hardlock::HardlockDeviceActivity device_activity;
    // Whether the device set held Hardlock material; never the material itself.
    bool hardlock_material_applied = false;
    // The guest's I/O board port accesses the run answered, and those it did
    // not; the first access's port and direction.
    std::uint32_t legacy_io_reads = 0;
    std::uint32_t legacy_io_writes = 0;
    std::uint32_t legacy_io_unanswered = 0;
    std::uint16_t legacy_io_first_port = 0;
    bool legacy_io_first_read = false;
};

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

// devices: what the guest may open, from the target profile and the user's
// Hardlock material.
// What a continuation run provides the guest beyond its image.
struct OriginalRunEnvironment
{
    hle::GuestDeviceConfig devices;
    // The main image's guest path, for example "D:\\ez2dj\\EZ2DJ.EXE".
    std::string module_path;
    hle::GuestFileConfig files;
    // Where the guest's window appears on the host, or null to show nothing.
    // The caller owns it, so the window can outlive the run.
    hle::HostPresentation* presentation = nullptr;
    // Where the guest's sound plays, or null to play it silently; the caller
    // owns it too.
    hle::HostAudio* audio = nullptr;
    // The profile's I/O board port contract; the run places it at the loaded
    // main image.
    input::LegacyIoTrapPolicy legacy_io;
    // The keys and gamepad controls the board's inputs follow: the built-in
    // defaults, or those with an --io-config INI's entries applied (task 444).
    input::IoBindings io_bindings = input::DefaultIoBindings();
    // Stops the run after this many calls, for diagnostics and regression
    // runs that must end on their own; 0 runs until the guest exits, stops,
    // or the host window is closed.
    std::uint32_t call_limit = 0;
    // How many calls the API log records in full; 0 records every call.
    std::uint32_t api_log_calls = kOriginalApiLogFullCalls;
    // How a launcher started this guest process, when one did, and the
    // first current directory it gave (a guest path; empty keeps the guest
    // root).
    hle::GuestStartup startup;
    std::string current_directory;
    // Where the guest's CreateProcessA starts its children, or null to start
    // none; the caller owns it.
    hle::HostProcessLauncher* process_launcher = nullptr;
    // RVA of the guest's autoplay flag in the main image, armed for this
    // build by target::ArmedAutoplayFlagRva; 0 offers no autoplay control.
    std::uint32_t autoplay_flag_rva = 0;
};

bool RunOriginalInProcessContinuation(const std::filesystem::path& executable_path,
                                      const exe::PeImageInfo& image_info,
                                      const OriginalRunEnvironment& environment,
                                      OriginalRunResult* result,
                                      std::string* error);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_ORIGINAL_RUNNER_H_
