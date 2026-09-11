#include "re2dj/hle/hardlock/seed_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace re2dj::hle::hardlock
{
namespace
{

std::string Trim(const std::string& value)
{
    const auto first = std::find_if_not(
        value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last)
    {
        return {};
    }
    return {first, last};
}

bool ParseU16(const std::string& text, std::uint16_t* value)
{
    if (value == nullptr)
    {
        return false;
    }
    const std::string token = Trim(text);
    if (token.empty())
    {
        return false;
    }
    std::size_t position = 0;
    int base = 10;
    if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
    {
        base = 16;
        position = 2;
    }
    try
    {
        const unsigned long parsed = std::stoul(token, &position, base);
        if (position != token.size() || parsed > 0xffff)
        {
            return false;
        }
        *value = static_cast<std::uint16_t>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

int HexValue(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool ParseBytes8(const std::string& text, std::array<std::uint8_t, 8>* bytes)
{
    if (bytes == nullptr) return false;
    const std::string token = Trim(text);
    if (token.size() != 16) return false;
    for (std::size_t i = 0; i < 8; ++i)
    {
        const int high = HexValue(token[i * 2]);
        const int low = HexValue(token[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        (*bytes)[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

}  // namespace

bool ReadHardlockSeedIni(const std::filesystem::path& path,
                         HardlockSeeds* seeds,
                         std::optional<std::uint16_t>* tail_word,
                         std::string* error)
{
    std::ifstream file(path);
    if (!file)
    {
        if (error != nullptr) *error = "unable to open seed ini: " + path.string();
        return false;
    }

    HardlockSeeds parsed{};
    bool has_module = false;
    bool has_s1 = false;
    bool has_s2 = false;
    bool has_s3 = false;
    std::optional<std::uint16_t> parsed_tail;

    std::string line;
    while (std::getline(file, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line[0] == '[' && line.back() == ']') continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = Trim(line.substr(0, eq));
        const std::string val = Trim(line.substr(eq + 1));

        if (key == "module_address")
        {
            if (ParseU16(val, &parsed.module_address)) has_module = true;
        }
        else if (key == "seed1")
        {
            if (ParseU16(val, &parsed.seed1)) has_s1 = true;
        }
        else if (key == "seed2")
        {
            if (ParseU16(val, &parsed.seed2)) has_s2 = true;
        }
        else if (key == "seed3")
        {
            if (ParseU16(val, &parsed.seed3)) has_s3 = true;
        }
        else if (key == "tail_word")
        {
            std::uint16_t tail = 0;
            if (ParseU16(val, &tail)) parsed_tail = tail;
        }
    }

    if (!has_module || !has_s1 || !has_s2 || !has_s3)
    {
        if (error != nullptr)
        {
            *error = "seed ini missing required keys (module_address, seed1, seed2, seed3)";
        }
        return false;
    }

    if (seeds != nullptr) *seeds = parsed;
    if (tail_word != nullptr) *tail_word = parsed_tail;
    return true;
}

bool WriteHardlockSeedIni(const std::filesystem::path& path,
                          const HardlockSeeds& seeds,
                          std::optional<std::uint16_t> tail_word,
                          std::string* error)
{
    std::ofstream file(path);
    if (!file)
    {
        if (error != nullptr) *error = "unable to create seed ini: " + path.string();
        return false;
    }

    file << "[hardlock]\n"
         << "module_address = 0x" << std::hex << std::setfill('0') << std::setw(4)
         << seeds.module_address << "\n"
         << "seed1 = 0x" << std::setw(4) << seeds.seed1 << "\n"
         << "seed2 = 0x" << std::setw(4) << seeds.seed2 << "\n"
         << "seed3 = 0x" << std::setw(4) << seeds.seed3 << "\n";
    if (tail_word.has_value())
    {
        file << "tail_word = 0x" << std::setw(4) << *tail_word << "\n";
    }
    return true;
}

bool ConfirmSeedsFromAnalysisArtifact(const std::filesystem::path& artifact_path,
                                      HardlockSeeds* confirmed_seeds,
                                      std::string* error)
{
    std::ifstream file(artifact_path);
    if (!file)
    {
        if (error != nullptr) *error = "unable to open artifact: " + artifact_path.string();
        return false;
    }

    std::uint16_t module_address = 0;
    std::array<std::uint8_t, 8> id_ref = {};
    std::array<std::uint8_t, 8> id_verify = {};
    std::vector<HardlockSeeds> candidates;

    auto extract_str_value = [](const std::string& line, const std::string& key) -> std::optional<std::string>
    {
        auto pos = line.find("\"" + key + "\"");
        if (pos == std::string::npos) return std::nullopt;
        auto colon = line.find(':', pos);
        if (colon == std::string::npos) return std::nullopt;
        auto q1 = line.find('\"', colon);
        if (q1 == std::string::npos) return std::nullopt;
        auto q2 = line.find('\"', q1 + 1);
        if (q2 == std::string::npos) return std::nullopt;
        return line.substr(q1 + 1, q2 - q1 - 1);
    };

    bool in_candidate_seeds = false;
    bool in_candidate_obj = false;
    std::uint16_t cur_s1 = 0;
    std::uint16_t cur_s2 = 0;
    std::uint16_t cur_s3 = 0;

    std::string raw_line;
    while (std::getline(file, raw_line))
    {
        std::string line = Trim(raw_line);
        if (line.empty()) continue;

        if (!in_candidate_seeds)
        {
            if (auto mod = extract_str_value(line, "module_address"))
            {
                ParseU16(*mod, &module_address);
            }
            else if (auto ref = extract_str_value(line, "id_ref"))
            {
                ParseBytes8(*ref, &id_ref);
            }
            else if (auto ver = extract_str_value(line, "id_verify"))
            {
                ParseBytes8(*ver, &id_verify);
            }
            else if (line.find("\"candidate_seeds\"") != std::string::npos)
            {
                in_candidate_seeds = true;
            }
        }
        else
        {
            if (!in_candidate_obj)
            {
                if (line.find('{') != std::string::npos)
                {
                    in_candidate_obj = true;
                    cur_s1 = cur_s2 = cur_s3 = 0;
                }
                else if (line.find(']') != std::string::npos)
                {
                    in_candidate_seeds = false;
                }
            }
            else
            {
                if (auto s1 = extract_str_value(line, "seed1")) ParseU16(*s1, &cur_s1);
                if (auto s2 = extract_str_value(line, "seed2")) ParseU16(*s2, &cur_s2);
                if (auto s3 = extract_str_value(line, "seed3")) ParseU16(*s3, &cur_s3);
                if (line.find('}') != std::string::npos)
                {
                    in_candidate_obj = false;
                    candidates.push_back(HardlockSeeds{module_address, cur_s1, cur_s2, cur_s3});
                }
            }
        }
    }

    if (candidates.empty())
    {
        if (error != nullptr) *error = "artifact contains no candidate seeds";
        return false;
    }

    // Verify candidate seeds against the ID_Ref -> ID_Verify round-trip invariant
    for (const auto& cand : candidates)
    {
        HardlockEngine engine(cand);
        const auto calc_verify = engine.CryptBlock(id_ref);
        if (calc_verify == id_verify)
        {
            if (confirmed_seeds != nullptr) *confirmed_seeds = cand;
            return true;
        }
    }

    if (error != nullptr) *error = "no candidate seed matched the verification check";
    return false;
}

}  // namespace re2dj::hle::hardlock
