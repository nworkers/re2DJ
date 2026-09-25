#include <cstdio>

#include "test_support.h"

int main()
{
    re2dj::test::Context context;

    RunAddressSpaceTests(context);
    RunCodeRegionScoreTests(context);
    RunCodeScanTests(context);
    RunPortHelperScanTests(context);
    RunImmediateScanTests(context);
    RunExecutionBackendTests(context);
    RunLoggingTests(context);
    RunVersionTests(context);
    RunImportDispatcherTests(context);
    RunGuestModuleRegistryTests(context);
    RunGuestPeFacadeTests(context);
    RunKernel32ModuleTests(context);
    RunUser32ModuleTests(context);
    RunDdrawModuleTests(context);
    RunDirectXDisplayTests(context);
    RunGuestProcessTests(context);
    RunKernel32CrtTests(context);
    RunApiCallRecordTests(context);
    RunGuestFilesTests(context);
    RunImportEventLoopTests(context);
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
    RunHardlockPayloadResponsesTests(context);
    RunHardlockTransformResponsesTests(context);
    RunHardlockEngineTests(context);
    RunHardlockMaterialConfigTests(context);
    RunHardlockDeviceMaterialTests(context);
    RunEz2DjIoBoardTests(context);
    RunEz2DancerIoBoardTests(context);
    RunLegacyIoPortBusTests(context);
    RunLegacyDrawCommandTests(context);
    RunPresentationFilterTests(context);
    RunPresentIntervalHistogramTests(context);
    RunPresentSyncTests(context);
    RunLegacyTextureTests(context);
    RunLegacyTransformTests(context);
    RunLegacyVertexBufferTests(context);
    RunLegacyAudioBufferTests(context);
    RunDirectSoundBufferPolicyTests(context);
    RunMameChdTests(context);
    RunFat32ChdTests(context);
    RunChdHunkCacheTests(context);
    RunFat32DirectoryNameTests(context);
    RunChdExtractNameTests(context);

    std::printf("checks: %d, failures: %d\n", context.checks, context.failures);
    return context.failures == 0 ? 0 : 1;
}
