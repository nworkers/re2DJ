#include "native_legacy_io.h"

#include <cstdint>
#include <array>
#include <optional>

#include <time.h>

#include "re2dj/input/ez2dancer_io_port_bus.h"
#include "re2dj/input/ez2dancer_keyboard_map.h"
#include "re2dj/input/ez2dj_keyboard_map.h"
#include "re2dj/input/legacy_io_port_bus.h"
#include "re2dj/input/virtual_keys.h"

namespace re2dj::platform::linux
{
namespace
{

// The run's board and the policy that routes guest faults to it. Both are
// set before the guest starts and read only from its signal handler, which
// runs on the guest thread holding the guest lock.
input::LegacyIoTrapPolicy g_policy;
// The byte-wide EZ2DJ board, or the word-wide EZ2Dancer board when the
// profile's width is a word.
input::LegacyIoPortBus g_bus;
input::Ez2DancerIoPortBus g_dancer_bus;
NativeLegacyIoActivity g_activity;
// The host's keys and the built-in key map, resolved to virtual keys before
// the guest starts so the signal handler only looks them up.
const hle::HostInputState* g_input = nullptr;
std::array<int, static_cast<std::size_t>(input::Ez2DjButton::kCount)> g_button_keys = {};
std::array<int, 4> g_turntable_keys = {};
std::array<int, static_cast<std::size_t>(input::Ez2DancerButton::kCount)> g_dancer_keys = {};
input::Ez2DjTurntables g_turntables;

bool Held(int virtual_key)
{
    return virtual_key > 0 && virtual_key < 256 && g_input->virtual_keys.test(static_cast<std::size_t>(virtual_key));
}

// Sets the board's inputs from the held keys. Async-signal-safe.
void PollHostInput()
{
    if (g_input == nullptr)
    {
        return;
    }
    if (g_policy.word_width)
    {
        for (std::size_t index = 0; index < g_dancer_keys.size(); ++index)
        {
            g_dancer_bus.SetButton(static_cast<input::Ez2DancerButton>(index), Held(g_dancer_keys[index]));
        }
        return;
    }
    for (std::size_t index = 0; index < g_button_keys.size(); ++index)
    {
        g_bus.SetButton(static_cast<input::Ez2DjButton>(index), Held(g_button_keys[index]));
    }
    std::array<bool, 4> held = {};
    for (std::size_t index = 0; index < held.size(); ++index)
    {
        held[index] = Held(g_turntable_keys[index]);
    }
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    const auto now_ms = static_cast<std::uint64_t>(now.tv_sec) * 1000U + static_cast<std::uint64_t>(now.tv_nsec) / 1000000U;
    g_turntables.Update(&g_bus, now_ms, held, input::kEz2DjDefaultTurntableStep);
}

}  // namespace

void SetNativeLegacyIo(const input::LegacyIoTrapPolicy& policy, const hle::HostInputState* host_input)
{
    g_input = host_input;
    for (const input::Ez2DjButtonBinding& binding : input::Ez2DjButtonBindings())
    {
        g_button_keys[static_cast<std::size_t>(binding.button)] = input::ParseKeyName(binding.default_key);
    }
    for (std::size_t index = 0; index < g_turntable_keys.size(); ++index)
    {
        g_turntable_keys[index] = input::ParseKeyName(input::Ez2DjTurntableBindings()[index].default_key);
    }
    for (const input::Ez2DancerButtonBinding& binding : input::Ez2DancerButtonBindings())
    {
        g_dancer_keys[static_cast<std::size_t>(binding.button)] = input::ParseKeyName(binding.default_key);
    }
    g_turntables = input::Ez2DjTurntables();
    g_policy = policy;
    g_bus = input::LegacyIoPortBus();
    g_dancer_bus = input::Ez2DancerIoPortBus();
    g_activity = {};
}

void ClearNativeLegacyIo()
{
    g_policy = {};
    g_input = nullptr;
}

bool HandleNativeLegacyIoTrap(NativeTrapRegisters* registers)
{
    if (!g_policy.enabled)
    {
        return false;
    }
    // The guest image is mapped at its own addresses in this process, and the
    // faulting instruction was just fetched from there.
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(registers->eip));
    const std::optional<input::LegacyIoAccess> access = input::DecodeLegacyIoAccess(
        g_policy, registers->eip, instruction[0], instruction[0] == 0x66 ? instruction[1] : std::uint8_t{0});
    if (!access.has_value())
    {
        return false;
    }
    const auto port = static_cast<std::uint16_t>(registers->edx);
    if (access->read)
    {
        PollHostInput();
    }
    std::uint16_t value = 0;
    bool answered = false;
    if (access->word)
    {
        value = static_cast<std::uint16_t>(registers->eax);
        answered = access->read ? g_dancer_bus.ReadWord(port, &value) : g_dancer_bus.WriteWord(port, value);
    }
    else
    {
        std::uint8_t byte = static_cast<std::uint8_t>(registers->eax);
        answered = access->read ? g_bus.ReadByte(port, &byte) : g_bus.WriteByte(port, byte);
        value = byte;
    }
    if (g_activity.reads + g_activity.writes + g_activity.unanswered == 0)
    {
        g_activity.first_port = port;
        g_activity.first_read = access->read;
    }
    if (!answered)
    {
        ++g_activity.unanswered;
        return false;
    }
    ++(access->read ? g_activity.reads : g_activity.writes);
    if (access->read)
    {
        registers->eax = input::MergeLegacyIoRead(*access, registers->eax, value);
    }
    registers->eip += access->length;
    return true;
}

NativeLegacyIoActivity NativeLegacyIoActivitySnapshot()
{
    return g_activity;
}

}  // namespace re2dj::platform::linux
