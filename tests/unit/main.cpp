#include <cstdio>

#include "test_support.h"

int main()
{
    re2dj::test::Context context;

    RunAddressSpaceTests(context);
    RunExecutionBackendTests(context);
    RunGuestPathTests(context);
    RunPeImageTests(context);
    RunPeLoaderTests(context);
    RunHddRootTests(context);
    RunTargetProfileTests(context);
    RunVfsFileTableTests(context);
    RunLptdiChallengeResponseTests(context);
    RunLptdiResponseProfileTests(context);
    RunLegacyIoPortBusTests(context);

    std::printf("checks: %d, failures: %d\n", context.checks, context.failures);
    return context.failures == 0 ? 0 : 1;
}
