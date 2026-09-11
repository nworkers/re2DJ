#include "re2dj/hle/hardlock/engine.h"

#include <algorithm>
#include <cstring>

namespace re2dj::hle::hardlock
{
namespace
{

constexpr std::uint8_t Rol8(std::uint8_t value, int shift)
{
    return static_cast<std::uint8_t>((value << shift) | (value >> (8 - shift)));
}

constexpr std::uint16_t Rol16(std::uint16_t value, int shift)
{
    return static_cast<std::uint16_t>((value << shift) | (value >> (16 - shift)));
}

}  // namespace

HardlockEngine::HardlockEngine(const HardlockSeeds& seeds)
{
    Init(seeds);
}

void HardlockEngine::Init(const HardlockSeeds& seeds)
{
    seeds_ = seeds;

    seed_array_[0] = static_cast<std::uint8_t>((seeds.seed3 & 0xF000) >> 12);
    seed_array_[1] = static_cast<std::uint8_t>((seeds.seed3 & 0x0F00) >> 8);
    seed_array_[2] = static_cast<std::uint8_t>((seeds.seed3 & 0x00F0) >> 4);
    seed_array_[3] = static_cast<std::uint8_t>(seeds.seed3 & 0x000F);

    for (int i = 0; i < 16; ++i)
    {
        ptr_array_[i] = static_cast<std::uint8_t>(((seeds.seed1 >> i) & 1) << 1);
        ptr_array_[i] |= static_cast<std::uint8_t>((seeds.seed2 >> i) & 1);
    }
}

void HardlockEngine::SetDongleData(std::uint8_t data, InternalState& state) const
{
    data &= 0x0f;
    const std::uint8_t tmp_ptr = ptr_array_[state.var1];
    state.var4 = static_cast<std::uint16_t>((state.var4 << 4) | tmp_ptr);
    state.var1 ^= data;
    state.var1 ^= state.var2;
    state.var1 ^= seed_array_[tmp_ptr];
    state.var2 = static_cast<std::uint8_t>((((state.var2 << 2) + tmp_ptr) >> 1) & 0x0f);
}

std::uint8_t HardlockEngine::GetBitFromDongleData(const InternalState& state) const
{
    return static_cast<std::uint8_t>((state.var4 >> 12) & 1);
}

std::uint16_t HardlockEngine::Transform0Hw(std::uint16_t w0,
                                           std::uint16_t ret_w,
                                           InternalState& state) const
{
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            SetDongleData(static_cast<std::uint8_t>((w0 & 0xff) >> 2), state);
            if (w0 & 0x8000)
            {
                w0 = static_cast<std::uint16_t>((w0 << 1) + 1);
            }
            else
            {
                w0 = static_cast<std::uint16_t>(w0 << 1);
            }
        }
        ret_w = static_cast<std::uint16_t>(ret_w >> 1);
        const std::uint16_t nb = static_cast<std::uint16_t>((state.var4 >> 12) & 1);
        if (nb == 0)
        {
            ret_w |= 0x8000;
        }
    }
    return ret_w;
}

std::uint16_t HardlockEngine::Transform0(std::uint16_t w3, std::uint16_t w4) const
{
    InternalState state;
    const std::uint16_t step1 = Transform0Hw(w3, 0, state);
    const std::uint16_t step2 = Transform0Hw(w4, step1, state);
    std::uint16_t ax = static_cast<std::uint16_t>(w4 ^ step2);
    ax = Rol16(ax, 1);
    const auto ax_low = static_cast<std::uint8_t>(ax & 0xff);
    const auto ax_high = static_cast<std::uint8_t>((ax >> 8) & 0xff);
    const auto sum_low = static_cast<std::uint8_t>(ax_low + ax_high);
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(ax_high) << 8) | sum_low);
}

void HardlockEngine::EncryptBlock(std::span<std::uint8_t, 8> block) const
{
    std::uint16_t w1 = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
    std::uint16_t w2 = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
    std::uint16_t w3 = static_cast<std::uint16_t>(block[4] | (block[5] << 8));
    std::uint16_t w4 = static_cast<std::uint16_t>(block[6] | (block[7] << 8));

    for (int i = 0; i < 5; ++i)
    {
        std::uint16_t transf = static_cast<std::uint16_t>(w3 ^ w4 ^ Transform0(w3, w4));
        const auto t_low = static_cast<std::uint8_t>(transf & 0xff);
        const auto t_high = static_cast<std::uint8_t>((transf >> 8) & 0xff);

        std::uint8_t tmp1 = static_cast<std::uint8_t>(t_high + t_low);
        tmp1 = Rol8(tmp1, 1);
        tmp1++;
        tmp1 = Rol8(tmp1, 1);

        std::uint8_t tmp2 = static_cast<std::uint8_t>(t_low + tmp1);
        tmp2 = Rol8(tmp2, 1);
        tmp2 = Rol8(tmp2, 1);

        transf = static_cast<std::uint16_t>((static_cast<std::uint16_t>(tmp1) << 8) | tmp2);

        std::uint16_t next_w4 = static_cast<std::uint16_t>(transf + w4);
        next_w4 = static_cast<std::uint16_t>((((next_w4 + 1) & 0xff) + (next_w4 & 0xff00)));
        next_w4 = Rol16(next_w4, 2);

        transf ^= w1;
        next_w4 ^= w2;
        w1 = w3;
        w2 = w4;
        w3 ^= transf;
        w4 ^= next_w4;
    }

    block[0] = static_cast<std::uint8_t>(w3 & 0xff);
    block[1] = static_cast<std::uint8_t>((w3 >> 8) & 0xff);
    block[2] = static_cast<std::uint8_t>(w4 & 0xff);
    block[3] = static_cast<std::uint8_t>((w4 >> 8) & 0xff);
    block[4] = static_cast<std::uint8_t>(w1 & 0xff);
    block[5] = static_cast<std::uint8_t>((w1 >> 8) & 0xff);
    block[6] = static_cast<std::uint8_t>(w2 & 0xff);
    block[7] = static_cast<std::uint8_t>((w2 >> 8) & 0xff);
}

std::array<std::uint8_t, 8> HardlockEngine::CryptBlock(
    std::span<const std::uint8_t, 8> challenge) const
{
    std::array<std::uint8_t, 8> response = {};
    std::copy_n(challenge.begin(), 8, response.begin());
    EncryptBlock(response);
    return response;
}

std::uint8_t HardlockEngine::CipherFunction(std::uint32_t& r,
                                            std::uint32_t& tmp_r,
                                            InternalState& state) const
{
    auto* data = reinterpret_cast<std::uint8_t*>(&tmp_r);
    std::uint8_t sum_bits = 0;
    std::uint8_t bit_from_dongle = 1;
    int outer_loop = 9;
    state = InternalState{};

    while (outer_loop > 0)
    {
        for (int inner = 0; inner < 4; ++inner)
        {
            const std::uint8_t tmp1 = Rol8(data[inner], 1);
            SetDongleData(data[inner], state);
            const auto tmp2 = static_cast<std::uint8_t>((bit_from_dongle + inner + 1) & 3);
            const std::uint8_t tmp3 = Rol8(static_cast<std::uint8_t>(data[tmp2] + tmp2 + tmp1), 1);
            data[inner] = tmp3;
        }
        bit_from_dongle = GetBitFromDongleData(state);
        sum_bits += bit_from_dongle;
        outer_loop--;
    }
    r ^= tmp_r;
    return sum_bits;
}

void HardlockEngine::CodePayloadInPlace(std::span<const std::uint8_t, 8> input,
                                        std::span<std::uint8_t, 56> output) const
{
    InternalState state;
    std::array<std::uint8_t, 0x50> data = {};
    std::copy_n(input.begin(), 8, data.begin() + 0x28);

    auto* dongle_bit_counter = reinterpret_cast<std::uint16_t*>(&data[0x1C]);
    auto* l = reinterpret_cast<std::uint32_t*>(&data[0x28]);
    auto* r = reinterpret_cast<std::uint32_t*>(&data[0x2C]);
    auto* tmp_r1 = reinterpret_cast<std::uint32_t*>(&data[0x20]);
    auto* tmp_r2 = reinterpret_cast<std::uint32_t*>(&data[0x24]);

    std::uint8_t* tmp_data = data.data();
    int loop_counter = 5;

    while (loop_counter >= 0)
    {
        *tmp_r1 = *r;
        *tmp_r2 = *r;

        *dongle_bit_counter = static_cast<std::uint16_t>(
            *dongle_bit_counter + CipherFunction(*r, *tmp_r2, state));
        *r ^= *l;
        *l = *tmp_r1;

        tmp_data += 4;
        std::memcpy(tmp_data - 4, tmp_r2, sizeof(std::uint32_t));

        loop_counter--;
    }

    std::copy_n(data.begin(), 56, output.begin());
}

std::array<std::uint8_t, 56> HardlockEngine::CodePayload(
    std::span<const std::uint8_t, 8> input) const
{
    std::array<std::uint8_t, 56> response = {};
    CodePayloadInPlace(input, response);
    return response;
}

std::uint8_t HardlockEngine::Calc(std::uint16_t p1, std::uint16_t p2) const
{
    const std::uint8_t i1 = Rol8(static_cast<std::uint8_t>((p2 >> 8) & 0xff), 4);
    const std::uint8_t i2 = Rol8(static_cast<std::uint8_t>(p2 & 0xff), 4);
    const std::uint8_t i3 = Rol8(static_cast<std::uint8_t>((p1 >> 8) & 0xff), 4);
    const std::uint8_t i4 = Rol8(static_cast<std::uint8_t>(p1 & 0xff), 4);

    std::uint32_t input = (static_cast<std::uint32_t>(i1) << 24) |
                          (static_cast<std::uint32_t>(i2) << 16) |
                          (static_cast<std::uint32_t>(i3) << 8)  |
                          static_cast<std::uint32_t>(i4);

    std::uint8_t var1 = 0x0f;
    std::uint8_t output = 0;

    for (int i = 0; i < 8; ++i)
    {
        const std::uint8_t tmp_ptr = ptr_array_[var1];
        var1 ^= static_cast<std::uint8_t>(input & 0x0f);
        var1 ^= seed_array_[tmp_ptr];

        output = static_cast<std::uint8_t>((output << 1) | (ptr_array_[var1] & 1));
        input >>= 4;
    }

    const std::uint8_t k = output;
    return static_cast<std::uint8_t>(~(((k << 7) & 0x80) |
                                       ((k << 5) & 0x40) |
                                       ((k << 3) & 0x20) |
                                       ((k << 1) & 0x10) |
                                       ((k >> 7) & 0x01) |
                                       ((k >> 5) & 0x02) |
                                       ((k >> 3) & 0x04) |
                                       ((k >> 1) & 0x08)));
}

}  // namespace re2dj::hle::hardlock
