#ifndef RE2DJ_TESTS_UNIT_TEST_SUPPORT_H_
#define RE2DJ_TESTS_UNIT_TEST_SUPPORT_H_

#include <cstdio>
#include <string>

// A deliberately small harness. Adding a test framework is a license and
// build-time decision that belongs in its own design note rather than in this
// scaffolding.
namespace re2dj::test
{

struct Context
{
    int checks = 0;
    int failures = 0;

    void Check(bool condition, const char* expression, const char* file, int line)
    {
        ++checks;
        if (condition)
        {
            return;
        }
        ++failures;
        std::fprintf(stderr, "%s:%d: FAILED %s\n", file, line, expression);
    }

    void Fail(const std::string& message, const char* file, int line)
    {
        ++checks;
        ++failures;
        std::fprintf(stderr, "%s:%d: FAILED %s\n", file, line, message.c_str());
    }
};

}  // namespace re2dj::test

#define RE2DJ_CHECK(context, expression) \
    (context).Check((expression), #expression, __FILE__, __LINE__)

#define RE2DJ_CHECK_EQ(context, actual, expected)                                \
    do                                                                           \
    {                                                                            \
        const auto& re2dj_actual = (actual);                                     \
        const auto& re2dj_expected = (expected);                                 \
        if (!(re2dj_actual == re2dj_expected))                                   \
        {                                                                        \
            (context).Fail(std::string(#actual) + " != " + #expected,            \
                           __FILE__,                                             \
                           __LINE__);                                            \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            (context).Check(true, #actual, __FILE__, __LINE__);                  \
        }                                                                        \
    } while (false)

void RunCodeRegionScoreTests(re2dj::test::Context& context);
void RunCodeScanTests(re2dj::test::Context& context);
void RunImmediateScanTests(re2dj::test::Context& context);
void RunGuestPathTests(re2dj::test::Context& context);
void RunAddressSpaceTests(re2dj::test::Context& context);
void RunExecutionBackendTests(re2dj::test::Context& context);
void RunPeImageTests(re2dj::test::Context& context);
void RunPeLoaderTests(re2dj::test::Context& context);
void RunHddRootTests(re2dj::test::Context& context);
void RunTargetProfileTests(re2dj::test::Context& context);
void RunVfsFileTableTests(re2dj::test::Context& context);
void RunLptdiChallengeResponseTests(re2dj::test::Context& context);
void RunLptdiResponseProfileTests(re2dj::test::Context& context);
void RunHardlockHandshakeResponseTests(re2dj::test::Context& context);
void RunHardlockApiDescriptorTests(re2dj::test::Context& context);
void RunHardlockProtocolTests(re2dj::test::Context& context);
void RunHardlockDeviceTests(re2dj::test::Context& context);
void RunHardlockTransformResponsesTests(re2dj::test::Context& context);
void RunHardlockMaterialConfigTests(re2dj::test::Context& context);
void RunLegacyIoPortBusTests(re2dj::test::Context& context);
void RunEz2DjIoBoardTests(re2dj::test::Context& context);
void RunLegacyDrawCommandTests(re2dj::test::Context& context);
void RunLegacyTextureTests(re2dj::test::Context& context);
void RunLegacyTransformTests(re2dj::test::Context& context);
void RunLegacyVertexBufferTests(re2dj::test::Context& context);
void RunLegacyAudioBufferTests(re2dj::test::Context& context);
void RunMameChdTests(re2dj::test::Context& context);
void RunFat32ChdTests(re2dj::test::Context& context);
void RunFat32DirectoryNameTests(re2dj::test::Context& context);

#endif  // RE2DJ_TESTS_UNIT_TEST_SUPPORT_H_
