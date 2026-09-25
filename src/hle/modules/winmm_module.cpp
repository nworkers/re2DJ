#include "re2dj/hle/modules/winmm_module.h"

#include <cstdint>
#include <string>
#include <utility>

#include "re2dj/hle/modules/resolve_only_modules.h"

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

GuestExportDescriptor MakeExport(std::string name, std::uint32_t argument_count, ImportHandler handler)
{
    GuestExportDescriptor descriptor;
    descriptor.name = std::move(name);
    descriptor.calling_convention = CallingConvention::kStdcall;
    descriptor.argument_count = argument_count;
    descriptor.handler = handler;
    return descriptor;
}

// The mixer exports the original imports but has not been seen to call
// (mmsystem.h signatures).
constexpr ResolveOnlyExport kWinmmResolveOnly[] = {
    {"mixerGetLineControlsA", 3}, {"mixerClose", 1}, {"mixerGetNumDevs", 0},
    {"mixerOpen", 5}, {"mixerGetControlDetailsA", 3}, {"mixerSetControlDetails", 3},
    {"mixerGetLineInfoA", 3},
};

}  // namespace

GuestModuleDescriptor MakeWinmmModuleDescriptor()
{
    GuestModuleDescriptor descriptor;
    descriptor.name = "winmm.dll";
    descriptor.aliases = {"winmm"};
    descriptor.exports.push_back(MakeExport("timeBeginPeriod", 1, &ChangePeriod));
    descriptor.exports.push_back(MakeExport("timeEndPeriod", 1, &ChangePeriod));
    descriptor.exports.push_back(MakeExport("timeGetTime", 0, &TimeGetTime));
    AddResolveOnlyExports(&descriptor, kWinmmResolveOnly);
    return descriptor;
}

}  // namespace re2dj::hle::modules
