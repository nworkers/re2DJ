#include "re2dj/storage/guest_path.h"

namespace re2dj::storage
{

namespace
{

char ToUpperAscii(char value)
{
    if (value >= 'a' && value <= 'z')
    {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

bool IsSeparator(char value)
{
    return value == '\\' || value == '/';
}

bool IsDriveLetter(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

// Characters Win32 rejects inside a path component. ':' is included because the
// only legal ':' is the drive separator, which the parser strips first.
bool IsForbiddenComponentCharacter(char value)
{
    const unsigned char byte = static_cast<unsigned char>(value);
    if (byte < 0x20)
    {
        return true;
    }
    switch (value)
    {
    case '<':
    case '>':
    case ':':
    case '"':
    case '|':
    case '?':
    case '*':
        return true;
    default:
        return false;
    }
}

bool SplitComponents(std::string_view text, std::vector<std::string>* out)
{
    std::string current;
    for (const char value : text)
    {
        if (IsSeparator(value))
        {
            if (!current.empty())
            {
                out->push_back(current);
                current.clear();
            }
            continue;
        }
        if (IsForbiddenComponentCharacter(value))
        {
            return false;
        }
        current.push_back(value);
    }
    if (!current.empty())
    {
        out->push_back(current);
    }
    return true;
}

}  // namespace

bool EqualsIgnoreAsciiCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (ToUpperAscii(left[index]) != ToUpperAscii(right[index]))
        {
            return false;
        }
    }
    return true;
}

bool ParseGuestPath(std::string_view text, GuestPath* out)
{
    if (out == nullptr || text.empty())
    {
        return false;
    }

    GuestPath parsed;

    if (text.size() >= 2 && IsSeparator(text[0]) && IsSeparator(text[1]))
    {
        parsed.kind = GuestPathKind::kUnc;
        *out = parsed;
        return false;
    }

    std::string_view rest = text;
    if (rest.size() >= 2 && IsDriveLetter(rest[0]) && rest[1] == ':')
    {
        parsed.drive_letter = ToUpperAscii(rest[0]);
        rest.remove_prefix(2);
        parsed.kind = (!rest.empty() && IsSeparator(rest[0]))
                          ? GuestPathKind::kDriveAbsolute
                          : GuestPathKind::kDriveRelative;
    }
    else if (IsSeparator(rest[0]))
    {
        parsed.kind = GuestPathKind::kRootRelative;
    }
    else
    {
        parsed.kind = GuestPathKind::kRelative;
    }

    if (!SplitComponents(rest, &parsed.components))
    {
        return false;
    }

    *out = parsed;
    return true;
}

bool NormalizeGuestPath(GuestPath* path)
{
    if (path == nullptr)
    {
        return false;
    }

    std::vector<std::string> folded;
    folded.reserve(path->components.size());
    for (const std::string& component : path->components)
    {
        if (component == ".")
        {
            continue;
        }
        if (component == "..")
        {
            if (!folded.empty())
            {
                folded.pop_back();
                continue;
            }
            // A rooted path cannot climb above its root, and an unrooted path
            // with a leading ".." would leave the HDD directory once resolved.
            // Both are refused rather than silently clamped.
            return false;
        }
        folded.push_back(component);
    }

    // Trailing dots and spaces are stripped by Win32 when a name is opened.
    // Reproduce that here so "DATA." and "DATA" resolve to the same entry.
    for (std::string& component : folded)
    {
        std::size_t end = component.size();
        while (end > 0 && (component[end - 1] == '.' || component[end - 1] == ' '))
        {
            --end;
        }
        if (end == 0)
        {
            return false;
        }
        component.resize(end);
    }

    path->components = std::move(folded);
    return true;
}

bool CombineGuestPath(const GuestPath& current_directory,
                      const GuestPath& path,
                      GuestPath* out)
{
    if (out == nullptr)
    {
        return false;
    }
    if (current_directory.kind != GuestPathKind::kDriveAbsolute)
    {
        return false;
    }
    if (path.kind == GuestPathKind::kUnc)
    {
        return false;
    }

    GuestPath combined;
    combined.kind = GuestPathKind::kDriveAbsolute;
    combined.drive_letter = current_directory.drive_letter;

    switch (path.kind)
    {
    case GuestPathKind::kDriveAbsolute:
        combined.drive_letter = path.drive_letter;
        combined.components = path.components;
        break;

    case GuestPathKind::kRootRelative:
        combined.components = path.components;
        break;

    case GuestPathKind::kDriveRelative:
        // Only one guest drive is mounted, so a drive-relative path naming a
        // different drive has no base directory to resolve against.
        if (path.drive_letter != current_directory.drive_letter)
        {
            return false;
        }
        combined.components = current_directory.components;
        combined.components.insert(combined.components.end(),
                                   path.components.begin(),
                                   path.components.end());
        break;

    case GuestPathKind::kRelative:
        combined.components = current_directory.components;
        combined.components.insert(combined.components.end(),
                                   path.components.begin(),
                                   path.components.end());
        break;

    case GuestPathKind::kUnc:
        return false;
    }

    if (!NormalizeGuestPath(&combined))
    {
        return false;
    }

    *out = std::move(combined);
    return true;
}

std::string GuestPathToString(const GuestPath& path)
{
    std::string text;
    if (path.drive_letter != '\0')
    {
        text.push_back(path.drive_letter);
        text.push_back(':');
    }
    if (path.kind == GuestPathKind::kDriveAbsolute ||
        path.kind == GuestPathKind::kRootRelative)
    {
        text.push_back('\\');
    }
    for (std::size_t index = 0; index < path.components.size(); ++index)
    {
        if (index != 0)
        {
            text.push_back('\\');
        }
        text.append(path.components[index]);
    }
    return text;
}

std::string GuestPathToRelativeString(const GuestPath& path)
{
    std::string text;
    for (std::size_t index = 0; index < path.components.size(); ++index)
    {
        if (index != 0)
        {
            text.push_back('/');
        }
        text.append(path.components[index]);
    }
    return text;
}

}  // namespace re2dj::storage
