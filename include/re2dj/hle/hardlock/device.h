#ifndef RE2DJ_HLE_HARDLOCK_DEVICE_H_
#define RE2DJ_HLE_HARDLOCK_DEVICE_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "re2dj/hle/hardlock/engine.h"
#include "re2dj/hle/hardlock/handshake_response.h"
#include "re2dj/hle/hardlock/payload_responses.h"
#include "re2dj/hle/hardlock/protocol.h"
#include "re2dj/hle/hardlock/transform_responses.h"

namespace re2dj::hle::hardlock
{

// The Hardlock device boundary. This answers the four IOCTLs the protection
// issues, using material computed outside this repository: nothing here derives
// a response, and the Function 0x0e transform is not implemented. A request
// outside the statically confirmed vendor driver framing is rejected rather
// than forced to succeed.
enum class HardlockOutcome
{
    // The control code is outside the four-IOCTL contract.
    kNotHandled,
    // The control code is known but the buffers violate the driver framing.
    kRejectedShape,
    // A response was written.
    kCompleted,
};

struct HardlockDeviceOptions
{
    // Six-byte answer for the 0x450 handshake. Without it the request buffer is
    // preserved, which is the smallest possible invention.
    std::optional<HardlockHandshakeResponse> handshake_response;
    // Descriptor tail word written at kHardlockApiTailWordOffset. Without it the
    // tail is left as the guest wrote it.
    std::optional<std::uint16_t> descriptor_tail_word;
    // Clears the descriptor status word so the API call reads as success.
    bool clear_descriptor_status = true;
    // When configured, dynamically computes transforms and payloads in real time,
    // taking precedence over static pre-computed maps.
    std::optional<HardlockSeeds> seeds;
    // Function 0x0e responses keyed by challenge block. Nothing here derives
    // them; a block with no entry passes through unchanged.
    std::vector<HardlockTransformResponseEntry> transform_responses;
    // Rows that answer a whole transform payload. A matching row takes the
    // request, and the block lookup above is then not consulted for it.
    std::vector<HardlockPayloadResponseEntry> payload_responses;
    // Diagnostic only: reject any descriptor or transform whose header function
    // equals this value, instead of completing it. Used to test whether the
    // protection inspects a given function's answer by watching for retries.
    // Never set from a profile; only from an explicit launcher option.
    std::optional<std::uint16_t> reject_function;
};

struct HardlockDeviceResult
{
    HardlockOutcome outcome = HardlockOutcome::kNotHandled;
    HardlockRequestKind kind = HardlockRequestKind::kUnknown;
    std::size_t bytes_written = 0;
    bool handshake_answered = false;
    bool descriptor_status_cleared = false;
    bool descriptor_tail_written = false;
    // Blocks answered from the response map, and blocks the map did not cover.
    // An incomplete map makes the run meaningless, so both are reported.
    std::size_t transform_blocks_mapped = 0;
    std::size_t transform_blocks_unmapped = 0;
    // A request row answered the whole payload; the block counts stay zero.
    bool transform_payload_mapped = false;
    // The clean-room engine dynamically calculated the transform/payload.
    bool transform_dynamically_computed = false;
};

class HardlockDevice
{
public:
    HardlockDevice() = default;
    explicit HardlockDevice(const HardlockDeviceOptions& options);

    HardlockDeviceResult Complete(std::uint32_t control_code,
                                  std::span<const std::uint8_t> input,
                                  std::span<std::uint8_t> output);

private:
    HardlockDeviceOptions options_;
};

const char* HardlockOutcomeName(HardlockOutcome outcome);

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_DEVICE_H_
