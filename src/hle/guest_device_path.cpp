#include "re2dj/hle/guest_device_path.h"

namespace re2dj::hle
{
namespace
{

char LowerAscii(char value)
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

}  // namespace

bool MatchesGuestDevicePrefix(std::string_view name, std::string_view prefix)
{
    if (prefix.empty() || name.size() < prefix.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index)
    {
        if (LowerAscii(name[index]) != LowerAscii(prefix[index]))
        {
            return false;
        }
    }
    return true;
}

}  // namespace re2dj::hle
