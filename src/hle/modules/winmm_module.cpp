#include "re2dj/hle/modules/winmm_module.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "facade_com.h"
#include "re2dj/hle/guest_mixer.h"
#include "re2dj/hle/guest_process.h"

namespace re2dj::hle::modules
{
namespace
{

bool CheckShape(const ImportCall& call, ImportReturn* result, std::size_t count, std::string* error)
{
    if (result == nullptr || call.arguments.size() != count)
    {
        if (error != nullptr)
        {
            *error = "winmm " + call.gate.name + " argument shape is invalid";
        }
        return false;
    }
    *result = {};
    return true;
}

// timeBeginPeriod(uPeriod) and timeEndPeriod(uPeriod): the host timer is
// already finer than a millisecond, so every period from 1 ms is granted;
// 0 is refused with TIMERR_NOCANDO, as the documented minimum is 1.
bool ChangePeriod(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckShape(call, result, 1, error))
    {
        return false;
    }
    result->eax = call.arguments[0] == 0 ? kTimerNoCanDo : kTimerNoError;
    return true;
}

// timeGetTime(): milliseconds of the host's monotonic clock, as GetTickCount.
bool TimeGetTime(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckShape(call, result, 0, error))
    {
        return false;
    }
    GuestClockReading reading;
    if (call.services == nullptr || !call.services->ReadClock(&reading))
    {
        if (error != nullptr) *error = "winmm timeGetTime needs the host clock";
        return false;
    }
    result->eax = reading.tick_ms;
    return true;
}

// mixerGetNumDevs(): the one mixer device the facade models, as the display
// side models one monitor.
bool MixerGetNumDevs(const ImportCall& call, ImportReturn* result, std::string* error)
{
    if (!CheckShape(call, result, 0, error))
    {
        return false;
    }
    result->eax = 1;
    return true;
}

// ---------------------------------------------------------------------------
// The mixer, as a Windows 11 host answers for its first mixer (GuestMixer)
// ---------------------------------------------------------------------------

// MIXERLINEA (168 bytes).
struct MixerLineA
{
    std::uint32_t size = 0;
    std::uint32_t destination = 0;
    std::uint32_t source = 0;
    std::uint32_t line_id = 0;
    std::uint32_t flags = 0;
    std::uint32_t user = 0;
    std::uint32_t component_type = 0;
    std::uint32_t channels = 0;
    std::uint32_t connections = 0;
    std::uint32_t controls = 0;
    std::array<char, 16> short_name{};
    std::array<char, 64> name{};
    std::uint32_t target_type = 0;
    std::uint32_t target_device_id = 0;
    std::uint16_t target_manufacturer = 0;
    std::uint16_t target_product = 0;
    std::uint32_t target_version = 0;
    std::array<char, 32> target_name{};
};
static_assert(sizeof(MixerLineA) == 168);

// MIXERLINECONTROLSA (24 bytes); control is dwControlID or dwControlType.
struct MixerLineControlsA
{
    std::uint32_t size = 0;
    std::uint32_t line_id = 0;
    std::uint32_t control = 0;
    std::uint32_t count = 0;
    std::uint32_t control_size = 0;
    std::uint32_t controls = 0;
};
static_assert(sizeof(MixerLineControlsA) == 24);

// MIXERCONTROLA (148 bytes).
struct MixerControlA
{
    std::uint32_t size = 0;
    std::uint32_t id = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t multiple_items = 0;
    std::array<char, 16> short_name{};
    std::array<char, 64> name{};
    std::array<std::uint32_t, 6> bounds{};
    std::array<std::uint32_t, 6> metrics{};
};
static_assert(sizeof(MixerControlA) == 148);

// MIXERCONTROLDETAILS (24 bytes).
struct MixerControlDetails
{
    std::uint32_t size = 0;
    std::uint32_t control_id = 0;
    std::uint32_t channels = 0;
    std::uint32_t multiple_items = 0;
    std::uint32_t detail_size = 0;
    std::uint32_t details = 0;
};
static_assert(sizeof(MixerControlDetails) == 24);

// MIXER_OBJECTF_* in a flag word's high bits, and the query in its low ones.
constexpr std::uint32_t kObjectTypeMask = 0xF0000000U;
constexpr std::uint32_t kObjectMixerId = 0x00000000U;
constexpr std::uint32_t kObjectHandle = 0x80000000U;
constexpr std::uint32_t kQueryMask = 0x0000000FU;
constexpr std::uint32_t kLineInfoDestination = 0;
constexpr std::uint32_t kLineInfoSource = 1;
constexpr std::uint32_t kLineInfoLineId = 2;
constexpr std::uint32_t kLineInfoComponentType = 3;
constexpr std::uint32_t kLineInfoTargetType = 4;
constexpr std::uint32_t kLineControlsAll = 0;
constexpr std::uint32_t kLineControlsOneById = 1;
constexpr std::uint32_t kLineControlsOneByType = 2;
constexpr std::uint32_t kControlDetailsValue = 0;

template <std::size_t N>
void CopyName(std::array<char, N>* destination, std::string_view text)
{
    destination->fill('\0');
    std::memcpy(destination->data(), text.data(), std::min(text.size(), N - 1));
}

MixerLineA DescribeLine(const GuestMixerLine& line, std::uint32_t size)
{
    MixerLineA out;
    out.size = size;
    out.destination = line.destination;
    out.source = line.source;
    out.line_id = line.line_id;
    out.flags = line.flags;
    out.component_type = line.component_type;
    out.channels = line.channels;
    out.connections = line.connections;
    out.controls = line.controls;
    CopyName(&out.short_name, line.short_name);
    CopyName(&out.name, line.name);
    out.target_type = line.target_type;
    out.target_manufacturer = line.target_manufacturer;
    out.target_product = line.target_product;
    out.target_version = line.target_version;
    CopyName(&out.target_name, line.target_name);
    return out;
}

MixerControlA DescribeControl(const GuestMixerControl& control)
{
    MixerControlA out;
    out.size = sizeof(MixerControlA);
    out.id = control.id;
    out.type = control.type;
    out.flags = control.flags;
    CopyName(&out.short_name, control.short_name);
    CopyName(&out.name, control.name);
    out.bounds[0] = control.minimum;
    out.bounds[1] = control.maximum;
    out.metrics[0] = control.steps;
    return out;
}

// The mixer a call names: the open handle its flags say it is, or under
// MIXER_OBJECTF_MIXER the mixer ID 0 or, as Windows 11 also accepts there,
// an open handle. The answer when it is neither, or false with error set
// for an object type (a wave device, say) that is not modelled.
bool ResolveMixer(const ImportCall& call, GuestProcess& process, std::uint32_t flags, std::uint32_t* refusal,
                  std::string* error)
{
    const std::uint32_t object = call.arguments[0];
    *refusal = kMmsyserrNoError;
    switch (flags & kObjectTypeMask)
    {
    case kObjectHandle:
        if (!process.mixer().IsOpen(object))
        {
            *refusal = kMmsyserrInvalHandle;
        }
        return true;
    case kObjectMixerId:
        if (object != 0 && !process.mixer().IsOpen(object))
        {
            *refusal = kMmsyserrBadDeviceId;
        }
        return true;
    default:
        return com::Fail(error, "winmm " + call.gate.name + " has no model of mixer object type " +
                                    std::to_string(flags >> 28));
    }
}

GuestProcess* MixerProcess(const ImportCall& call, ImportReturn* result, std::size_t count, std::string* error)
{
    if (!CheckShape(call, result, count, error))
    {
        return nullptr;
    }
    GuestProcess* process = call.services == nullptr ? nullptr : call.services->Process();
    if (process == nullptr)
    {
        com::Fail(error, "winmm " + call.gate.name + " needs the guest process");
    }
    return process;
}

// mixerOpen(phmx, uMxId, dwCallback, dwInstance, fdwOpen): a handle to mixer
// 0; any other ID is MMSYSERR_BADDEVICEID. Callbacks are not modelled.
bool MixerOpen(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MixerProcess(call, result, 5, error);
    if (process == nullptr)
    {
        return false;
    }
    if (call.arguments[0] == 0 || call.arguments[4] != 0)
    {
        return com::Fail(error, "winmm mixerOpen has no model of a null handle pointer or open flags");
    }
    if (call.arguments[1] != 0)
    {
        return com::Succeed(result, kMmsyserrBadDeviceId, error);
    }
    const std::uint32_t handle = process->mixer().Open(process->handles());
    if (!com::WriteWord(call, call.arguments[0], handle, error))
    {
        process->mixer().Close(handle);
        return false;
    }
    return com::Succeed(result, kMmsyserrNoError, error);
}

// mixerClose(hmx): MMSYSERR_INVALHANDLE for a handle that is not open.
bool MixerClose(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MixerProcess(call, result, 1, error);
    if (process == nullptr)
    {
        return false;
    }
    return com::Succeed(result, process->mixer().Close(call.arguments[0]) ? kMmsyserrNoError : kMmsyserrInvalHandle,
                        error);
}

// mixerGetLineInfoA(hmxobj, pmxl, fdwInfo): a destination, a destination's
// source, or the line with an ID or component type. A MIXERLINEA smaller than
// its own size is MMSYSERR_INVALPARAM, a line that does not exist
// MIXERR_INVALLINE, and an unknown query MMSYSERR_INVALFLAG.
bool MixerGetLineInfoA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MixerProcess(call, result, 3, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t flags = call.arguments[2];
    std::uint32_t refusal = kMmsyserrNoError;
    if (!ResolveMixer(call, *process, flags, &refusal, error))
    {
        return false;
    }
    if (refusal != kMmsyserrNoError)
    {
        return com::Succeed(result, refusal, error);
    }
    MixerLineA request;
    if (call.arguments[1] == 0 || !com::ReadStruct(call, call.arguments[1], &request.size, error))
    {
        return call.arguments[1] == 0 ? com::Fail(error, "winmm mixerGetLineInfoA has no model of a null line")
                                      : false;
    }
    if (request.size < sizeof(MixerLineA))
    {
        return com::Succeed(result, kMmsyserrInvalParam, error);
    }
    if (!com::ReadStruct(call, call.arguments[1], &request, error))
    {
        return false;
    }
    const GuestMixerLine* line = nullptr;
    switch (flags & kQueryMask)
    {
    case kLineInfoDestination:
        if (request.destination < GuestMixerDestinations().size())
        {
            line = &GuestMixerDestinations()[request.destination];
        }
        break;
    case kLineInfoSource:
        if (request.destination < GuestMixerDestinations().size() && request.source < GuestMixerSources().size())
        {
            line = &GuestMixerSources()[request.source];
        }
        break;
    case kLineInfoLineId:
        line = FindGuestMixerLine(request.line_id);
        break;
    case kLineInfoComponentType:
        line = FindGuestMixerComponent(request.component_type);
        break;
    case kLineInfoTargetType:
        return com::Fail(error, "winmm mixerGetLineInfoA has no model of a target type query");
    default:
        return com::Succeed(result, kMmsyserrInvalFlag, error);
    }
    if (line == nullptr)
    {
        return com::Succeed(result, kMixerrInvalLine, error);
    }
    return com::WriteStruct(call, call.arguments[1], DescribeLine(*line, request.size), error) &&
           com::Succeed(result, kMmsyserrNoError, error);
}

// mixerGetLineControlsA(hmxobj, pmxlc, fdwControls): all of a line's
// controls, or one by ID or type. A line that does not exist is
// MIXERR_INVALLINE, a control that does not MIXERR_INVALCONTROL.
bool MixerGetLineControlsA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MixerProcess(call, result, 3, error);
    if (process == nullptr)
    {
        return false;
    }
    const std::uint32_t flags = call.arguments[2];
    std::uint32_t refusal = kMmsyserrNoError;
    if (!ResolveMixer(call, *process, flags, &refusal, error))
    {
        return false;
    }
    if (refusal != kMmsyserrNoError)
    {
        return com::Succeed(result, refusal, error);
    }
    MixerLineControlsA request;
    if (call.arguments[1] == 0 || !com::ReadStruct(call, call.arguments[1], &request, error))
    {
        return call.arguments[1] == 0 ? com::Fail(error, "winmm mixerGetLineControlsA has no model of a null request")
                                      : false;
    }
    if (request.size != sizeof(MixerLineControlsA) || request.control_size != sizeof(MixerControlA) ||
        request.controls == 0)
    {
        return com::Fail(error, "winmm mixerGetLineControlsA has no model of this request's sizes");
    }
    switch (flags & kQueryMask)
    {
    case kLineControlsAll:
    {
        const GuestMixerLine* line = FindGuestMixerLine(request.line_id);
        if (line == nullptr)
        {
            return com::Succeed(result, kMixerrInvalLine, error);
        }
        if (request.count != line->controls)
        {
            return com::Fail(error, "winmm mixerGetLineControlsA has no model of a count other than the line's");
        }
        std::uint32_t address = request.controls;
        for (const GuestMixerControl& control : GuestMixerControls())
        {
            if (control.line_id == line->line_id)
            {
                if (!com::WriteStruct(call, address, DescribeControl(control), error))
                {
                    return false;
                }
                address += sizeof(MixerControlA);
            }
        }
        return com::Succeed(result, kMmsyserrNoError, error);
    }
    case kLineControlsOneById:
    {
        const GuestMixerControl* control = FindGuestMixerControl(request.control);
        if (control == nullptr)
        {
            return com::Succeed(result, kMixerrInvalControl, error);
        }
        return com::WriteWord(call, call.arguments[1] + 4, control->line_id, error) &&
               com::WriteStruct(call, request.controls, DescribeControl(*control), error) &&
               com::Succeed(result, kMmsyserrNoError, error);
    }
    case kLineControlsOneByType:
    {
        if (FindGuestMixerLine(request.line_id) == nullptr)
        {
            return com::Succeed(result, kMixerrInvalLine, error);
        }
        const GuestMixerControl* control = FindGuestMixerControlByType(request.line_id, request.control);
        if (control == nullptr)
        {
            return com::Succeed(result, kMixerrInvalControl, error);
        }
        return com::WriteStruct(call, request.controls, DescribeControl(*control), error) &&
               com::Succeed(result, kMmsyserrNoError, error);
    }
    default:
        return com::Fail(error, "winmm mixerGetLineControlsA has no model of query " + std::to_string(flags & kQueryMask));
    }
}

// The control a MIXERCONTROLDETAILS names, once the object, size, and query
// check out: null with the refusal in *refusal, or null with error set for a
// shape that is not modelled.
const GuestMixerControl* DetailedControl(const ImportCall& call,
                                         GuestProcess& process,
                                         bool setting,
                                         MixerControlDetails* request,
                                         std::uint32_t* refusal,
                                         std::string* error)
{
    const std::uint32_t flags = call.arguments[2];
    if (!ResolveMixer(call, process, flags, refusal, error) || *refusal != kMmsyserrNoError)
    {
        return nullptr;
    }
    if (call.arguments[1] == 0)
    {
        com::Fail(error, "winmm " + call.gate.name + " has no model of null details");
        return nullptr;
    }
    if (!com::ReadStruct(call, call.arguments[1], &request->size, error))
    {
        return nullptr;
    }
    if (request->size != sizeof(MixerControlDetails))
    {
        *refusal = kMmsyserrInvalParam;
        return nullptr;
    }
    if (!com::ReadStruct(call, call.arguments[1], request, error))
    {
        return nullptr;
    }
    if ((flags & kQueryMask) != kControlDetailsValue)
    {
        com::Fail(error, "winmm " + call.gate.name + " has no model of query " + std::to_string(flags & kQueryMask));
        return nullptr;
    }
    const GuestMixerControl* control = FindGuestMixerControl(request->control_id);
    if (control == nullptr)
    {
        *refusal = kMixerrInvalControl;
        return nullptr;
    }
    // Measured: a mute takes one channel and refuses two; a volume answers one
    // or two alike. Setting was measured with one channel only.
    const bool mute = control->type == kMixerControlTypeMute;
    const std::uint32_t most = setting || mute ? 1U : 2U;
    if (mute && request->channels == 2 && !setting)
    {
        *refusal = kMmsyserrInvalParam;
        return nullptr;
    }
    if (request->channels < 1 || request->channels > most || request->multiple_items != 0 ||
        request->detail_size != 4 || request->details == 0)
    {
        com::Fail(error, "winmm " + call.gate.name + " has no model of these details' shape");
        return nullptr;
    }
    return control;
}

// mixerGetControlDetailsA(hmxobj, pmxcd, fdwDetails): the control's value
// in every channel asked for.
bool MixerGetControlDetailsA(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MixerProcess(call, result, 3, error);
    if (process == nullptr)
    {
        return false;
    }
    MixerControlDetails request;
    std::uint32_t refusal = kMmsyserrNoError;
    const GuestMixerControl* control = DetailedControl(call, *process, false, &request, &refusal, error);
    if (control == nullptr)
    {
        return refusal != kMmsyserrNoError && com::Succeed(result, refusal, error);
    }
    const std::uint32_t value = process->mixer().Value(*control);
    for (std::uint32_t channel = 0; channel < request.channels; ++channel)
    {
        if (!com::WriteWord(call, request.details + channel * 4, value, error))
        {
            return false;
        }
    }
    return com::Succeed(result, kMmsyserrNoError, error);
}

// mixerSetControlDetails(hmxobj, pmxcd, fdwDetails): the control's new value;
// one outside its bounds is ignored with MMSYSERR_NOERROR, as measured.
bool MixerSetControlDetails(const ImportCall& call, ImportReturn* result, std::string* error)
{
    GuestProcess* process = MixerProcess(call, result, 3, error);
    if (process == nullptr)
    {
        return false;
    }
    MixerControlDetails request;
    std::uint32_t refusal = kMmsyserrNoError;
    const GuestMixerControl* control = DetailedControl(call, *process, true, &request, &refusal, error);
    if (control == nullptr)
    {
        return refusal != kMmsyserrNoError && com::Succeed(result, refusal, error);
    }
    std::uint32_t value = 0;
    if (!com::ReadStruct(call, request.details, &value, error))
    {
        return false;
    }
    process->mixer().SetValue(*control, value);
    return com::Succeed(result, kMmsyserrNoError, error);
}

GuestExportDescriptor MakeExport(std::string name, std::uint32_t argument_count, ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

}  // namespace

GuestModuleDescriptor MakeWinmmModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "winmm.dll";
    descriptor.aliases = {"winmm"};
    descriptor.exports.push_back(MakeExport("timeBeginPeriod", 1, &ChangePeriod));
    descriptor.exports.push_back(MakeExport("timeEndPeriod", 1, &ChangePeriod));
    descriptor.exports.push_back(MakeExport("timeGetTime", 0, &TimeGetTime));
    descriptor.exports.push_back(MakeExport("mixerGetNumDevs", 0, &MixerGetNumDevs));
    descriptor.exports.push_back(MakeExport("mixerOpen", 5, &MixerOpen));
    descriptor.exports.push_back(MakeExport("mixerClose", 1, &MixerClose));
    descriptor.exports.push_back(MakeExport("mixerGetLineInfoA", 3, &MixerGetLineInfoA));
    descriptor.exports.push_back(MakeExport("mixerGetLineControlsA", 3, &MixerGetLineControlsA));
    descriptor.exports.push_back(MakeExport("mixerGetControlDetailsA", 3, &MixerGetControlDetailsA));
    descriptor.exports.push_back(MakeExport("mixerSetControlDetails", 3, &MixerSetControlDetails));
    return descriptor;
}

}  // namespace re2dj::hle::modules
