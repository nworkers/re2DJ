#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>

#include "audio_volume_trace.h"

namespace
{
template <typename T>
bool CopyGuest(const T* source, T* destination)
{
    SIZE_T copied = 0;
    return source != nullptr && destination != nullptr &&
           ReadProcessMemory(GetCurrentProcess(), source, destination, sizeof(T), &copied) != FALSE &&
           copied == sizeof(T);
}

void TraceControlDetails(const char* operation, const MIXERCONTROLDETAILS* details, DWORD flags,
                         MMRESULT result, bool capture_values)
{
    MIXERCONTROLDETAILS copy = {};
    if (!CopyGuest(details, &copy))
    {
        Re2djAudioTrace("winmm:%s flags=0x%08lx result=%u details=unreadable",
                        operation, static_cast<unsigned long>(flags), result);
        return;
    }
    Re2djAudioTrace("winmm:%s control=%lu channels=%lu multiple=%lu cb=%lu flags=0x%08lx result=%u",
                    operation, static_cast<unsigned long>(copy.dwControlID),
                    static_cast<unsigned long>(copy.cChannels),
                    static_cast<unsigned long>(copy.cMultipleItems),
                    static_cast<unsigned long>(copy.cbDetails),
                    static_cast<unsigned long>(flags), result);
    if (!capture_values || copy.paDetails == nullptr ||
        copy.cbDetails < sizeof(MIXERCONTROLDETAILS_UNSIGNED))
    {
        return;
    }
    const DWORD count = (std::min)(8UL, (std::max)(1UL, copy.cChannels) *
                                          (std::max)(1UL, copy.cMultipleItems));
    const auto* values = static_cast<const unsigned char*>(copy.paDetails);
    for (DWORD index = 0; index < count; ++index)
    {
        MIXERCONTROLDETAILS_UNSIGNED value = {};
        SIZE_T copied = 0;
        if (ReadProcessMemory(GetCurrentProcess(), values + index * copy.cbDetails,
                              &value, sizeof(value), &copied) == FALSE || copied != sizeof(value))
        {
            break;
        }
        Re2djAudioTrace("winmm:%s-value control=%lu index=%lu value=%lu", operation,
                        static_cast<unsigned long>(copy.dwControlID),
                        static_cast<unsigned long>(index),
                        static_cast<unsigned long>(value.dwValue));
    }
}
}  // namespace

extern "C" __declspec(dllexport) UINT WINAPI Re2djTraceMixerGetNumDevs()
{
    const UINT result = mixerGetNumDevs();
    Re2djAudioTrace("winmm:mixerGetNumDevs result=%u", result);
    return result;
}

extern "C" __declspec(dllexport) MMRESULT WINAPI Re2djTraceMixerOpen(LPHMIXER mixer, UINT id, DWORD_PTR callback,
                                                  DWORD_PTR instance, DWORD flags)
{
    const MMRESULT result = mixerOpen(mixer, id, callback, instance, flags);
    Re2djAudioTrace("winmm:mixerOpen id=%u flags=0x%08lx result=%u", id,
                    static_cast<unsigned long>(flags), result);
    return result;
}

extern "C" __declspec(dllexport) MMRESULT WINAPI Re2djTraceMixerClose(HMIXER mixer)
{
    const MMRESULT result = mixerClose(mixer);
    Re2djAudioTrace("winmm:mixerClose result=%u", result);
    return result;
}

extern "C" __declspec(dllexport) MMRESULT WINAPI Re2djTraceMixerGetLineInfoA(HMIXEROBJ mixer, LPMIXERLINEA line,
                                                         DWORD flags)
{
    const MMRESULT result = mixerGetLineInfoA(mixer, line, flags);
    MIXERLINEA copy = {};
    if (CopyGuest(line, &copy))
    {
        Re2djAudioTrace("winmm:mixerGetLineInfoA destination=%lu source=%lu channels=%lu controls=%lu component=0x%08lx flags=0x%08lx result=%u",
                        static_cast<unsigned long>(copy.dwDestination),
                        static_cast<unsigned long>(copy.dwSource),
                        static_cast<unsigned long>(copy.cChannels),
                        static_cast<unsigned long>(copy.cControls),
                        static_cast<unsigned long>(copy.dwComponentType),
                        static_cast<unsigned long>(flags), result);
    }
    else
    {
        Re2djAudioTrace("winmm:mixerGetLineInfoA flags=0x%08lx result=%u line=unreadable",
                        static_cast<unsigned long>(flags), result);
    }
    return result;
}

extern "C" __declspec(dllexport) MMRESULT WINAPI Re2djTraceMixerGetLineControlsA(HMIXEROBJ mixer,
                                                             LPMIXERLINECONTROLSA controls,
                                                             DWORD flags)
{
    const MMRESULT result = mixerGetLineControlsA(mixer, controls, flags);
    MIXERLINECONTROLSA copy = {};
    if (CopyGuest(controls, &copy))
    {
        Re2djAudioTrace("winmm:mixerGetLineControlsA line=%lu controls=%lu cb=%lu flags=0x%08lx result=%u",
                        static_cast<unsigned long>(copy.dwLineID),
                        static_cast<unsigned long>(copy.cControls),
                        static_cast<unsigned long>(copy.cbmxctrl),
                        static_cast<unsigned long>(flags), result);
        const DWORD count = (std::min)(copy.cControls, 8UL);
        for (DWORD index = 0; index < count && copy.pamxctrl != nullptr; ++index)
        {
            MIXERCONTROLA control = {};
            SIZE_T copied = 0;
            const auto* address = reinterpret_cast<const unsigned char*>(copy.pamxctrl) +
                                  index * copy.cbmxctrl;
            if (copy.cbmxctrl < sizeof(control) ||
                ReadProcessMemory(GetCurrentProcess(), address, &control, sizeof(control),
                                  &copied) == FALSE || copied != sizeof(control))
            {
                break;
            }
            Re2djAudioTrace("winmm:control index=%lu id=%lu type=0x%08lx min=%ld max=%ld name=%s",
                            static_cast<unsigned long>(index),
                            static_cast<unsigned long>(control.dwControlID),
                            static_cast<unsigned long>(control.dwControlType),
                            control.Bounds.lMinimum, control.Bounds.lMaximum,
                            control.szShortName);
        }
    }
    return result;
}

extern "C" __declspec(dllexport) MMRESULT WINAPI Re2djTraceMixerGetControlDetailsA(HMIXEROBJ mixer,
                                                               LPMIXERCONTROLDETAILS details,
                                                               DWORD flags)
{
    const MMRESULT result = mixerGetControlDetailsA(mixer, details, flags);
    TraceControlDetails("mixerGetControlDetailsA", details, flags, result,
                        flags == MIXER_GETCONTROLDETAILSF_VALUE);
    return result;
}

extern "C" __declspec(dllexport) MMRESULT WINAPI Re2djTraceMixerSetControlDetails(HMIXEROBJ mixer,
                                                              LPMIXERCONTROLDETAILS details,
                                                              DWORD flags)
{
    TraceControlDetails("mixerSetControlDetails-input", details, flags, MMSYSERR_NOERROR,
                        flags == MIXER_SETCONTROLDETAILSF_VALUE);
    const MMRESULT result = mixerSetControlDetails(mixer, details, flags);
    Re2djAudioTrace("winmm:mixerSetControlDetails-result flags=0x%08lx result=%u",
                    static_cast<unsigned long>(flags), result);
    return result;
}
