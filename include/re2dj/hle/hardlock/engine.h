#ifndef RE2DJ_HLE_HARDLOCK_ENGINE_H_
#define RE2DJ_HLE_HARDLOCK_ENGINE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace re2dj::hle::hardlock
{

struct HardlockSeeds
{
    std::uint16_t module_address = 0;
    std::uint16_t seed1 = 0;
    std::uint16_t seed2 = 0;
    std::uint16_t seed3 = 0;

    auto operator<=>(const HardlockSeeds&) const = default;
};

// Independent clean-room implementation of the Hardlock cryptographic engine.
// All algorithms are implemented using standard C++ without any third-party or OS dependencies.
class HardlockEngine
{
public:
    HardlockEngine() = default;
    explicit HardlockEngine(const HardlockSeeds& seeds);

    void Init(const HardlockSeeds& seeds);

    // Encrypts an 8-byte block in-place (HL_CRYPT algorithm for functions 0x000e / 0x0011).
    void EncryptBlock(std::span<std::uint8_t, 8> block) const;
    std::array<std::uint8_t, 8> CryptBlock(std::span<const std::uint8_t, 8> challenge) const;

    // Transforms payload data (HL_CODE algorithm for function 0x0009).
    // Takes an 8-byte input and generates 56 bytes of payload response.
    void CodePayloadInPlace(std::span<const std::uint8_t, 8> input,
                            std::span<std::uint8_t, 56> output) const;
    std::array<std::uint8_t, 56> CodePayload(std::span<const std::uint8_t, 8> input) const;

    // Calculates verification byte for given parameters (HL_CALC algorithm).
    std::uint8_t Calc(std::uint16_t p1, std::uint16_t p2) const;

    const HardlockSeeds& seeds() const { return seeds_; }

private:
    struct InternalState
    {
        std::uint8_t var1 = 0x0f;
        std::uint8_t var2 = 0x00;
        std::uint16_t var4 = 0x0000;
    };

    void SetDongleData(std::uint8_t data, InternalState& state) const;
    std::uint8_t GetBitFromDongleData(const InternalState& state) const;
    std::uint16_t Transform0Hw(std::uint16_t w0, std::uint16_t ret_w, InternalState& state) const;
    std::uint16_t Transform0(std::uint16_t w3, std::uint16_t w4) const;
    std::uint8_t CipherFunction(std::uint32_t& r, std::uint32_t& tmp_r, InternalState& state) const;

    HardlockSeeds seeds_ = {};
    std::array<std::uint8_t, 4> seed_array_ = {};
    std::array<std::uint8_t, 16> ptr_array_ = {};
};

}  // namespace re2dj::hle::hardlock

#endif  // RE2DJ_HLE_HARDLOCK_ENGINE_H_
