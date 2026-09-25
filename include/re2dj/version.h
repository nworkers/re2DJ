#ifndef RE2DJ_VERSION_H_
#define RE2DJ_VERSION_H_

#include <cstdio>
#include <string>
#include <string_view>

// The host a build runs on and how it was built, as the window title shows
// them. Header-only so the Windows injected runtime, which does not link the
// core, uses the same text.
#if defined(_WIN32)
#define RE2DJ_HOST_OS_LABEL "Win"
#elif defined(__linux__)
#define RE2DJ_HOST_OS_LABEL "Linux"
#else
#define RE2DJ_HOST_OS_LABEL "Unknown"
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define RE2DJ_HOST_ARCH_LABEL "x64"
#elif defined(_M_IX86) || defined(__i386__)
#define RE2DJ_HOST_ARCH_LABEL "x86"
#else
#define RE2DJ_HOST_ARCH_LABEL "unknown"
#endif

// CMake passes the configuration name (Debug, Release, ...); a build outside
// CMake falls back to what NDEBUG says.
#ifndef RE2DJ_BUILD_CONFIG
#ifdef NDEBUG
#define RE2DJ_BUILD_CONFIG "Release"
#else
#define RE2DJ_BUILD_CONFIG "Debug"
#endif
#endif

namespace re2dj
{

// Value of the repository-root VERSION file, injected by CMake at build time.
std::string_view VersionString();

// "Win/x86 Debug", "Linux/x64 Release", and so on: the host OS and
// architecture this binary was built for, then its build configuration when
// one is known.
inline std::string BuildLabel()
{
    const std::string_view configuration = RE2DJ_BUILD_CONFIG;
    std::string label = RE2DJ_HOST_OS_LABEL "/" RE2DJ_HOST_ARCH_LABEL;
    if (!configuration.empty())
    {
        label += ' ';
        label += configuration;
    }
    return label;
}

// "<program> v<version> (<BuildLabel()>)", such as
// "re2DJ v0.0.52 (Win/x86 Debug)": how every program and the window title name
// the build. The version is a parameter because the Windows injected runtime
// has only the RE2DJ_VERSION macro, not VersionString().
inline std::string VersionBanner(std::string_view program, std::string_view version)
{
    std::string banner(program);
    banner += " v";
    banner += version;
    banner += " (";
    banner += BuildLabel();
    banner += ')';
    return banner;
}

// The host window's title on every platform:
// "re2DJ v0.0.52 (Linux/x64 Debug) - Build Sep 26 2026 - SDL3 OpenGL - FPS : 60.0".
// The build date is that of the file that includes this header.
inline std::string WindowTitle(std::string_view version, double fps)
{
    char tail[96] = {};
    std::snprintf(tail, sizeof(tail), " - Build %s - SDL3 OpenGL - FPS : %.1f", __DATE__, fps);
    return VersionBanner("re2DJ", version) + tail;
}

}  // namespace re2dj

#endif  // RE2DJ_VERSION_H_
