#include <cstdio>

#include "test_support.h"

int main()
{
    re2dj::test::Context context;

    RunAddressSpaceTests(context);
    RunCodeRegionScoreTests(context);
    RunExecutionBackendTests(context);
    RunGuestPathTests(context);
    RunPeImageTests(context);
    RunPeLoaderTests(context);
    RunHddRootTests(context);
    RunTargetProfileTests(context);
    RunVfsFileTableTests(context);
    RunLptdiChallengeResponseTests(context);
    RunLptdiResponseProfileTests(context);
    RunHardlockHandshakeResponseTests(context);
    RunHardlockApiDescriptorTests(context);
    RunHardlockProtocolTests(context);
    RunHardlockDeviceTests(context);
    RunHardlockTransformResponsesTests(context);
    RunHardlockMaterialConfigTests(context);
    RunEz2DjIoBoardTests(context);
    RunLegacyIoPortBusTests(context);
    RunLegacyDrawCommandTests(context);
    RunLegacyTextureTests(context);
    RunLegacyTransformTests(context);
    RunLegacyVertexBufferTests(context);
    RunLegacyAudioBufferTests(context);
    RunMameChdTests(context);
    RunFat32ChdTests(context);

    std::printf("checks: %d, failures: %d\n", context.checks, context.failures);
    return context.failures == 0 ? 0 : 1;
}
