#include <cstdint>
#include <string>

#include "re2dj/config/hardlock_secret_config.h"

#include "temporary_tree.h"
#include "test_support.h"

namespace
{

std::string MakeConfig(bool include_last_seed = true)
{
    std::string text =
        "[another-profile]\n"
        "ignored=value\n"
        "[test-profile]\n"
        "modad=" + std::to_string(0x1200u + 0x34u) + "\n" +
        "seed1=0x" + std::to_string(1000u + 234u) + "\n" +
        "seed2=" + std::to_string(0x2345u) + "\n";
    if (include_last_seed)
    {
        text += "seed3=" + std::to_string(0x3456u) + "\n";
    }
    return text;
}

}  // namespace

void RunHardlockSecretConfigTests(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    tree.WriteText("hardlock.ini", MakeConfig());
    re2dj::config::HardlockSecretMaterial material;
    std::string error;
    RE2DJ_CHECK(context,
                 re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "hardlock.ini", "test-profile", &material, &error));
    RE2DJ_CHECK_EQ(context, material.module_address, std::uint16_t{0x1234});
    RE2DJ_CHECK_EQ(context, material.seeds[0], std::uint16_t{0x1234});
    RE2DJ_CHECK_EQ(context, material.seeds[1], std::uint16_t{0x2345});
    RE2DJ_CHECK_EQ(context, material.seeds[2], std::uint16_t{0x3456});

    tree.WriteText("missing.ini", MakeConfig(false));
    RE2DJ_CHECK(context,
                 !re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "missing.ini", "test-profile", &material, &error));
    RE2DJ_CHECK(context, error.find("missing") != std::string::npos);

    tree.WriteText("duplicate.ini", MakeConfig() + "seed3=1\n");
    RE2DJ_CHECK(context,
                 !re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "duplicate.ini", "test-profile", &material, &error));
    RE2DJ_CHECK(context, error.find("duplicate") != std::string::npos);

    tree.WriteText("overflow.ini",
                   "[test-profile]\nmodad=65536\nseed1=1\nseed2=2\nseed3=3\n");
    RE2DJ_CHECK(context,
                 !re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "overflow.ini", "test-profile", &material, &error));
    RE2DJ_CHECK(context, error.find("16-bit") != std::string::npos);

    tree.WriteText("unknown.ini",
                   "[test-profile]\nmodad=1\nseed1=2\nseed2=3\nseed3=4\nextra=5\n");
    RE2DJ_CHECK(context,
                 !re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "unknown.ini", "test-profile", &material, &error));
    RE2DJ_CHECK(context, error.find("unknown") != std::string::npos);

    tree.MakeDirectory("checkout/.git");
    tree.WriteText("checkout/private/hardlock.ini", MakeConfig());
    RE2DJ_CHECK(context,
                 !re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "checkout/private/hardlock.ini",
                     "test-profile",
                     &material,
                     &error));
    RE2DJ_CHECK(context, error.find("under cfg") != std::string::npos);

    tree.WriteText("checkout/cfg/hardlock.ini", MakeConfig());
    RE2DJ_CHECK(context,
                 re2dj::config::LoadHardlockSecretConfig(
                     tree.root() / "checkout/cfg/hardlock.ini",
                     "test-profile",
                     &material,
                     &error));
}
