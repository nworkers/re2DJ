#include "re2dj/version.h"

#include <string>

#include "test_support.h"

namespace
{

// The label names the OS and architecture the test binary was built for, then
// the CMake configuration, such as "Linux/x64 Debug".
void CheckBuildLabel(re2dj::test::Context& context)
{
#if defined(_WIN32)
    const std::string os = "Win";
#else
    const std::string os = "Linux";
#endif
    const std::string architecture = sizeof(void*) == 8 ? "x64" : "x86";
    const std::string label = re2dj::BuildLabel();
    RE2DJ_CHECK_EQ(context, label, os + "/" + architecture + " " + RE2DJ_BUILD_CONFIG);
    // CMake always passes a configuration to this build.
    RE2DJ_CHECK(context, std::string(RE2DJ_BUILD_CONFIG) == "Debug" ||
                             std::string(RE2DJ_BUILD_CONFIG) == "Release" ||
                             std::string(RE2DJ_BUILD_CONFIG) == "RelWithDebInfo" ||
                             std::string(RE2DJ_BUILD_CONFIG) == "MinSizeRel");
}

// The banner every program and the window title start with.
void CheckVersionBanner(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, re2dj::VersionBanner("re2DJ", "0.0.52"),
                   "re2DJ v0.0.52 (" + re2dj::BuildLabel() + ")");
    RE2DJ_CHECK_EQ(context, re2dj::VersionBanner("re2dj_pe_loader", "1.2.3"),
                   "re2dj_pe_loader v1.2.3 (" + re2dj::BuildLabel() + ")");
}

// The window title every host shows: the banner, the build date, the
// renderer, and the frame rate to one decimal.
void CheckWindowTitle(re2dj::test::Context& context)
{
    const std::string title = re2dj::WindowTitle("0.0.52", 59.94);
    const std::string banner = re2dj::VersionBanner("re2DJ", "0.0.52");
    RE2DJ_CHECK_EQ(context, title.rfind(banner + " - Build ", 0), std::size_t{0});
    RE2DJ_CHECK(context, title.find(" - SDL3 OpenGL - FPS : 59.9") != std::string::npos);
}

}  // namespace

void RunVersionTests(re2dj::test::Context& context)
{
    CheckBuildLabel(context);
    CheckVersionBanner(context);
    CheckWindowTitle(context);
}
