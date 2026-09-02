#include "re2dj/config/hardlock_secret_config.h"

#include <filesystem>
#include <string>

#include "temporary_tree.h"
#include "test_support.h"

void RunHardlockMaterialConfigTests(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    re2dj::config::HardlockSecretMaterial material;
    std::string error;
    bool found = true;

    // A missing file is absence, not failure: a profile default consults these
    // paths on every run and most runs have nothing there.
    RE2DJ_CHECK(context,
                re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "absent.ini", "test-profile", &material, &found, &error));
    RE2DJ_CHECK(context, !found);
    RE2DJ_CHECK(context, error.empty());

    // Both keys, with comments and other sections around them.
    tree.WriteText("hardlock.ini",
                   "# comment\n"
                   "[another-profile]\n"
                   "response450=ffffffffffff\n"
                   "\n"
                   "[test-profile]\n"
                   "response450=0100fafa0010\n"
                   "tail44c=0001\n");
    found = false;
    RE2DJ_CHECK(context,
                re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "hardlock.ini", "test-profile", &material, &found, &error));
    RE2DJ_CHECK(context, found);
    // The text is kept verbatim so one parser downstream validates the width.
    RE2DJ_CHECK_EQ(context, material.handshake_response_hex, std::string("0100fafa0010"));
    RE2DJ_CHECK_EQ(context, material.descriptor_tail_hex, std::string("0001"));

    // A section that is not there reads as absence, and the other section's
    // value never leaks into the result.
    re2dj::config::HardlockSecretMaterial other;
    found = true;
    RE2DJ_CHECK(context,
                re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "hardlock.ini", "missing-profile", &other, &found, &error));
    RE2DJ_CHECK(context, !found);
    RE2DJ_CHECK(context, other.handshake_response_hex.empty());

    // One key alone is valid; the pair is not required by this loader.
    tree.WriteText("partial.ini", "[test-profile]\ntail44c=0001\n");
    re2dj::config::HardlockSecretMaterial partial;
    found = false;
    RE2DJ_CHECK(context,
                re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "partial.ini", "test-profile", &partial, &found, &error));
    RE2DJ_CHECK(context, found);
    RE2DJ_CHECK(context, partial.handshake_response_hex.empty());
    RE2DJ_CHECK_EQ(context, partial.descriptor_tail_hex, std::string("0001"));

    // Keys re2DJ does not use are rejected rather than ignored: a key nothing
    // reads would look configured while doing nothing.
    tree.WriteText("unknown.ini", "[test-profile]\nmodad=1\n");
    re2dj::config::HardlockSecretMaterial unknown;
    RE2DJ_CHECK(context,
                !re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "unknown.ini", "test-profile", &unknown, &found, &error));
    RE2DJ_CHECK(context, error.find("unknown") != std::string::npos);

    tree.WriteText("duplicate.ini",
                   "[test-profile]\nresponse450=0100fafa0010\nresponse450=0100fafa0010\n");
    RE2DJ_CHECK(context,
                !re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "duplicate.ini", "test-profile", &unknown, &found, &error));
    RE2DJ_CHECK(context, error.find("duplicate") != std::string::npos);

    tree.WriteText("empty-value.ini", "[test-profile]\ntail44c=\n");
    RE2DJ_CHECK(context,
                !re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "empty-value.ini", "test-profile", &unknown, &found, &error));

    tree.WriteText("malformed.ini", "[test-profile]\nresponse450\n");
    RE2DJ_CHECK(context,
                !re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "malformed.ini", "test-profile", &unknown, &found, &error));
    RE2DJ_CHECK(context, error.find("malformed") != std::string::npos);

    // Inside a Git work tree the material may only sit under cfg/, so a
    // completed file cannot be committed by accident.
    tree.MakeDirectory("checkout/.git");
    tree.WriteText("checkout/private/hardlock.ini", "[test-profile]\ntail44c=0001\n");
    RE2DJ_CHECK(context,
                !re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "checkout/private/hardlock.ini",
                    "test-profile",
                    &unknown,
                    &found,
                    &error));
    RE2DJ_CHECK(context, error.find("under cfg") != std::string::npos);

    tree.WriteText("checkout/cfg/hardlock.ini", "[test-profile]\ntail44c=0001\n");
    found = false;
    RE2DJ_CHECK(context,
                re2dj::config::LoadHardlockProfileMaterial(
                    tree.root() / "checkout/cfg/hardlock.ini",
                    "test-profile",
                    &unknown,
                    &found,
                    &error));
    RE2DJ_CHECK(context, found);

    // The map convention path is per profile and sits beside the ini.
    const std::filesystem::path map_path =
        re2dj::config::DefaultHardlockTransformMapPath("ez2dj4th");
    RE2DJ_CHECK_EQ(context, map_path.filename().string(), std::string("hardlock-ez2dj4th.map"));
    RE2DJ_CHECK_EQ(context,
                   map_path.parent_path(),
                   re2dj::config::DefaultHardlockSecretConfigPath().parent_path());
    RE2DJ_CHECK(context, re2dj::config::DefaultHardlockTransformMapPath("").empty());
}
