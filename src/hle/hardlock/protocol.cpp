#include "re2dj/hle/hardlock/protocol.h"

namespace re2dj::hle::hardlock
{

HardlockRequestKind ClassifyHardlockRequest(std::uint32_t control_code)
{
    switch (control_code)
    {
    case kHardlockIoctlInitialize:
        return HardlockRequestKind::kInitialize;
    case kHardlockIoctlHandshake:
        return HardlockRequestKind::kHandshake;
    case kHardlockIoctlDescriptor:
        return HardlockRequestKind::kDescriptor;
    case kHardlockIoctlTransform:
        return HardlockRequestKind::kTransform;
    default:
        return HardlockRequestKind::kUnknown;
    }
}

const char* HardlockRequestKindName(HardlockRequestKind kind)
{
    switch (kind)
    {
    case HardlockRequestKind::kInitialize:
        return "initialize";
    case HardlockRequestKind::kHandshake:
        return "handshake";
    case HardlockRequestKind::kDescriptor:
        return "descriptor";
    case HardlockRequestKind::kTransform:
        return "transform";
    case HardlockRequestKind::kUnknown:
        return "unknown";
    }
    return "unknown";
}

}  // namespace re2dj::hle::hardlock
