#ifndef RE2DJ_HDD_CHD_IMAGE_LOCATOR_H_
#define RE2DJ_HDD_CHD_IMAGE_LOCATOR_H_

#include <cstdint>
#include <filesystem>
#include <string>

namespace re2dj::hdd
{

// How a CHD lookup ended, for callers that report the cause rather than just
// the message (the launcher's availability column).
enum class ChdImageLookup : std::uint8_t
{
    kFound = 0,
    // `input` is a regular file without a `.chd` extension.
    kNotChd,
    // `input` is empty or names nothing.
    kMissing,
    // The directory holds no `.chd` file.
    kNone,
    // The directory holds more than one.
    kSeveral,
};

// The CHD image a profile runs from: `input` itself when it is a `.chd` file,
// otherwise the one `.chd` file directly inside the directory `input`
// (extension compared without case). False, with `error` set, when `input` is
// some other file, is missing, or holds no `.chd` or more than one. Shared by
// the command-line profile shortcut and the launcher (#12), so both judge a
// profile's image the same way.
ChdImageLookup LocateChdImage(const std::filesystem::path& input,
                              std::filesystem::path* image,
                              std::string* error);

// LocateChdImage reduced to found or not.
bool FindChdImage(const std::filesystem::path& input,
                  std::filesystem::path* image,
                  std::string* error);

}  // namespace re2dj::hdd

#endif  // RE2DJ_HDD_CHD_IMAGE_LOCATOR_H_
