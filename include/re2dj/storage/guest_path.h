#ifndef RE2DJ_STORAGE_GUEST_PATH_H_
#define RE2DJ_STORAGE_GUEST_PATH_H_

#include <string>
#include <string_view>
#include <vector>

namespace re2dj::storage
{

// Shape of a Win32-style path as the guest wrote it. The guest was built for
// Windows, so it may hand us any of these forms and each resolves against a
// different base.
enum class GuestPathKind
{
    // "DATA\SONG.EZ" resolves against the guest current directory.
    kRelative,
    // "\DATA\SONG.EZ" resolves against the root of the current drive.
    kRootRelative,
    // "C:DATA" resolves against the per-drive current directory. Rare, but
    // legal, and silently mis-resolving it would read the wrong file.
    kDriveRelative,
    // "C:\DATA\SONG.EZ" is fully qualified.
    kDriveAbsolute,
    // "\\server\share\..." has no meaning for a self-contained HDD dump.
    kUnc,
};

struct GuestPath
{
    GuestPathKind kind = GuestPathKind::kRelative;
    // Uppercase drive letter, or '\0' when the path carries no drive.
    char drive_letter = '\0';
    std::vector<std::string> components;
};

// Parses a Win32-style guest path. Accepts both '\' and '/' as separators
// because guest code mixes them. Returns false for an empty string, for a UNC
// path, or for a component holding a character Win32 forbids in a file name.
bool ParseGuestPath(std::string_view text, GuestPath* out);

// Folds "." away and applies "..". Returns false when ".." would climb above
// the root, which is how a path that escapes the HDD directory is rejected.
bool NormalizeGuestPath(GuestPath* path);

// Resolves `path` against `current_directory` and normalizes the result.
// `current_directory` must be drive-absolute. A drive-absolute `path` is used
// as-is; a drive-relative path keeps its own drive letter but is appended to
// the current directory only when the drives match, because this project mounts
// exactly one guest drive.
bool CombineGuestPath(const GuestPath& current_directory,
                      const GuestPath& path,
                      GuestPath* out);

// Renders the path back to Win32 syntax with '\' separators, for logs and for
// error messages that should echo what the guest asked for.
std::string GuestPathToString(const GuestPath& path);

// Renders the components alone, '/'-separated, as a relative host path. The
// drive letter and rooted-ness are dropped, so the result is always relative to
// the HDD root. Returns an empty string when the path has no components.
std::string GuestPathToRelativeString(const GuestPath& path);

// True when the two names refer to the same file under Win32 rules. Only ASCII
// letters fold; bytes at or above 0x80 compare exactly, which keeps multi-byte
// code-page file names such as CP949 matching byte-for-byte.
bool EqualsIgnoreAsciiCase(std::string_view left, std::string_view right);

}  // namespace re2dj::storage

#endif  // RE2DJ_STORAGE_GUEST_PATH_H_
