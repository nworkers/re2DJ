#include "re2dj/graphics/sdl3_opengl_backend.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using re2dj::graphics::DecodeLegacyBlendFactor;
using re2dj::graphics::LegacyDrawCommand;
using re2dj::graphics::LegacyFixedFunctionState;
using re2dj::graphics::LegacyTextureView;
using re2dj::graphics::Sdl3OpenGlBackend;
using ReadPixelsFunction = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                                           void*);

constexpr std::uint32_t kSize = 32;

LegacyDrawCommand Quad(float left, float top, float right, float bottom,
                       std::uint32_t color)
{
    LegacyDrawCommand command;
    command.vertices = {
        {left, bottom, 1.0f, 1.0f, color, 0, 0.0f, 1.0f},
        {left, top, 1.0f, 1.0f, color, 0, 0.0f, 0.0f},
        {right, bottom, 1.0f, 1.0f, color, 0, 1.0f, 1.0f},
        {right, top, 1.0f, 1.0f, color, 0, 1.0f, 0.0f},
    };
    return command;
}

LegacyFixedFunctionState Blend(std::uint32_t source, std::uint32_t destination)
{
    LegacyFixedFunctionState state;
    if (!DecodeLegacyBlendFactor(source, &state.source_blend) ||
        !DecodeLegacyBlendFactor(destination, &state.destination_blend))
    {
        throw std::runtime_error("unsupported raw Direct3D blend factor");
    }
    state.alpha_blend_enabled = true;
    state.cull_mode = re2dj::graphics::CullMode::kCounterClockwise;
    state.minification_filter = re2dj::graphics::TextureFilter::kLinear;
    state.magnification_filter = re2dj::graphics::TextureFilter::kLinear;
    return state;
}

void Draw(Sdl3OpenGlBackend& backend, const LegacyDrawCommand& command,
          const LegacyFixedFunctionState& state, const LegacyTextureView* texture = nullptr)
{
    std::string error;
    if (!backend.Draw(command, state, kSize, kSize, texture, &error))
    {
        throw std::runtime_error(error);
    }
}

class PixelChecks
{
public:
    explicit PixelChecks(ReadPixelsFunction read_pixels) : read_pixels_(read_pixels) {}

    void Check(const char* name, GLint x, GLint y, const std::array<int, 3>& expected)
    {
        std::array<std::uint8_t, 4> pixel = {};
        read_pixels_(x, static_cast<GLint>(kSize) - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixel.data());
        bool matches = true;
        for (std::size_t channel = 0; channel < expected.size(); ++channel)
        {
            // Allow one RGB565 quantization step per channel.
            matches &= std::abs(static_cast<int>(pixel[channel]) - expected[channel]) <= 8;
        }
        ++checks_;
        failures_ += matches ? 0 : 1;
        std::cout << (matches ? "PASS " : "FAIL ") << name << " rgb="
                  << static_cast<int>(pixel[0]) << ',' << static_cast<int>(pixel[1]) << ','
                  << static_cast<int>(pixel[2]) << '\n';
    }

    int Finish() const
    {
        std::cout << "pixel checks: " << checks_ << ", failures: " << failures_ << '\n';
        return failures_ == 0 ? 0 : 1;
    }

private:
    ReadPixelsFunction read_pixels_;
    int checks_ = 0;
    int failures_ = 0;
};

}  // namespace

int main()
{
    try
    {
        Sdl3OpenGlBackend backend;
        std::string error;
        if (!backend.Initialize({nullptr, kSize, kSize, "re2DJ blend regression"}, &error))
        {
            throw std::runtime_error(error);
        }
        SDL_HideWindow(SDL_GL_GetCurrentWindow());
        const auto read_pixels =
            reinterpret_cast<ReadPixelsFunction>(SDL_GL_GetProcAddress("glReadPixels"));
        if (read_pixels == nullptr)
        {
            throw std::runtime_error("glReadPixels unavailable");
        }
        PixelChecks checks(read_pixels);
        const auto copy = Blend(2, 1);
        const auto additive = Blend(2, 2);
        auto mask_state = Blend(9, 6);
        mask_state.color_key_enabled = true;

        // White preserves the left half; near-black removes the right half.
        // The nonzero mask value avoids the active black source color key.
        const std::array<std::uint16_t, 2> mask_pixels = {0xffff, 0x0001};
        LegacyTextureView mask;
        mask.pixels = mask_pixels.data();
        mask.width = 2;
        mask.height = 1;
        mask.pitch = sizeof(mask_pixels);
        mask.identity = 1;
        mask.revision = 1;
        mask.source_color_key = {true, 0, 0};

        Draw(backend, Quad(0, 0, 32, 32, 0xff4080c0), copy);
        Draw(backend, Quad(16, 0, 32, 32, 0xffff0000), copy);
        checks.Check("disc before header mask", 24, 4, {255, 0, 0});
        const auto header = Quad(0, 0, 32, 16, 0xffffffff);
        Draw(backend, header, mask_state, &mask);
        checks.Check("mask removes disc under header", 24, 4, {0, 0, 0});
        checks.Check("white mask preserves background", 7, 4, {64, 128, 192});
        checks.Check("disc below header preserved", 24, 24, {255, 0, 0});
        Draw(backend, Quad(0, 0, 32, 16, 0xff008000), additive);
        checks.Check("header ink without underlying disc", 24, 4, {0, 128, 0});

        Draw(backend, Quad(0, 0, 32, 32, 0xffff0000), copy);
        Draw(backend, Quad(0, 0, 32, 32, 0x80000000), mask_state);
        checks.Check("partial source alpha retains half destination", 24, 4, {127, 0, 0});

        Draw(backend, Quad(0, 0, 32, 32, 0xff4080c0), copy);
        Draw(backend, Quad(0, 0, 32, 32, 0xffffffff), Blend(10, 1));
        checks.Check("inverse destination color", 24, 4, {191, 127, 63});

        Draw(backend, Quad(0, 0, 32, 32, 0xffff0000), copy);
        Draw(backend, header, Blend(1, 3), &mask);
        checks.Check("existing ZERO/SRCCOLOR mask", 24, 4, {0, 0, 0});

        // Present changes framebuffer and raster state. The next draw must
        // rebind the logical target and restore the requested blend/cull state.
        if (!backend.Present(&error))
        {
            throw std::runtime_error(error);
        }
        Draw(backend, Quad(0, 0, 32, 32, 0xffff0000), copy);
        Draw(backend, header, mask_state, &mask);
        checks.Check("mask after Present", 24, 4, {0, 0, 0});
        return checks.Finish();
    }
    catch (const std::exception& error)
    {
        std::cerr << "blend probe failed: " << error.what() << '\n';
        return 1;
    }
}
