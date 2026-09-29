// Checks 32-bit colour (design 429) on a real OpenGL driver: the render
// target keeps 8 bits per channel through draws and blends, textures take
// their colours from a true-color plane and their key from RGB565, a change
// of depth carries the picture over, and a guest's render-target write keeps
// the pixels it left alone. Run it on a desktop, as the blend probe is.

#include "re2dj/graphics/sdl3_opengl_backend.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "re2dj/graphics/color_depth.h"
#include "re2dj/graphics/true_color.h"

namespace
{

using re2dj::graphics::ColorDepth;
using re2dj::graphics::LegacyDrawCommand;
using re2dj::graphics::LegacyFixedFunctionState;
using re2dj::graphics::LegacyTextureView;
using re2dj::graphics::Sdl3OpenGlBackend;
using ReadPixelsFunction = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);

constexpr std::uint32_t kSize = 32;

LegacyDrawCommand Quad(float left, float top, float right, float bottom, std::uint32_t color)
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

LegacyFixedFunctionState AlphaBlend()
{
    LegacyFixedFunctionState state;
    // D3DBLEND_SRCALPHA and D3DBLEND_INVSRCALPHA.
    if (!re2dj::graphics::DecodeLegacyBlendFactor(5, &state.source_blend) ||
        !re2dj::graphics::DecodeLegacyBlendFactor(6, &state.destination_blend))
    {
        throw std::runtime_error("unsupported raw Direct3D blend factor");
    }
    state.alpha_blend_enabled = true;
    return state;
}

void Draw(Sdl3OpenGlBackend& backend, const LegacyDrawCommand& command, const LegacyFixedFunctionState& state,
          const LegacyTextureView* texture = nullptr)
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

    std::array<int, 3> Read(GLint x, GLint y) const
    {
        std::array<std::uint8_t, 4> pixel = {};
        read_pixels_(x, static_cast<GLint>(kSize) - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return {pixel[0], pixel[1], pixel[2]};
    }

    // Tolerance is per channel: 1 for 8-bit exactness, a 565 step otherwise.
    void Check(const char* name, GLint x, GLint y, const std::array<int, 3>& expected, int tolerance)
    {
        const std::array<int, 3> pixel = Read(x, y);
        bool matches = true;
        for (std::size_t channel = 0; channel < expected.size(); ++channel)
        {
            matches &= std::abs(pixel[channel] - expected[channel]) <= tolerance;
        }
        Record(name, matches);
        std::cout << "  rgb=" << pixel[0] << ',' << pixel[1] << ',' << pixel[2] << " expected " << expected[0]
                  << ',' << expected[1] << ',' << expected[2] << '\n';
    }

    void Record(const char* name, bool passed)
    {
        ++checks_;
        failures_ += passed ? 0 : 1;
        std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
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
    // Unbuffered, so a run the driver cuts short still shows how far it got.
    std::cout.setf(std::ios::unitbuf);
    try
    {
        re2dj::graphics::SelectColorDepth(ColorDepth::k32);
        Sdl3OpenGlBackend backend;
        std::string error;
        if (!backend.Initialize({nullptr, kSize, kSize, "re2DJ true-color regression"}, &error))
        {
            throw std::runtime_error(error);
        }
        SDL_HideWindow(SDL_GL_GetCurrentWindow());
        const auto read_pixels = reinterpret_cast<ReadPixelsFunction>(SDL_GL_GetProcAddress("glReadPixels"));
        if (read_pixels == nullptr)
        {
            throw std::runtime_error("glReadPixels unavailable");
        }
        PixelChecks checks(read_pixels);
        if (backend.true_color_unavailable())
        {
            std::cout << "the driver cannot render into RGB8\n";
        }
        checks.Record("render target starts 32-bit", backend.render_target_depth() == ColorDepth::k32);

        // A colour 565 cannot hold stays exact.
        const LegacyFixedFunctionState copy;
        Draw(backend, Quad(0, 0, 32, 32, 0xff808183), copy);
        checks.Check("8-bit vertex colour", 24, 4, {128, 129, 131}, 1);

        // Sixteen translucent layers accumulate at 8 bits, as each step
        // rounds: c = c * (1 - a) + 255 * a.
        Draw(backend, Quad(0, 0, 32, 32, 0xff000000), copy);
        const LegacyFixedFunctionState blend = AlphaBlend();
        double expected = 0.0;
        for (int layer = 0; layer < 16; ++layer)
        {
            Draw(backend, Quad(0, 0, 32, 32, 0x08ffffff), blend);
            expected = std::round(expected * (1.0 - 8.0 / 255.0) + 255.0 * 8.0 / 255.0);
        }
        const int layered = static_cast<int>(expected);
        checks.Check("blend accumulation", 24, 4, {layered, layered, layered}, 2);

        // A texture's colours come from its plane, its key from RGB565.
        const std::array<std::uint32_t, 2> plane = {0x00808183U, 0x00FF07FFU};
        const std::array<std::uint16_t, 2> pixels = {re2dj::graphics::NarrowToRgb565(plane[0]),
                                                     re2dj::graphics::NarrowToRgb565(plane[1])};
        LegacyTextureView texture;
        texture.pixels = pixels.data();
        texture.width = 2;
        texture.height = 1;
        texture.pitch = sizeof(pixels);
        texture.identity = 1;
        texture.revision = 1;
        texture.source_color_key = {true, pixels[1], pixels[1]};
        texture.true_color = plane.data();
        texture.true_color_stride = 2;
        Draw(backend, Quad(0, 0, 32, 32, 0xff00ff00), copy);
        LegacyFixedFunctionState keyed;
        keyed.color_key_enabled = true;
        Draw(backend, Quad(0, 0, 32, 32, 0xffffffff), keyed, &texture);
        checks.Check("texel from the plane", 4, 4, {128, 129, 131}, 1);
        checks.Check("keyed texel dropped", 24, 4, {0, 255, 0}, 1);

        // Back to 16 bits: the picture carries over, narrowed.
        re2dj::graphics::SelectColorDepth(ColorDepth::k16);
        Draw(backend, Quad(31, 31, 32, 32, 0xff000000), copy);
        checks.Record("render target follows to 16-bit", backend.render_target_depth() == ColorDepth::k16);
        checks.Check("picture kept at 16 bits", 4, 4, {132, 130, 132}, 4);
        // The same texture now uploads from RGB565.
        Draw(backend, Quad(0, 0, 32, 32, 0xffffffff), keyed, &texture);
        checks.Check("texel from RGB565", 4, 4, {132, 130, 132}, 4);

        // And to 32 again, keeping what 16 bits left. The frame after a
        // present starts from nothing unless the guest flips, so this one
        // retains.
        re2dj::graphics::SelectColorDepth(ColorDepth::k32);
        backend.SetRetainBetweenFrames(true);
        if (!backend.Present(&error))
        {
            throw std::runtime_error(error);
        }
        checks.Record("render target follows to 32-bit on Present",
                      backend.render_target_depth() == ColorDepth::k32);
        Draw(backend, Quad(31, 31, 32, 32, 0xff000000), copy);
        checks.Check("picture kept at 32 bits", 24, 4, {0, 255, 0}, 4);

        // A guest's render-target round trip keeps what it did not change.
        if (!backend.ClearRenderTargetColor(0x00808183U, &error))
        {
            throw std::runtime_error(error);
        }
        std::array<std::uint8_t, 8> row = {};
        if (!backend.ReadRenderTarget(0, 4, 4, 1, row, 8, &error))
        {
            throw std::runtime_error(error);
        }
        checks.Record("read narrows as a plane does",
                      static_cast<std::uint16_t>(row[0] | (row[1] << 8)) ==
                          re2dj::graphics::NarrowToRgb565(0x00808183U));
        row[2] = 0x00;
        row[3] = 0xF8;
        if (!backend.WriteRenderTarget(0, 4, 4, 1, row, 8, &error))
        {
            throw std::runtime_error(error);
        }
        checks.Check("untouched pixel keeps 8 bits", 0, 4, {128, 129, 131}, 1);
        checks.Check("changed pixel takes the write", 1, 4, {255, 0, 0}, 1);
        return checks.Finish();
    }
    catch (const std::exception& error)
    {
        std::cerr << "true-color probe failed: " << error.what() << '\n';
        return 1;
    }
}
