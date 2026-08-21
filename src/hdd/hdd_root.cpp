#include "re2dj/hdd/hdd_root.h"

#include <system_error>
#include <utility>

#include "re2dj/storage/guest_path.h"

namespace re2dj::hdd
{

namespace
{

std::vector<std::string> SplitRelative(std::string_view relative_path)
{
    std::vector<std::string> components;
    std::string current;
    for (const char value : relative_path)
    {
        if (value == '/' || value == '\\')
        {
            if (!current.empty())
            {
                components.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(value);
    }
    if (!current.empty())
    {
        components.push_back(std::move(current));
    }
    return components;
}

}  // namespace

bool HddRoot::Open(const std::filesystem::path& root, HddRoot* out, std::string* error)
{
    const auto fail = [error](std::string message)
    {
        if (error != nullptr)
        {
            *error = std::move(message);
        }
        return false;
    };

    if (out == nullptr)
    {
        return fail("internal error: output pointer is null");
    }
    if (root.empty())
    {
        return fail("HDD directory path is empty");
    }

    std::error_code code;
    const std::filesystem::file_status status = std::filesystem::status(root, code);
    if (code)
    {
        return fail("cannot read HDD directory '" + root.string() + "': " + code.message());
    }
    if (!std::filesystem::exists(status))
    {
        return fail("HDD directory does not exist: " + root.string());
    }
    if (!std::filesystem::is_directory(status))
    {
        return fail("HDD path is not a directory: " + root.string());
    }

    // weakly_canonical rather than canonical: the path already exists, but a
    // non-canonicalisable mount should still be usable rather than fatal.
    std::filesystem::path canonical = std::filesystem::weakly_canonical(root, code);
    if (code)
    {
        canonical = root;
    }

    out->root_ = std::move(canonical);
    out->entry_cache_.clear();
    return true;
}

const std::vector<std::string>* HddRoot::DirectoryEntries(
    const std::filesystem::path& directory) const
{
    const std::string key = directory.string();
    const auto cached = entry_cache_.find(key);
    if (cached != entry_cache_.end())
    {
        return &cached->second;
    }

    std::error_code code;
    std::filesystem::directory_iterator iterator(directory, code);
    if (code)
    {
        return nullptr;
    }

    std::vector<std::string> names;
    const std::filesystem::directory_iterator end;
    for (; iterator != end; iterator.increment(code))
    {
        if (code)
        {
            return nullptr;
        }
        names.push_back(iterator->path().filename().string());
    }

    const auto inserted = entry_cache_.emplace(key, std::move(names));
    return &inserted.first->second;
}

bool HddRoot::Resolve(std::string_view relative_path, std::filesystem::path* out) const
{
    if (out == nullptr || !is_open())
    {
        return false;
    }

    std::filesystem::path current = root_;
    for (const std::string& component : SplitRelative(relative_path))
    {
        if (component == ".")
        {
            continue;
        }
        if (component == "..")
        {
            // Callers normalise before reaching here. A surviving ".." would
            // walk outside the user's HDD directory, so it is refused.
            return false;
        }

        const std::vector<std::string>* names = DirectoryEntries(current);
        if (names == nullptr)
        {
            // The directory cannot be listed. Probe the name verbatim so a
            // traverse-only directory still resolves, at the cost of returning
            // the requested spelling rather than the on-disk one.
            std::filesystem::path candidate = current / component;
            std::error_code code;
            if (!std::filesystem::exists(candidate, code) || code)
            {
                return false;
            }
            current = std::move(candidate);
            continue;
        }

        // Always match against the listing, even on a case-insensitive host.
        // Probing the requested spelling first would succeed on Windows and
        // return that spelling, so the same dump would produce different paths
        // on Windows and Linux.
        const std::string* exact = nullptr;
        const std::string* folded = nullptr;
        for (const std::string& name : *names)
        {
            if (name == component)
            {
                exact = &name;
                break;
            }
            // A case-sensitive host can hold both "DATA" and "data". An exact
            // match wins; otherwise the first case-insensitive one does.
            if (folded == nullptr && storage::EqualsIgnoreAsciiCase(name, component))
            {
                folded = &name;
            }
        }

        const std::string* match = exact != nullptr ? exact : folded;
        if (match == nullptr)
        {
            return false;
        }
        current /= *match;
    }

    *out = std::move(current);
    return true;
}

bool HddRoot::ResolveFile(std::string_view relative_path,
                          std::filesystem::path* out) const
{
    std::filesystem::path resolved;
    if (!Resolve(relative_path, &resolved))
    {
        return false;
    }
    std::error_code code;
    if (!std::filesystem::is_regular_file(resolved, code) || code)
    {
        return false;
    }
    *out = std::move(resolved);
    return true;
}

bool HddRoot::ResolveDirectory(std::string_view relative_path,
                               std::filesystem::path* out) const
{
    std::filesystem::path resolved;
    if (!Resolve(relative_path, &resolved))
    {
        return false;
    }
    std::error_code code;
    if (!std::filesystem::is_directory(resolved, code) || code)
    {
        return false;
    }
    *out = std::move(resolved);
    return true;
}

void HddRoot::ClearCache() const
{
    entry_cache_.clear();
}

}  // namespace re2dj::hdd
