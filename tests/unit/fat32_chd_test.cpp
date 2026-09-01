#include "re2dj/storage/fat32_chd.h"

#include <filesystem>
#include <memory>
#include <string>

#include "test_support.h"

void RunFat32ChdTests(re2dj::test::Context& context)
{
    std::unique_ptr<re2dj::storage::Fat32Volume> volume;
    std::string error;
    RE2DJ_CHECK(context,
                !re2dj::storage::Fat32Volume::Open({}, &volume, &error));
    RE2DJ_CHECK(context,
                !re2dj::storage::Fat32Volume::Open(
                    std::filesystem::path("does-not-exist.chd"), &volume, &error));
}
