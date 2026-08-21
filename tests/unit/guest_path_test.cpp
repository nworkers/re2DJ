#include "re2dj/storage/guest_path.h"

#include "test_support.h"

using re2dj::storage::CombineGuestPath;
using re2dj::storage::EqualsIgnoreAsciiCase;
using re2dj::storage::GuestPath;
using re2dj::storage::GuestPathKind;
using re2dj::storage::GuestPathToRelativeString;
using re2dj::storage::GuestPathToString;
using re2dj::storage::NormalizeGuestPath;
using re2dj::storage::ParseGuestPath;

namespace
{

GuestPath Parse(const char* text)
{
    GuestPath path;
    ParseGuestPath(text, &path);
    return path;
}

}  // namespace

void RunGuestPathTests(re2dj::test::Context& context)
{
    GuestPath path;

    RE2DJ_CHECK(context, ParseGuestPath("C:\\EZ2DJ\\DATA\\SONG.EZ", &path));
    RE2DJ_CHECK(context, path.kind == GuestPathKind::kDriveAbsolute);
    RE2DJ_CHECK_EQ(context, path.drive_letter, 'C');
    RE2DJ_CHECK_EQ(context, path.components.size(), std::size_t{3});
    RE2DJ_CHECK_EQ(context, GuestPathToRelativeString(path), std::string("EZ2DJ/DATA/SONG.EZ"));

    // A lowercase drive letter normalises to uppercase so two spellings of the
    // same drive compare equal.
    RE2DJ_CHECK(context, ParseGuestPath("c:\\ez2dj", &path));
    RE2DJ_CHECK_EQ(context, path.drive_letter, 'C');

    // Guest code mixes separators; both must parse the same way.
    RE2DJ_CHECK(context, ParseGuestPath("DATA/SUB\\FILE.DAT", &path));
    RE2DJ_CHECK(context, path.kind == GuestPathKind::kRelative);
    RE2DJ_CHECK_EQ(context, path.components.size(), std::size_t{3});

    RE2DJ_CHECK(context, ParseGuestPath("\\DATA", &path));
    RE2DJ_CHECK(context, path.kind == GuestPathKind::kRootRelative);

    RE2DJ_CHECK(context, ParseGuestPath("C:DATA", &path));
    RE2DJ_CHECK(context, path.kind == GuestPathKind::kDriveRelative);

    // UNC and empty input are rejected rather than resolved to something wrong.
    RE2DJ_CHECK(context, !ParseGuestPath("\\\\server\\share\\x", &path));
    RE2DJ_CHECK(context, !ParseGuestPath("", &path));
    RE2DJ_CHECK(context, !ParseGuestPath("DATA\\BAD?NAME", &path));

    // Normalisation folds "." and "..".
    path = Parse("C:\\A\\.\\B\\..\\C");
    RE2DJ_CHECK(context, NormalizeGuestPath(&path));
    RE2DJ_CHECK_EQ(context, GuestPathToString(path), std::string("C:\\A\\C"));

    // Escaping the root fails instead of clamping, so no guest path can reach
    // outside the user's HDD directory.
    path = Parse("C:\\..\\OUTSIDE");
    RE2DJ_CHECK(context, !NormalizeGuestPath(&path));
    path = Parse("..\\OUTSIDE");
    RE2DJ_CHECK(context, !NormalizeGuestPath(&path));

    // Win32 drops trailing dots and spaces when opening a name.
    path = Parse("C:\\DATA.\\FILE.DAT ");
    RE2DJ_CHECK(context, NormalizeGuestPath(&path));
    RE2DJ_CHECK_EQ(context, GuestPathToRelativeString(path), std::string("DATA/FILE.DAT"));

    // Combining against the guest current directory.
    const GuestPath cwd = Parse("C:\\EZ2DJ");
    GuestPath combined;

    RE2DJ_CHECK(context, CombineGuestPath(cwd, Parse("DATA\\SONG.EZ"), &combined));
    RE2DJ_CHECK_EQ(context, GuestPathToString(combined), std::string("C:\\EZ2DJ\\DATA\\SONG.EZ"));

    RE2DJ_CHECK(context, CombineGuestPath(cwd, Parse("\\SYSTEM\\CFG.DAT"), &combined));
    RE2DJ_CHECK_EQ(context, GuestPathToString(combined), std::string("C:\\SYSTEM\\CFG.DAT"));

    RE2DJ_CHECK(context, CombineGuestPath(cwd, Parse("D:\\OTHER"), &combined));
    RE2DJ_CHECK_EQ(context, combined.drive_letter, 'D');

    RE2DJ_CHECK(context, CombineGuestPath(cwd, Parse("C:SUB"), &combined));
    RE2DJ_CHECK_EQ(context, GuestPathToString(combined), std::string("C:\\EZ2DJ\\SUB"));

    // Only one guest drive is mounted, so a drive-relative path on another
    // drive has no current directory to resolve against.
    RE2DJ_CHECK(context, !CombineGuestPath(cwd, Parse("D:SUB"), &combined));

    // A relative path may not climb out of the mounted root.
    RE2DJ_CHECK(context, !CombineGuestPath(cwd, Parse("..\\..\\OUTSIDE"), &combined));

    RE2DJ_CHECK(context, EqualsIgnoreAsciiCase("Song01.EZ", "SONG01.ez"));
    RE2DJ_CHECK(context, !EqualsIgnoreAsciiCase("SONG01.EZ", "SONG02.EZ"));
    RE2DJ_CHECK(context, !EqualsIgnoreAsciiCase("SONG", "SONG1"));
    // Bytes at or above 0x80 must compare exactly, so multi-byte code-page
    // names such as CP949 are never folded into a different name.
    RE2DJ_CHECK(context, !EqualsIgnoreAsciiCase("\xB0\xA1", "\xB0\xA2"));
}
