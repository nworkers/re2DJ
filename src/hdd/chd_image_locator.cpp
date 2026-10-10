#include "re2dj/hdd/chd_image_locator.h"

#include <algorithm>
#include <system_error>
#include <vector>

#include "re2dj/storage/guest_path.h"

namespace re2dj::hdd
{

ChdImageLookup LocateChdImage(const std::filesystem::path& input,
                              std::filesystem::path* image,
                              std::string* error)
{
    if (image == nullptr || error == nullptr || input.empty())
    {
        if (error != nullptr)
        {
            *error = "CHD path is empty";
        }
        return ChdImageLookup::kMissing;
    }
    std::error_code code;
    if (std::filesystem::is_regular_file(input, code))
    {
        if (storage::EqualsIgnoreAsciiCase(input.extension().string(), ".chd"))
        {
            *image = std::filesystem::weakly_canonical(input, code);
            if (code)
            {
                *image = input;
            }
            return ChdImageLookup::kFound;
        }
        *error = "CHD input is a regular file but does not have a .chd extension";
        return ChdImageLookup::kNotChd;
    }
    if (code || !std::filesystem::is_directory(input, code))
    {
        *error = "CHD input directory does not exist: " + input.string();
        return ChdImageLookup::kMissing;
    }
    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator iterator(input, code), end;
         !code && iterator != end;
         iterator.increment(code))
    {
        if (!iterator->is_regular_file(code) || code)
        {
            continue;
        }
        const std::string extension = iterator->path().extension().string();
        if (storage::EqualsIgnoreAsciiCase(extension, ".chd"))
        {
            candidates.push_back(iterator->path());
        }
    }
    if (code || candidates.empty())
    {
        *error = "no .chd image was found under " + input.string();
        return ChdImageLookup::kNone;
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.size() > 1)
    {
        *error = "more than one .chd image was found under " + input.string();
        return ChdImageLookup::kSeveral;
    }
    *image = std::filesystem::weakly_canonical(candidates.front(), code);
    if (code)
    {
        *image = candidates.front();
    }
    return ChdImageLookup::kFound;
}

bool FindChdImage(const std::filesystem::path& input,
                  std::filesystem::path* image,
                  std::string* error)
{
    return LocateChdImage(input, image, error) == ChdImageLookup::kFound;
}

}  // namespace re2dj::hdd
