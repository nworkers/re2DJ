#include "re2dj/storage/mame_chd.h"

#include <filesystem>
#include <string>

#include "test_support.h"

void RunMameChdTests(re2dj::test::Context& context)
{
    std::unique_ptr<re2dj::storage::MameChdImage> image;
    std::string error;
    RE2DJ_CHECK(context,
                !re2dj::storage::MameChdImage::Open({}, &image, &error));
    RE2DJ_CHECK(context,
                !re2dj::storage::MameChdImage::Open(
                    std::filesystem::path("does-not-exist.chd"), &image, &error));
    RE2DJ_CHECK_EQ(context,
                   re2dj::storage::MameChdCodecName(
                       re2dj::storage::MameChdCodec::kLzma),
                   std::string("lzma"));
    RE2DJ_CHECK_EQ(context,
                   re2dj::storage::MameChdCodecName(
                       re2dj::storage::MameChdCodec::kCdZstd),
                   std::string("cdzstd"));
}
