// Checks the post-processing pass (design 455) on a real OpenGL driver: a
// white 64x48 frame presented into a 128x96 window must come out of
// `scanline` at full strength as alternating bright and dark rows, `crt` must
// draw with its curved-off corner black, a missing shader must leave the pass
// at `none`, `none` must show the frame as it is, and the guest's next draw
// must be unaffected. The window's back buffer is read just before the swap,
// through the present overlay. Run it on a desktop, as the blend probe is.

#include "re2dj/graphics/sdl3_opengl_backend.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/graphics/present_overlay.h"
#include "re2dj/graphics/window_policy.h"

namespace
{

using re2dj::graphics::LegacyDrawCommand;
using re2dj::graphics::LegacyFixedFunctionState;
using re2dj::graphics::PostShaderControl;
using re2dj::graphics::Sdl3OpenGlBackend;
using ReadPixelsFunction = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);

constexpr std::uint32_t kLogicalWidth = 64;
constexpr std::uint32_t kLogicalHeight = 48;
constexpr std::uint32_t kScale = 2;

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

// Keeps a copy of the composited back buffer from every present.
class Capture final : public re2dj::graphics::PresentOverlay
{
public:
    explicit Capture(ReadPixelsFunction read_pixels) : read_pixels_(read_pixels) {}

    void DrawOverlay(int pixel_width, int pixel_height) override
    {
        width_ = pixel_width;
        height_ = pixel_height;
        pixels_.assign(static_cast<std::size_t>(pixel_width) * static_cast<std::size_t>(pixel_height) * 4U, 0);
        read_pixels_(0, 0, pixel_width, pixel_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());
    }

    int width() const { return width_; }
    int height() const { return height_; }

    // Window pixel (x, y), y counted from the top.
    std::array<int, 3> At(int x, int y) const
    {
        const std::size_t row = static_cast<std::size_t>(height_ - 1 - y);
        const std::size_t offset = (row * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)) * 4U;
        return {pixels_[offset], pixels_[offset + 1], pixels_[offset + 2]};
    }

private:
    ReadPixelsFunction read_pixels_;
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> pixels_;
};

class Checks
{
public:
    void Record(const std::string& name, bool passed, const std::string& detail = std::string())
    {
        ++checks_;
        failures_ += passed ? 0 : 1;
        std::cout << (passed ? "PASS " : "FAIL ") << name;
        if (!detail.empty())
        {
            std::cout << "  (" << detail << ')';
        }
        std::cout << '\n';
    }

    int Finish() const
    {
        std::cout << "post-shader checks: " << checks_ << ", failures: " << failures_ << '\n';
        return failures_ == 0 ? 0 : 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};

void DrawFrame(Sdl3OpenGlBackend& backend, std::uint32_t color)
{
    std::string error;
    const LegacyFixedFunctionState copy;
    if (!backend.Draw(Quad(0, 0, kLogicalWidth, kLogicalHeight, color), copy, kLogicalWidth, kLogicalHeight,
                      nullptr, &error))
    {
        throw std::runtime_error(error);
    }
}

void Present(Sdl3OpenGlBackend& backend)
{
    std::string error;
    if (!backend.Present(&error))
    {
        throw std::runtime_error(error);
    }
}

void SetParameter(PostShaderControl* control, std::string_view name, float value)
{
    for (re2dj::graphics::PostShaderParameter& parameter : control->parameters())
    {
        if (parameter.name == name)
        {
            parameter.value = value;
            return;
        }
    }
    throw std::runtime_error("no parameter " + std::string(name));
}

std::string Rgb(const std::array<int, 3>& pixel)
{
    return std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]);
}

}  // namespace

int main()
{
    // Unbuffered, so a run the driver cuts short still shows how far it got.
    std::cout.setf(std::ios::unitbuf);
    try
    {
        Sdl3OpenGlBackend backend;
        std::string error;
        if (!backend.Initialize({nullptr, kLogicalWidth, kLogicalHeight, "re2DJ post-shader probe"}, &error) ||
            !backend.ResizeWindow(kLogicalWidth * kScale, kLogicalHeight * kScale, &error))
        {
            throw std::runtime_error(error);
        }
        const auto read_pixels = reinterpret_cast<ReadPixelsFunction>(SDL_GL_GetProcAddress("glReadPixels"));
        if (read_pixels == nullptr)
        {
            throw std::runtime_error("glReadPixels unavailable");
        }
        Capture capture(read_pixels);
        backend.SetPresentOverlay(&capture);
        PostShaderControl* control = backend.post_shader_control();
        Checks checks;
        checks.Record("post-processing pass available", control != nullptr);
        if (control == nullptr)
        {
            return checks.Finish();
        }
        checks.Record("starts at none", control->active_id() == "none");

        // A few presents let the window system settle the new size.
        for (int frame = 0; frame < 4; ++frame)
        {
            DrawFrame(backend, 0xffffffff);
            Present(backend);
        }
        const re2dj::graphics::PresentRect rect =
            re2dj::graphics::FitPresentation(capture.width(), capture.height(), kLogicalWidth, kLogicalHeight);
        const bool integer_scale = rect.width == static_cast<int>(kLogicalWidth * kScale) &&
                                   rect.height == static_cast<int>(kLogicalHeight * kScale);
        checks.Record("picture at 2x", integer_scale,
                      std::to_string(capture.width()) + "x" + std::to_string(capture.height()) + " window, " +
                          std::to_string(rect.width) + "x" + std::to_string(rect.height) + " picture");
        const int centre_x = rect.x + rect.width / 2;
        const int centre_y = rect.y + rect.height / 2;
        checks.Record("none shows the frame", capture.At(centre_x, centre_y) == std::array<int, 3>{255, 255, 255},
                      Rgb(capture.At(centre_x, centre_y)));

        // scanline at full strength: one row of every line lit, one dark.
        checks.Record("scanline selected", control->Select("scanline"), control->last_error());
        SetParameter(control, "SCANLINE_STRENGTH", 1.0f);
        DrawFrame(backend, 0xffffffff);
        Present(backend);
        int bright = 0;
        int dark = 0;
        for (int row = 0; row < rect.height; ++row)
        {
            const int value = capture.At(centre_x, rect.y + row)[1];
            bright += value >= 200 ? 1 : 0;
            dark += value <= 40 ? 1 : 0;
        }
        bool alternating = integer_scale;
        for (int line = 0; alternating && line < static_cast<int>(kLogicalHeight); ++line)
        {
            const int first = capture.At(centre_x, rect.y + line * 2)[1];
            const int second = capture.At(centre_x, rect.y + line * 2 + 1)[1];
            alternating = (first >= 200) != (second >= 200);
        }
        checks.Record("scanline rows alternate", alternating && bright == static_cast<int>(kLogicalHeight) &&
                                                    dark == static_cast<int>(kLogicalHeight),
                      "bright " + std::to_string(bright) + ", dark " + std::to_string(dark) + " of " +
                          std::to_string(rect.height));

        // crt with strong curvature: the corner falls outside the screen.
        checks.Record("crt selected", control->Select("crt"), control->last_error());
        SetParameter(control, "CRT_CURVATURE", 0.30f);
        DrawFrame(backend, 0xffffffff);
        Present(backend);
        const std::array<int, 3> corner = capture.At(rect.x, rect.y);
        const std::array<int, 3> centre = capture.At(centre_x, centre_y);
        checks.Record("crt corner black", corner[0] <= 8 && corner[1] <= 8 && corner[2] <= 8, Rgb(corner));
        checks.Record("crt centre lit", centre[0] + centre[1] + centre[2] >= 300, Rgb(centre));

        // The guest draws on after a post-processed present as before.
        DrawFrame(backend, 0xffff0000);
        checks.Record("missing shader refused", !control->Select("missing.glsl") && !control->last_error().empty(),
                      control->last_error());
        checks.Record("missing shader leaves none", control->active_id() == "none");
        Present(backend);
        checks.Record("guest draw after the pass", capture.At(centre_x, centre_y) == std::array<int, 3>{255, 0, 0},
                      Rgb(capture.At(centre_x, centre_y)));

        backend.SetPresentOverlay(nullptr);
        return checks.Finish();
    }
    catch (const std::exception& error)
    {
        std::cerr << "post-shader probe failed: " << error.what() << '\n';
        return 1;
    }
}
