#ifndef RE2DJ_PLATFORM_LINUX_NATIVE_FAULT_OBSERVATION_H_
#define RE2DJ_PLATFORM_LINUX_NATIVE_FAULT_OBSERVATION_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "native_pe_image.h"
#include "native_process_bootstrap.h"

namespace re2dj::platform::linux
{

constexpr std::size_t kNativeFaultInstructionBytesBefore = 64;
constexpr std::size_t kNativeFaultInstructionBytesAfter = 16;
constexpr std::size_t kNativeFaultInstructionWindowBytes =
    kNativeFaultInstructionBytesBefore + kNativeFaultInstructionBytesAfter;

struct NativeFaultObservation
{
    bool instruction_window_observed = false;
    std::uint32_t instruction_window_address = 0;
    std::array<std::uint8_t, kNativeFaultInstructionWindowBytes> instruction_window = {};
    bool stack_words_observed = false;
    std::array<std::uint32_t, 4> stack_words = {};
    std::uint32_t stack_word_count = 0;
    bool seh_frame_observed = false;
    std::uint32_t fs_base = 0;
    std::uint32_t seh_frame_address = 0;
    std::uint32_t seh_next = 0;
    std::uint32_t seh_handler = 0;
    bool seh_handler_window_observed = false;
    std::array<std::uint8_t, 64> seh_handler_window = {};
};

void CaptureNativeFaultObservation(const NativePeImage& image,
                                   const NativeProcessBootstrap& bootstrap,
                                   const NativeGuestFault& fault,
                                   NativeFaultObservation* observation);

}  // namespace re2dj::platform::linux

#endif  // RE2DJ_PLATFORM_LINUX_NATIVE_FAULT_OBSERVATION_H_
