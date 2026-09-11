#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <string>
#include <vector>

#include "re2dj/hle/hardlock/api_descriptor.h"
#include "re2dj/hle/hardlock/device.h"
#include "re2dj/hle/hardlock/engine.h"
#include "re2dj/hle/hardlock/protocol.h"
#include "re2dj/hle/hardlock/seed_config.h"

namespace
{

void TestEngineBasics(re2dj::test::Context& context)
{
    const re2dj::hle::hardlock::HardlockSeeds seeds{0x044c, 0x1234, 0x5678, 0x9abc};
    re2dj::hle::hardlock::HardlockEngine engine(seeds);

    const std::array<std::uint8_t, 8> challenge{1, 2, 3, 4, 5, 6, 7, 8};
    const auto resp1 = engine.CryptBlock(challenge);
    const auto resp2 = engine.CryptBlock(challenge);
    RE2DJ_CHECK(context, resp1 == resp2);
    RE2DJ_CHECK(context, resp1 != challenge);

    std::array<std::uint8_t, 8> block_copy = challenge;
    engine.EncryptBlock(block_copy);
    RE2DJ_CHECK(context, block_copy == resp1);

    const auto payload1 = engine.CodePayload(challenge);
    const auto payload2 = engine.CodePayload(challenge);
    RE2DJ_CHECK(context, payload1 == payload2);
    RE2DJ_CHECK_EQ(context, payload1.size(), std::size_t{56});

    const auto calc1 = engine.Calc(0x1122, 0x3344);
    const auto calc2 = engine.Calc(0x1122, 0x3344);
    RE2DJ_CHECK_EQ(context, calc1, calc2);
}

void TestDynamicDeviceTransform(re2dj::test::Context& context)
{
    const re2dj::hle::hardlock::HardlockSeeds seeds{0x044c, 0x1234, 0x5678, 0x9abc};
    re2dj::hle::hardlock::HardlockDeviceOptions options;
    options.seeds = seeds;
    options.descriptor_tail_word = static_cast<std::uint16_t>(0x0001);

    re2dj::hle::hardlock::HardlockDevice device(options);

    // 1. Initialize
    auto init_res = device.Complete(re2dj::hle::hardlock::kHardlockIoctlInitialize, {}, {});
    RE2DJ_CHECK(context, init_res.outcome == re2dj::hle::hardlock::HardlockOutcome::kCompleted);

    // 2. Handshake
    std::array<std::uint8_t, 6> hs_in{1, 2, 3, 4, 5, 6};
    std::array<std::uint8_t, 6> hs_out{};
    auto hs_res = device.Complete(re2dj::hle::hardlock::kHardlockIoctlHandshake, hs_in, hs_out);
    RE2DJ_CHECK(context, hs_res.outcome == re2dj::hle::hardlock::HardlockOutcome::kCompleted);

    // 3. Dynamic 8-byte transform (function 0x000e, block_count 1 -> total size 256 + 8 = 264)
    std::vector<std::uint8_t> req(re2dj::hle::hardlock::kHardlockApiDescriptorSize + 8, 0);
    // write function 0x000e
    req[0x18] = 0x0e;
    req[0x19] = 0x00;
    // write block count 1 at offset 0x16
    req[0x16] = 0x01;
    req[0x17] = 0x00;
    // challenge in payload block 0
    std::array<std::uint8_t, 8> chal{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    std::copy(chal.begin(), chal.end(), req.begin() + re2dj::hle::hardlock::kHardlockApiDescriptorSize);

    std::vector<std::uint8_t> resp = req;
    auto trans_res = device.Complete(re2dj::hle::hardlock::kHardlockIoctlTransform, req, resp);
    RE2DJ_CHECK(context, trans_res.outcome == re2dj::hle::hardlock::HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, trans_res.transform_dynamically_computed);
    RE2DJ_CHECK_EQ(context, trans_res.transform_blocks_mapped, std::size_t{1});

    // Verify response matches engine direct calculation
    re2dj::hle::hardlock::HardlockEngine engine(seeds);
    const auto expected = engine.CryptBlock(chal);
    std::array<std::uint8_t, 8> actual{};
    std::copy_n(resp.begin() + re2dj::hle::hardlock::kHardlockApiDescriptorSize, 8, actual.begin());
    RE2DJ_CHECK(context, actual == expected);
}

void TestDynamicDevicePayload(re2dj::test::Context& context)
{
    const re2dj::hle::hardlock::HardlockSeeds seeds{0x044c, 0x1234, 0x5678, 0x9abc};
    re2dj::hle::hardlock::HardlockDeviceOptions options;
    options.seeds = seeds;

    re2dj::hle::hardlock::HardlockDevice device(options);

    // 7 blocks = 56 bytes payload -> total size 256 + 56 = 312 bytes
    std::vector<std::uint8_t> req(re2dj::hle::hardlock::kHardlockApiDescriptorSize + 56, 0);
    req[0x18] = 0x09; // function 0x0009
    req[0x19] = 0x00;
    req[0x16] = 0x07; // block count 7 at offset 0x16
    req[0x17] = 0x00;

    std::array<std::uint8_t, 8> input_block{0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04};
    // block 5 sits at offset 40 in payload (offset 256 + 40 in req)
    std::copy(input_block.begin(), input_block.end(), req.begin() + re2dj::hle::hardlock::kHardlockApiDescriptorSize + 40);
    // Put test DWORDs into block 3 of payload (offset 24 and 28)
    const std::uint32_t init_b3_dw0 = 0x10203040;
    const std::uint32_t init_b3_dw1 = 0x50607080;
    std::memcpy(req.data() + re2dj::hle::hardlock::kHardlockApiDescriptorSize + 24, &init_b3_dw0, 4);
    std::memcpy(req.data() + re2dj::hle::hardlock::kHardlockApiDescriptorSize + 28, &init_b3_dw1, 4);

    std::vector<std::uint8_t> resp = req;
    auto trans_res = device.Complete(re2dj::hle::hardlock::kHardlockIoctlTransform, req, resp);
    RE2DJ_CHECK(context, trans_res.outcome == re2dj::hle::hardlock::HardlockOutcome::kCompleted);
    RE2DJ_CHECK(context, trans_res.transform_dynamically_computed);
    RE2DJ_CHECK(context, trans_res.transform_payload_mapped);

    re2dj::hle::hardlock::HardlockEngine engine(seeds);
    const auto expected_code = engine.CodePayload(input_block);
    // Block 0 should match bytes 0..7 of expected_code
    std::array<std::uint8_t, 8> b0{};
    std::copy_n(resp.begin() + re2dj::hle::hardlock::kHardlockApiDescriptorSize, 8, b0.begin());
    std::array<std::uint8_t, 8> exp_b0{};
    std::copy_n(expected_code.begin(), 8, exp_b0.begin());
    RE2DJ_CHECK(context, b0 == exp_b0);

    // Block 3 should be initial DWORDs + expected_code DWORDs
    std::uint32_t resp_b3_dw0 = 0, resp_b3_dw1 = 0;
    std::uint32_t exp_b3_dw0 = 0, exp_b3_dw1 = 0;
    std::memcpy(&resp_b3_dw0, resp.data() + re2dj::hle::hardlock::kHardlockApiDescriptorSize + 24, 4);
    std::memcpy(&resp_b3_dw1, resp.data() + re2dj::hle::hardlock::kHardlockApiDescriptorSize + 28, 4);
    std::memcpy(&exp_b3_dw0, expected_code.data() + 24, 4);
    std::memcpy(&exp_b3_dw1, expected_code.data() + 28, 4);
    RE2DJ_CHECK_EQ(context, resp_b3_dw0, init_b3_dw0 + exp_b3_dw0);
    RE2DJ_CHECK_EQ(context, resp_b3_dw1, init_b3_dw1 + exp_b3_dw1);
}

void TestSeedConfigAndArtifact(re2dj::test::Context& context)
{
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto ini_path = temp_dir / "test_hardlock.ini";
    const auto art_path = temp_dir / "test_hardlock_analysis.json";

    const re2dj::hle::hardlock::HardlockSeeds seeds{0x044c, 0x1111, 0x2222, 0x3333};
    std::string err;

    // INI round-trip
    RE2DJ_CHECK(context, re2dj::hle::hardlock::WriteHardlockSeedIni(ini_path, seeds, static_cast<std::uint16_t>(0x0042), &err));
    re2dj::hle::hardlock::HardlockSeeds read_seeds{};
    std::optional<std::uint16_t> read_tail;
    RE2DJ_CHECK(context, re2dj::hle::hardlock::ReadHardlockSeedIni(ini_path, &read_seeds, &read_tail, &err));
    RE2DJ_CHECK(context, read_seeds == seeds);
    RE2DJ_CHECK(context, read_tail.has_value() && *read_tail == static_cast<std::uint16_t>(0x0042));
    std::filesystem::remove(ini_path);

    // Artifact confirmation test
    re2dj::hle::hardlock::HardlockEngine engine(seeds);
    const std::array<std::uint8_t, 8> id_ref{1, 2, 3, 4, 5, 6, 7, 8};
    const auto id_verify = engine.CryptBlock(id_ref);

    {
        std::ofstream art(art_path);
        art << "{\n"
            << "  \"executable\": \"TEST.EXE\",\n"
            << "  \"module_address\": \"0x044c\",\n"
            << "  \"id_ref\": \"0102030405060708\",\n"
            << "  \"id_verify\": \"";
        for (auto b : id_verify)
        {
            art << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        art << "\",\n"
            << "  \"challenges\": [\"0102030405060708\"],\n"
            << "  \"candidate_seeds\": [\n"
            << "    {\"seed1\": \"0x9999\", \"seed2\": \"0x9999\", \"seed3\": \"0x9999\"},\n"
            << "    {\"seed1\": \"0x1111\", \"seed2\": \"0x2222\", \"seed3\": \"0x3333\"}\n"
            << "  ]\n"
            << "}\n";
    }

    re2dj::hle::hardlock::HardlockSeeds confirmed{};
    RE2DJ_CHECK(context, re2dj::hle::hardlock::ConfirmSeedsFromAnalysisArtifact(art_path, &confirmed, &err));
    RE2DJ_CHECK(context, confirmed == seeds);
    std::filesystem::remove(art_path);
}

}  // namespace

void RunHardlockEngineTests(re2dj::test::Context& context)
{
    TestEngineBasics(context);
    TestDynamicDeviceTransform(context);
    TestDynamicDevicePayload(context);
    TestSeedConfigAndArtifact(context);
}
