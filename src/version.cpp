#include "re2dj/version.h"

#ifndef RE2DJ_VERSION
#define RE2DJ_VERSION "0.0.0"
#endif

namespace re2dj
{

std::string_view VersionString()
{
    return RE2DJ_VERSION;
}

}  // namespace re2dj
