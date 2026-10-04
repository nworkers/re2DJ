#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_PROBE_FIXTURE_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_PROBE_FIXTURE_H_

// The synthetic PE32 the native execution probes of every host load: two
// imports of probe.dll (ProbeGate by name, ordinal 7), a TLS callback that
// checks FS and the TEB stack bounds, and relocations. Host-neutral; each
// OS's probes include it from here.

#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/hle/import_dispatcher.h"

namespace re2dj::platform::native_probe
{

inline constexpr std::uint32_t kImageBase = 0x10000000;
// The base the probes ask for, so the relocations run.
inline constexpr std::uint32_t kRequestedBase = 0x11000000;
inline constexpr std::uint32_t kEntryRva = 0x1000;
inline constexpr std::uint32_t kIatRva = 0x2040;
inline constexpr std::uint32_t kTlsCallbackRva = 0x1020;
inline constexpr std::uint32_t kTlsCallbacksRva = 0x3020;
inline constexpr std::uint32_t kTlsStateRva = 0x3030;

// The entry calls ProbeGate with 41 and ordinal 7 with its result, adds the
// TLS state to the answer, and returns it.
std::vector<std::uint8_t> MakeSyntheticPe32();

// Answers ProbeGate(41) with 42 and ordinal 7 (42) with 43 and edx 1.
bool ProbeImportHandler(const re2dj::hle::ImportCall& call,
                        re2dj::hle::ImportReturn* result,
                        std::string* error);

}  // namespace re2dj::platform::native_probe

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_PROBE_FIXTURE_H_
