#include "chd_extract.h"

#include <filesystem>
#include <string>

#include "test_support.h"

void RunChdExtractNameTests(re2dj::test::Context& context)
{
    // FAT long names decode to UTF-8. Building a host path component from those
    // bytes as a plain std::string reaches std::filesystem::path as the host's
    // narrow encoding, which on Windows is an ANSI code page. The ez2d2m image
    // holds a Korean-named directory whose UTF-8 bytes are not valid there, and
    // the conversion did not fail: it never returned. These checks pin the
    // conversion that replaced it, so both the hang and a silently mangled name
    // stay caught.
    const auto round_trip = [](std::string_view utf8_name) {
        const std::filesystem::path host = re2dj::tools::ChdEntryHostName(utf8_name);
        const std::u8string back = host.u8string();
        return std::string(back.begin(), back.end());
    };

    RE2DJ_CHECK_EQ(context, round_trip("EZ2Dancer.exe"), std::string("EZ2Dancer.exe"));
    RE2DJ_CHECK_EQ(context, round_trip(""), std::string(""));
    RE2DJ_CHECK_EQ(context, round_trip("Web Server Extensions"),
                   std::string("Web Server Extensions"));
    // The directory that exposed the bug, written as its UTF-8 bytes so this
    // file itself stays ASCII.
    const std::string korean_folder = "\xec\x9b\xb9 \xed\x8f\xb4\xeb\x8d\x94";
    RE2DJ_CHECK_EQ(context, round_trip(korean_folder), korean_folder);
    RE2DJ_CHECK(context, !re2dj::tools::ChdEntryHostName(korean_folder).empty());
    // A name is one component, never a path: joining must not escape upward.
    RE2DJ_CHECK(context,
                !re2dj::tools::ChdEntryHostName("FONTKR.DAT").has_root_directory());
}
