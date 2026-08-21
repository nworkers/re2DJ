#ifndef RE2DJ_HDD_HDD_ROOT_H_
#define RE2DJ_HDD_HDD_ROOT_H_

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace re2dj::hdd
{

// The original HDD contents, supplied by the user as a directory path rather
// than a disk image. The repository never carries these files, so every path in
// this class is host-side and owned by the user.
//
// The class exists mostly for one reason: the guest was compiled for Windows
// and opens files with whatever case its source happened to use, while Linux
// and Web hosts have case-sensitive file systems. Resolve() bridges that gap.
class HddRoot
{
public:
    HddRoot() = default;

    // Validates that `root` exists and is a directory, then stores its
    // canonical form. On failure `error` receives a message suitable for
    // printing to the user.
    static bool Open(const std::filesystem::path& root,
                     HddRoot* out,
                     std::string* error);

    bool is_open() const
    {
        return !root_.empty();
    }

    const std::filesystem::path& root() const
    {
        return root_;
    }

    // Resolves a '/'-separated path relative to the root. Each component is
    // matched against the parent directory's listing: an exact match wins, and
    // otherwise the first case-insensitive match does. The result therefore
    // carries the on-disk spelling on every host. An empty path resolves to the
    // root itself. Returns false when any component has no match.
    bool Resolve(std::string_view relative_path, std::filesystem::path* out) const;

    // Resolve() plus a check that the result is a regular file.
    bool ResolveFile(std::string_view relative_path, std::filesystem::path* out) const;

    // Resolve() plus a check that the result is a directory.
    bool ResolveDirectory(std::string_view relative_path,
                          std::filesystem::path* out) const;

    // Drops the cached directory listings. Call this after the host writes
    // into the tree, which currently only tools do.
    void ClearCache() const;

private:
    // Cached listing for `directory`, or nullptr when it cannot be read.
    const std::vector<std::string>* DirectoryEntries(
        const std::filesystem::path& directory) const;

    std::filesystem::path root_;
    mutable std::map<std::string, std::vector<std::string>> entry_cache_;
};

}  // namespace re2dj::hdd

#endif  // RE2DJ_HDD_HDD_ROOT_H_
