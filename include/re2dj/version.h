#ifndef RE2DJ_VERSION_H_
#define RE2DJ_VERSION_H_

#include <string_view>

namespace re2dj
{

// Value of the repository-root VERSION file, injected by CMake at build time.
std::string_view VersionString();

}  // namespace re2dj

#endif  // RE2DJ_VERSION_H_
