#include "re2dj/hdd/hdd_root.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_scan.h"
#include "synthetic_pe32.h"
#include "test_support.h"

namespace fs = std::filesystem;

namespace
{

// A throwaway stand-in for a user's HDD dump. Real dumps never enter the
// repository, so every fixture the suite needs is built at run time.
class TemporaryTree
{
public:
    TemporaryTree()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = fs::temp_directory_path() /
                ("re2dj_hdd_test_" + std::to_string(static_cast<long long>(stamp)));
        std::error_code code;
        fs::create_directories(root_, code);
    }

    ~TemporaryTree()
    {
        std::error_code code;
        fs::remove_all(root_, code);
    }

    TemporaryTree(const TemporaryTree&) = delete;
    TemporaryTree& operator=(const TemporaryTree&) = delete;

    const fs::path& root() const
    {
        return root_;
    }

    void WriteText(const std::string& relative, const std::string& text) const
    {
        const fs::path target = root_ / fs::path(relative);
        std::error_code code;
        fs::create_directories(target.parent_path(), code);
        std::ofstream stream(target, std::ios::binary);
        stream << text;
    }

    void WriteBytes(const std::string& relative, const std::vector<std::uint8_t>& bytes) const
    {
        const fs::path target = root_ / fs::path(relative);
        std::error_code code;
        fs::create_directories(target.parent_path(), code);
        std::ofstream stream(target, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

private:
    fs::path root_;
};

}  // namespace

void RunHddRootTests(re2dj::test::Context& context)
{
    const TemporaryTree tree;

    std::vector<std::uint8_t> game = re2dj::test::MakeSyntheticPe32Image();
    tree.WriteBytes("EZ2DJ/Ez2dj.exe", game);
    tree.WriteText("EZ2DJ/DATA/Song01.EZ", "song data");
    tree.WriteText("Readme.txt", "notes");

    // A 64-bit helper: present in plenty of dumps, never a launch candidate.
    std::vector<std::uint8_t> helper = re2dj::test::MakeSyntheticPe32Image();
    re2dj::test::PutU16(helper,
                        re2dj::test::kSyntheticFileHeaderOffset + 0,
                        re2dj::exe::kMachineAmd64);
    tree.WriteBytes("tools/Helper.exe", helper);

    re2dj::hdd::HddRoot root;
    std::string error;
    RE2DJ_CHECK(context, re2dj::hdd::HddRoot::Open(tree.root(), &root, &error));
    RE2DJ_CHECK(context, root.is_open());

    // A file path and a missing path are both refused, with a message.
    re2dj::hdd::HddRoot rejected;
    RE2DJ_CHECK(context,
                !re2dj::hdd::HddRoot::Open(tree.root() / "Readme.txt", &rejected, &error));
    RE2DJ_CHECK(context, !error.empty());
    RE2DJ_CHECK(context,
                !re2dj::hdd::HddRoot::Open(tree.root() / "no_such_directory", &rejected, &error));
    RE2DJ_CHECK(context, !re2dj::hdd::HddRoot::Open(fs::path(), &rejected, &error));

    fs::path resolved;
    RE2DJ_CHECK(context, root.Resolve("EZ2DJ/DATA/Song01.EZ", &resolved));

    // The point of the class: the guest may ask in any case, and Linux and Web
    // hosts would otherwise fail the open.
    RE2DJ_CHECK(context, root.Resolve("ez2dj/data/song01.ez", &resolved));
    RE2DJ_CHECK(context, root.Resolve("EZ2DJ/data/SONG01.ez", &resolved));
    std::error_code code;
    RE2DJ_CHECK(context, fs::is_regular_file(resolved, code));

    // The result carries the on-disk spelling rather than the requested one.
    // Returning the requested spelling would work on Windows and produce a
    // different path on Linux for the same dump.
    RE2DJ_CHECK_EQ(context, resolved.filename().string(), std::string("Song01.EZ"));
    RE2DJ_CHECK_EQ(context, resolved.parent_path().filename().string(), std::string("DATA"));

    // Backslashes arrive from guest code that never saw a POSIX path.
    RE2DJ_CHECK(context, root.Resolve("EZ2DJ\\DATA\\Song01.EZ", &resolved));

    RE2DJ_CHECK(context, !root.Resolve("EZ2DJ/DATA/MISSING.EZ", &resolved));
    RE2DJ_CHECK(context, !root.Resolve("NOPE/DATA/Song01.EZ", &resolved));

    RE2DJ_CHECK(context, root.ResolveFile("EZ2DJ/Ez2dj.exe", &resolved));
    RE2DJ_CHECK(context, !root.ResolveFile("EZ2DJ/DATA", &resolved));
    RE2DJ_CHECK(context, root.ResolveDirectory("EZ2DJ/DATA", &resolved));
    RE2DJ_CHECK(context, !root.ResolveDirectory("EZ2DJ/Ez2dj.exe", &resolved));

    // An empty path is the root itself; ".." never escapes it.
    RE2DJ_CHECK(context, root.Resolve("", &resolved));
    RE2DJ_CHECK_EQ(context, resolved, root.root());
    RE2DJ_CHECK(context, !root.Resolve("..", &resolved));
    RE2DJ_CHECK(context, !root.Resolve("EZ2DJ/../..", &resolved));

    // An unopened root resolves nothing rather than reading the process cwd.
    const re2dj::hdd::HddRoot unopened;
    RE2DJ_CHECK(context, !unopened.Resolve("EZ2DJ", &resolved));

    const re2dj::hdd::HddScanResult scan = re2dj::hdd::ScanHdd(root);
    RE2DJ_CHECK(context, !scan.truncated);
    RE2DJ_CHECK_EQ(context, scan.executables.size(), std::size_t{2});
    RE2DJ_CHECK_EQ(context, scan.directory_count, std::size_t{3});
    RE2DJ_CHECK_EQ(context, scan.file_count, std::size_t{4});

    if (scan.executables.size() == 2)
    {
        // The guest-format GUI executable must sort ahead of the amd64 helper.
        RE2DJ_CHECK_EQ(context,
                       scan.executables[0].relative_path,
                       std::string("EZ2DJ/Ez2dj.exe"));
        RE2DJ_CHECK(context, scan.executables[0].pe_readable);
        RE2DJ_CHECK(context, re2dj::exe::IsGuestExecutable(scan.executables[0].pe_info));
        RE2DJ_CHECK(context, !re2dj::exe::IsGuestExecutable(scan.executables[1].pe_info));
    }
}
