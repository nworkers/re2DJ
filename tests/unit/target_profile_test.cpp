#include "re2dj/target/target_profile.h"

#include <string>

#include "re2dj/exe/pe_image.h"
#include "re2dj/hdd/hdd_scan.h"
#include "synthetic_pe32.h"
#include "test_support.h"

namespace
{

re2dj::hdd::ExecutableEntry MakeEntry(const std::string& relative_path,
                                      std::uint16_t machine,
                                      std::uint16_t subsystem)
{
    std::vector<std::uint8_t> image = re2dj::test::MakeSyntheticPe32Image();
    re2dj::test::PutU16(image, re2dj::test::kSyntheticFileHeaderOffset + 0, machine);
    re2dj::test::PutU16(image, re2dj::test::kSyntheticOptionalOffset + 68, subsystem);

    re2dj::hdd::ExecutableEntry entry;
    entry.relative_path = relative_path;
    entry.file_size = image.size();
    entry.pe_readable =
        re2dj::exe::ReadPeImageInfo(image.data(), image.size(), &entry.pe_info, nullptr);
    return entry;
}

}  // namespace

void RunTargetProfileTests(re2dj::test::Context& context)
{
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("EZ2DJ/Ez2dj.exe"), std::string("ez2dj"));
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("EZ2DJ.EXE"), std::string("ez2dj"));
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("a/b/EZ2DJ 4th.exe"),
                   std::string("ez2dj_4th"));
    RE2DJ_CHECK_EQ(context, re2dj::target::MakeProfileId("dir/.exe"), std::string("_exe"));

    // The built-in table is deliberately empty until a real dump confirms the
    // per-version paths, so detection has to carry the load on its own.
    RE2DJ_CHECK(context, re2dj::target::GetBuiltInTargetProfiles().empty());

    re2dj::hdd::HddScanResult scan;
    scan.executables.push_back(
        MakeEntry("EZ2DJ/Ez2dj.exe", re2dj::exe::kMachineI386, re2dj::exe::kSubsystemWindowsGui));
    scan.executables.push_back(
        MakeEntry("tools/Helper.exe", re2dj::exe::kMachineAmd64, re2dj::exe::kSubsystemWindowsCui));
    scan.executables.push_back(
        MakeEntry("BACKUP/Ez2dj.exe", re2dj::exe::kMachineI386, re2dj::exe::kSubsystemWindowsGui));

    const std::vector<re2dj::target::TargetProfile> profiles =
        re2dj::target::BuildTargetProfiles(scan);

    // The amd64 helper is not a launch candidate, so two profiles remain.
    RE2DJ_CHECK_EQ(context, profiles.size(), std::size_t{2});
    if (profiles.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, profiles[0].id, std::string("ez2dj"));
        RE2DJ_CHECK_EQ(context, profiles[0].executable_relative_path,
                       std::string("EZ2DJ/Ez2dj.exe"));
        RE2DJ_CHECK_EQ(context, profiles[0].working_directory_relative_path,
                       std::string("EZ2DJ"));
        RE2DJ_CHECK(context, profiles[0].detected);

        // Two copies of the same executable name keep distinct ids instead of
        // one shadowing the other, which a dump with a backup folder needs.
        RE2DJ_CHECK_EQ(context, profiles[1].id, std::string("ez2dj_2"));
        RE2DJ_CHECK_EQ(context, profiles[1].executable_relative_path,
                       std::string("BACKUP/Ez2dj.exe"));
    }

    RE2DJ_CHECK(context, re2dj::target::FindTargetProfileById(profiles, "ez2dj") != nullptr);
    RE2DJ_CHECK(context, re2dj::target::FindTargetProfileById(profiles, "EZ2DJ") != nullptr);
    RE2DJ_CHECK(context, re2dj::target::FindTargetProfileById(profiles, "missing") == nullptr);

    // An executable at the root has no parent directory, so the guest working
    // directory is the HDD root itself.
    re2dj::hdd::HddScanResult root_scan;
    root_scan.executables.push_back(
        MakeEntry("Ez2dj.exe", re2dj::exe::kMachineI386, re2dj::exe::kSubsystemWindowsGui));
    const std::vector<re2dj::target::TargetProfile> root_profiles =
        re2dj::target::BuildTargetProfiles(root_scan);
    RE2DJ_CHECK_EQ(context, root_profiles.size(), std::size_t{1});
    if (!root_profiles.empty())
    {
        RE2DJ_CHECK(context, root_profiles[0].working_directory_relative_path.empty());
    }
}
