#include "re2dj/hle/hardlock/protocol.h"

#include <cstdint>
#include <string>

#include "test_support.h"

void RunHardlockProtocolTests(re2dj::test::Context& context)
{
    using re2dj::hle::hardlock::ClassifyHardlockRequest;
    using re2dj::hle::hardlock::HardlockRequestKind;
    using re2dj::hle::hardlock::HardlockRequestKindName;

    // The four confirmed control codes, and nothing else, are claimed.
    RE2DJ_CHECK_EQ(context,
                   ClassifyHardlockRequest(re2dj::hle::hardlock::kHardlockIoctlInitialize),
                   HardlockRequestKind::kInitialize);
    RE2DJ_CHECK_EQ(context,
                   ClassifyHardlockRequest(re2dj::hle::hardlock::kHardlockIoctlHandshake),
                   HardlockRequestKind::kHandshake);
    RE2DJ_CHECK_EQ(context,
                   ClassifyHardlockRequest(re2dj::hle::hardlock::kHardlockIoctlDescriptor),
                   HardlockRequestKind::kDescriptor);
    RE2DJ_CHECK_EQ(context,
                   ClassifyHardlockRequest(re2dj::hle::hardlock::kHardlockIoctlTransform),
                   HardlockRequestKind::kTransform);

    // A neighbouring code in the same device range is not claimed: answering a
    // request the driver contract does not cover would be an invention.
    RE2DJ_CHECK_EQ(context,
                   ClassifyHardlockRequest(0x9c402454),
                   HardlockRequestKind::kUnknown);
    RE2DJ_CHECK_EQ(context, ClassifyHardlockRequest(0), HardlockRequestKind::kUnknown);

    RE2DJ_CHECK_EQ(context,
                   std::string(HardlockRequestKindName(HardlockRequestKind::kTransform)),
                   std::string("transform"));
    RE2DJ_CHECK_EQ(context,
                   std::string(HardlockRequestKindName(HardlockRequestKind::kUnknown)),
                   std::string("unknown"));
}
