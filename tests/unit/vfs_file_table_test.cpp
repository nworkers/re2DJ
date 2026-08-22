#include "re2dj/hdd/hdd_root.h"
#include "re2dj/storage/vfs_file_table.h"

#include <array>
#include <filesystem>
#include <string>

#include "temporary_tree.h"
#include "test_support.h"

namespace fs = std::filesystem;

void RunVfsFileTableTests(re2dj::test::Context& context)
{
    const re2dj::test::TemporaryTree tree;
    tree.WriteText("EZ2DJ/DATA/ORIGINAL.DAT", "original");
    tree.WriteText("support/CONFIG.INI", "support");

    re2dj::hdd::HddRoot hdd;
    std::string error;
    RE2DJ_CHECK(context, re2dj::hdd::HddRoot::Open(tree.root(), &hdd, &error));

    re2dj::storage::GuestPath current_directory;
    RE2DJ_CHECK(context, re2dj::storage::ParseGuestPath("D:\\ez2dj", &current_directory));
    const re2dj::storage::VfsRoots roots{
        &hdd, tree.root() / "overlay", tree.root() / "support", current_directory};
    re2dj::storage::VfsFileTable files(roots);

    // The overlay replaces an original read without changing the original file.
    tree.WriteText("overlay/DATA/ORIGINAL.DAT", "overlay");
    std::uint32_t handle = files.Open("D:\\ez2dj\\DATA\\original.dat",
                                      re2dj::storage::VfsOpenMode::kRead,
                                      &error);
    RE2DJ_CHECK(context, handle != 0);
    std::array<char, 16> contents = {};
    std::size_t transferred = 0;
    RE2DJ_CHECK(context, files.Read(handle, contents.data(), contents.size(), &transferred, &error));
    RE2DJ_CHECK_EQ(context, std::string(contents.data(), transferred), std::string("overlay"));
    RE2DJ_CHECK(context, files.Close(handle, &error));

    // Writes only create files below the overlay root.
    handle = files.Open("SAVE\\RESULT.DAT", re2dj::storage::VfsOpenMode::kCreate, &error);
    RE2DJ_CHECK(context, handle != 0);
    const std::string written = "result";
    RE2DJ_CHECK(context,
                files.Write(handle, written.data(), written.size(), &transferred, &error));
    RE2DJ_CHECK_EQ(context, transferred, written.size());
    std::uint64_t size = 0;
    RE2DJ_CHECK(context, files.Size(handle, &size, &error));
    RE2DJ_CHECK_EQ(context, size, static_cast<std::uint64_t>(written.size()));
    std::uint64_t position = 0;
    RE2DJ_CHECK(context, files.Seek(handle, -3, 2, &position, &error));
    RE2DJ_CHECK_EQ(context, position, std::uint64_t{3});
    std::array<char, 4> tail = {};
    RE2DJ_CHECK(context, files.Read(handle, tail.data(), 3, &transferred, &error));
    RE2DJ_CHECK_EQ(context, std::string(tail.data(), transferred), std::string("ult"));
    RE2DJ_CHECK(context, files.Close(handle, &error));
    RE2DJ_CHECK(context, fs::is_regular_file(tree.root() / "overlay/SAVE/RESULT.DAT"));
    RE2DJ_CHECK(context, !fs::exists(tree.root() / "EZ2DJ/SAVE/RESULT.DAT"));
}
