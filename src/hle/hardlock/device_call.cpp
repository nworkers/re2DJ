#include "re2dj/hle/hardlock/device_call.h"

#include <cstdio>

#include "re2dj/hle/hardlock/protocol.h"

namespace re2dj::hle::hardlock
{

HardlockDeviceCall CompleteHardlockDeviceIoControl(HardlockDevice& device,
                                                   std::uint32_t control_code,
                                                   std::span<const std::uint8_t> input,
                                                   std::span<std::uint8_t> output)
{
    HardlockDeviceCall call;
    call.result = device.Complete(control_code, input, output);
    switch (call.result.outcome)
    {
        case HardlockOutcome::kNotHandled:
            return call;
        case HardlockOutcome::kRejectedShape:
            call.handled = true;
            call.succeeded = false;
            call.bytes_returned = 0;
            call.win32_error = kWin32ErrorInvalidData;
            return call;
        case HardlockOutcome::kCompleted:
            call.handled = true;
            call.succeeded = true;
            call.bytes_returned = static_cast<std::uint32_t>(call.result.bytes_written);
            call.win32_error = kWin32ErrorSuccess;
            return call;
    }
    return call;
}

void RecordHardlockDeviceCall(const HardlockDeviceResult& result,
                              std::uint64_t tick_ms,
                              HardlockDeviceActivity* activity)
{
    if (activity == nullptr)
    {
        return;
    }
    ++activity->total;
    switch (result.kind)
    {
        case HardlockRequestKind::kInitialize:
            ++activity->initialize;
            break;
        case HardlockRequestKind::kHandshake:
            ++activity->handshake;
            break;
        case HardlockRequestKind::kDescriptor:
            ++activity->descriptor;
            break;
        case HardlockRequestKind::kTransform:
            ++activity->transform;
            break;
        default:
            ++activity->other;
            break;
    }
    if (result.outcome == HardlockOutcome::kRejectedShape)
    {
        ++activity->rejected;
    }
    activity->last_kind = HardlockRequestKindName(result.kind);
    activity->last_outcome = HardlockOutcomeName(result.outcome);
    activity->last_bytes = static_cast<unsigned>(result.bytes_written);
    activity->last_tick = tick_ms;
}

std::string FormatHardlockDeviceTrace(const HardlockDeviceResult& result, std::uint64_t tick_ms)
{
    char message[256] = {};
    std::snprintf(message,
                  sizeof(message),
                  "re2dj:vfs:hardlock-device:request=%s:outcome=%s:bytes=%u:"
                  "handshake_answered=%u:status_cleared=%u:tail=%u:"
                  "mapped=%u:unmapped=%u:payload=%u:tick_ms=%llu\r\n",
                  HardlockRequestKindName(result.kind),
                  HardlockOutcomeName(result.outcome),
                  static_cast<unsigned>(result.bytes_written),
                  result.handshake_answered ? 1U : 0U,
                  result.descriptor_status_cleared ? 1U : 0U,
                  result.descriptor_tail_written ? 1U : 0U,
                  static_cast<unsigned>(result.transform_blocks_mapped),
                  static_cast<unsigned>(result.transform_blocks_unmapped),
                  result.transform_payload_mapped ? 1U : 0U,
                  static_cast<unsigned long long>(tick_ms));
    return message;
}

}  // namespace re2dj::hle::hardlock
