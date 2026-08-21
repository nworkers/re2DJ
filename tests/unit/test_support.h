#ifndef RE2DJ_TESTS_UNIT_TEST_SUPPORT_H_
#define RE2DJ_TESTS_UNIT_TEST_SUPPORT_H_

#include <cstdio>
#include <string>

// A deliberately small harness. The project has no third-party dependency yet,
// and adding a test framework is a license and build-time decision that belongs
// in its own design note rather than in the initial scaffolding.
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

void RunGuestPathTests(re2dj::test::Context& context);
void RunPeImageTests(re2dj::test::Context& context);
void RunHddRootTests(re2dj::test::Context& context);
void RunTargetProfileTests(re2dj::test::Context& context);

#endif  // RE2DJ_TESTS_UNIT_TEST_SUPPORT_H_
