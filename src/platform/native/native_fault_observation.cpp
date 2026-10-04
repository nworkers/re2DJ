#include "native_fault_observation.h"

#include <cstring>
#include <limits>

namespace re2dj::platform::native
{
void CaptureNativeFaultObservation(const NativePeImage& image,
                                   const NativeProcessBootstrap& bootstrap,
                                   const NativeGuestFault& fault,
                                   NativeFaultObservation* observation)
{
    if (observation == nullptr)
    {
        return;
    }
    *observation = {};
    if (fault.status_code == 0 || image.memory == nullptr)
    {
        return;
    }

    const std::uintptr_t image_address = reinterpret_cast<std::uintptr_t>(image.memory);
    if (image_address > (std::numeric_limits<std::uint32_t>::max)())
    {
        return;
    }
    const std::uint32_t image_base = static_cast<std::uint32_t>(image_address);
    const std::uint32_t window_bytes = static_cast<std::uint32_t>(observation->instruction_window.size());
    const std::uint32_t bytes_before =
        static_cast<std::uint32_t>(kNativeFaultInstructionBytesBefore);
    if (image.size >= window_bytes - bytes_before &&
        fault.instruction_pointer >= image_base)
    {
        const std::uint32_t instruction_rva = fault.instruction_pointer - image_base;
        if (instruction_rva >= bytes_before &&
            instruction_rva <= image.size - (window_bytes - bytes_before))
        {
            observation->instruction_window_address =
                fault.instruction_pointer - bytes_before;
            std::memcpy(observation->instruction_window.data(),
                        static_cast<const std::uint8_t*>(image.memory) +
                            observation->instruction_window_address - image_base,
                        observation->instruction_window.size());
            observation->instruction_window_observed = true;
        }
    }

    for (std::size_t index = 0; index < observation->stack_words.size(); ++index)
    {
        const std::uint32_t offset = static_cast<std::uint32_t>(index * sizeof(std::uint32_t));
        if (fault.stack_pointer > (std::numeric_limits<std::uint32_t>::max)() - offset ||
            !bootstrap.IsGuestStackRange(fault.stack_pointer + offset, sizeof(std::uint32_t)))
        {
            break;
        }
        std::memcpy(&observation->stack_words[index],
                    reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(fault.stack_pointer + offset)),
                    sizeof(observation->stack_words[index]));
        ++observation->stack_word_count;
    }
    observation->stack_words_observed = observation->stack_word_count != 0;

    observation->fs_base = bootstrap.Teb();
    if (observation->fs_base != 0)
    {
        std::uint32_t exception_list = 0;
        std::memcpy(&exception_list,
                    reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(observation->fs_base)),
                    sizeof(exception_list));
        observation->seh_frame_address = exception_list;
        if (exception_list != 0 && exception_list != 0xFFFFFFFFU &&
            bootstrap.IsGuestStackRange(exception_list, sizeof(std::uint32_t) * 2))
        {
            std::memcpy(&observation->seh_next,
                        reinterpret_cast<const void*>(
                            static_cast<std::uintptr_t>(exception_list)),
                        sizeof(observation->seh_next));
            std::memcpy(&observation->seh_handler,
                        reinterpret_cast<const void*>(
                            static_cast<std::uintptr_t>(exception_list + sizeof(std::uint32_t))),
                        sizeof(observation->seh_handler));
            observation->seh_frame_observed = true;
            if (observation->seh_handler >= image_base &&
                observation->seh_handler - image_base + observation->seh_handler_window.size() <=
                    image.size)
            {
                std::memcpy(observation->seh_handler_window.data(),
                            static_cast<const std::uint8_t*>(image.memory) +
                                (observation->seh_handler - image_base),
                            observation->seh_handler_window.size());
                observation->seh_handler_window_observed = true;
            }
        }
    }
}

}  // namespace re2dj::platform::native
