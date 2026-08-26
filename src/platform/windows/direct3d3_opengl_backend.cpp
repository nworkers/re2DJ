#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "direct3d3_opengl_backend.h"

namespace re2dj::platform::windows
{
namespace
{

using GlChar = char;
using CreateShaderFunction = GLuint(APIENTRY*)(GLenum);
using ShaderSourceFunction = void(APIENTRY*)(GLuint, GLsizei, const GlChar* const*, const GLint*);
using CompileShaderFunction = void(APIENTRY*)(GLuint);
using GetShaderIvFunction = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetShaderInfoLogFunction = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GlChar*);
using DeleteShaderFunction = void(APIENTRY*)(GLuint);
using CreateProgramFunction = GLuint(APIENTRY*)();
using AttachShaderFunction = void(APIENTRY*)(GLuint, GLuint);
using BindAttribLocationFunction = void(APIENTRY*)(GLuint, GLuint, const GlChar*);
using LinkProgramFunction = void(APIENTRY*)(GLuint);
using GetProgramIvFunction = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetProgramInfoLogFunction = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GlChar*);
using DeleteProgramFunction = void(APIENTRY*)(GLuint);
using UseProgramFunction = void(APIENTRY*)(GLuint);
using GetUniformLocationFunction = GLint(APIENTRY*)(GLuint, const GlChar*);
using Uniform1iFunction = void(APIENTRY*)(GLint, GLint);
using Uniform1fFunction = void(APIENTRY*)(GLint, GLfloat);
using Uniform2fFunction = void(APIENTRY*)(GLint, GLfloat, GLfloat);
using EnableVertexAttribArrayFunction = void(APIENTRY*)(GLuint);
using DisableVertexAttribArrayFunction = void(APIENTRY*)(GLuint);
using VertexAttribPointerFunction =
    void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

constexpr GLenum kVertexShader = 0x8b31;
constexpr GLenum kFragmentShader = 0x8b30;
constexpr GLenum kCompileStatus = 0x8b81;
constexpr GLenum kLinkStatus = 0x8b82;
constexpr GLenum kClampToEdge = 0x812f;

template <typename Function>
bool LoadGlFunction(const char* name, Function* function)
{
    *function = reinterpret_cast<Function>(wglGetProcAddress(name));
    return *function != nullptr && *function != reinterpret_cast<Function>(1) &&
           *function != reinterpret_cast<Function>(2) &&
           *function != reinterpret_cast<Function>(3) &&
           *function != reinterpret_cast<Function>(-1);
}

struct GlVertex
{
    float position[4];
    float color[4];
    float texture[2];
};

std::array<float, 4> ArgbToFloats(std::uint32_t argb)
{
    constexpr float scale = 1.0f / 255.0f;
    return {static_cast<float>((argb >> 16) & 0xff) * scale,
            static_cast<float>((argb >> 8) & 0xff) * scale,
            static_cast<float>(argb & 0xff) * scale,
            static_cast<float>((argb >> 24) & 0xff) * scale};
}

}  // namespace

struct Direct3d3OpenGlBackend::Impl
{
    struct CachedTexture
    {
        GLuint name = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t revision = 0;
        graphics::Rgb565ColorKey color_key;
    };

    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC context = nullptr;
    GLuint program = 0;
    std::unordered_map<std::uint64_t, CachedTexture> textures;
    bool frame_started = false;

    CreateShaderFunction create_shader = nullptr;
    ShaderSourceFunction shader_source = nullptr;
    CompileShaderFunction compile_shader = nullptr;
    GetShaderIvFunction get_shader_iv = nullptr;
    GetShaderInfoLogFunction get_shader_info_log = nullptr;
    DeleteShaderFunction delete_shader = nullptr;
    CreateProgramFunction create_program = nullptr;
    AttachShaderFunction attach_shader = nullptr;
    BindAttribLocationFunction bind_attrib_location = nullptr;
    LinkProgramFunction link_program = nullptr;
    GetProgramIvFunction get_program_iv = nullptr;
    GetProgramInfoLogFunction get_program_info_log = nullptr;
    DeleteProgramFunction delete_program = nullptr;
    UseProgramFunction use_program = nullptr;
    GetUniformLocationFunction get_uniform_location = nullptr;
    Uniform1iFunction uniform_1i = nullptr;
    Uniform1fFunction uniform_1f = nullptr;
    Uniform2fFunction uniform_2f = nullptr;
    EnableVertexAttribArrayFunction enable_vertex_attrib_array = nullptr;
    DisableVertexAttribArrayFunction disable_vertex_attrib_array = nullptr;
    VertexAttribPointerFunction vertex_attrib_pointer = nullptr;

    bool MakeCurrent(std::string* error)
    {
        if (wglMakeCurrent(dc, context) == FALSE)
        {
            *error = "cannot make the Direct3D3 OpenGL context current";
            return false;
        }
        return true;
    }

    GLuint Compile(GLenum type, const char* source, std::string* error)
    {
        const GLuint shader = create_shader(type);
        if (shader == 0)
        {
            *error = "cannot create an OpenGL shader";
            return 0;
        }
        shader_source(shader, 1, &source, nullptr);
        compile_shader(shader);
        GLint status = GL_FALSE;
        get_shader_iv(shader, kCompileStatus, &status);
        if (status == GL_TRUE)
        {
            return shader;
        }
        std::array<char, 512> message = {};
        GLsizei length = 0;
        get_shader_info_log(shader,
                            static_cast<GLsizei>(message.size() - 1),
                            &length,
                            message.data());
        delete_shader(shader);
        *error = std::string("OpenGL shader compilation failed: ") + message.data();
        return 0;
    }

    bool CreateProgram(std::string* error)
    {
        constexpr char vertex_source[] =
            "#version 120\n"
            "attribute vec4 a_position;\n"
            "attribute vec4 a_color;\n"
            "attribute vec2 a_texture;\n"
            "uniform vec2 u_viewport;\n"
            "varying vec4 v_color;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  float clip_w = 1.0 / a_position.w;\n"
            "  vec2 ndc = vec2(a_position.x * 2.0 / u_viewport.x - 1.0,\n"
            "                  1.0 - a_position.y * 2.0 / u_viewport.y);\n"
            "  gl_Position = vec4(ndc * clip_w, (a_position.z * 2.0 - 1.0) * clip_w, clip_w);\n"
            "  v_color = a_color;\n"
            "  v_texture = a_texture;\n"
            "}\n";
        constexpr char fragment_source[] =
            "#version 120\n"
            "uniform sampler2D u_texture;\n"
            "uniform int u_texture_enabled;\n"
            "uniform int u_color_key_enabled;\n"
            "uniform int u_alpha_test_enabled;\n"
            "uniform float u_alpha_reference;\n"
            "varying vec4 v_color;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  vec4 texel = u_texture_enabled != 0 ? texture2D(u_texture, v_texture) : vec4(1.0);\n"
            // Direct3D color keying drops matching texels regardless of the
            // blend factors, so it cannot ride on the alpha channel alone: a
            // copy blend of ONE and ZERO would write them out as solid key
            // color. Upload marks matching texels with zero alpha, and this
            // branch is what actually removes them.
            "  if (u_color_key_enabled != 0 && texel.a < 0.5) discard;\n"
            "  vec4 output_color = texel * v_color;\n"
            "  if (u_alpha_test_enabled != 0 &&\n"
            "      abs(output_color.a - u_alpha_reference) < 0.001) discard;\n"
            "  gl_FragColor = output_color;\n"
            "}\n";

        const GLuint vertex_shader = Compile(kVertexShader, vertex_source, error);
        if (vertex_shader == 0)
        {
            return false;
        }
        const GLuint fragment_shader = Compile(kFragmentShader, fragment_source, error);
        if (fragment_shader == 0)
        {
            delete_shader(vertex_shader);
            return false;
        }
        program = create_program();
        attach_shader(program, vertex_shader);
        attach_shader(program, fragment_shader);
        bind_attrib_location(program, 0, "a_position");
        bind_attrib_location(program, 1, "a_color");
        bind_attrib_location(program, 2, "a_texture");
        link_program(program);
        delete_shader(vertex_shader);
        delete_shader(fragment_shader);
        GLint status = GL_FALSE;
        get_program_iv(program, kLinkStatus, &status);
        if (status == GL_TRUE)
        {
            return true;
        }
        std::array<char, 512> message = {};
        GLsizei length = 0;
        get_program_info_log(program,
                             static_cast<GLsizei>(message.size() - 1),
                             &length,
                             message.data());
        delete_program(program);
        program = 0;
        *error = std::string("OpenGL program link failed: ") + message.data();
        return false;
    }
};

Direct3d3OpenGlBackend::Direct3d3OpenGlBackend() = default;

Direct3d3OpenGlBackend::~Direct3d3OpenGlBackend()
{
    if (impl_ == nullptr)
    {
        return;
    }
    if (impl_->context != nullptr && impl_->dc != nullptr &&
        wglMakeCurrent(impl_->dc, impl_->context) != FALSE)
    {
        for (const auto& [identity, texture] : impl_->textures)
        {
            (void)identity;
            glDeleteTextures(1, &texture.name);
        }
        if (impl_->program != 0 && impl_->delete_program != nullptr)
        {
            impl_->delete_program(impl_->program);
        }
        wglMakeCurrent(nullptr, nullptr);
    }
    if (impl_->context != nullptr)
    {
        wglDeleteContext(impl_->context);
    }
    if (impl_->dc != nullptr && impl_->window != nullptr)
    {
        ReleaseDC(impl_->window, impl_->dc);
    }
    delete impl_;
}

bool Direct3d3OpenGlBackend::Initialize(HWND window, std::string* error)
{
    if (impl_ != nullptr || window == nullptr || error == nullptr)
    {
        return false;
    }
    auto* const impl = new (std::nothrow) Impl;
    if (impl == nullptr)
    {
        *error = "cannot allocate the Direct3D3 OpenGL backend";
        return false;
    }
    impl_ = impl;
    impl->window = window;
    impl->dc = GetDC(window);
    PIXELFORMATDESCRIPTOR descriptor = {};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cDepthBits = 24;
    descriptor.iLayerType = PFD_MAIN_PLANE;
    if (impl->dc == nullptr)
    {
        *error = "cannot acquire the Direct3D3 window DC";
        return false;
    }
    if (GetPixelFormat(impl->dc) == 0)
    {
        const int format = ChoosePixelFormat(impl->dc, &descriptor);
        if (format == 0 || SetPixelFormat(impl->dc, format, &descriptor) == FALSE)
        {
            *error = "cannot set the Direct3D3 OpenGL pixel format";
            return false;
        }
    }
    impl->context = wglCreateContext(impl->dc);
    if (impl->context == nullptr || !impl->MakeCurrent(error))
    {
        return false;
    }

    const bool loaded =
        LoadGlFunction("glCreateShader", &impl->create_shader) &&
        LoadGlFunction("glShaderSource", &impl->shader_source) &&
        LoadGlFunction("glCompileShader", &impl->compile_shader) &&
        LoadGlFunction("glGetShaderiv", &impl->get_shader_iv) &&
        LoadGlFunction("glGetShaderInfoLog", &impl->get_shader_info_log) &&
        LoadGlFunction("glDeleteShader", &impl->delete_shader) &&
        LoadGlFunction("glCreateProgram", &impl->create_program) &&
        LoadGlFunction("glAttachShader", &impl->attach_shader) &&
        LoadGlFunction("glBindAttribLocation", &impl->bind_attrib_location) &&
        LoadGlFunction("glLinkProgram", &impl->link_program) &&
        LoadGlFunction("glGetProgramiv", &impl->get_program_iv) &&
        LoadGlFunction("glGetProgramInfoLog", &impl->get_program_info_log) &&
        LoadGlFunction("glDeleteProgram", &impl->delete_program) &&
        LoadGlFunction("glUseProgram", &impl->use_program) &&
        LoadGlFunction("glGetUniformLocation", &impl->get_uniform_location) &&
        LoadGlFunction("glUniform1i", &impl->uniform_1i) &&
        LoadGlFunction("glUniform1f", &impl->uniform_1f) &&
        LoadGlFunction("glUniform2f", &impl->uniform_2f) &&
        LoadGlFunction("glEnableVertexAttribArray", &impl->enable_vertex_attrib_array) &&
        LoadGlFunction("glDisableVertexAttribArray", &impl->disable_vertex_attrib_array) &&
        LoadGlFunction("glVertexAttribPointer", &impl->vertex_attrib_pointer);
    if (!loaded)
    {
        *error = "required OpenGL shader entry points are unavailable";
        return false;
    }
    if (!impl->CreateProgram(error))
    {
        return false;
    }
    error->clear();
    return true;
}

bool Direct3d3OpenGlBackend::Draw(const graphics::LegacyDrawCommand& command,
                                  const graphics::LegacyFixedFunctionState& state,
                                  std::uint32_t logical_width,
                                  std::uint32_t logical_height,
                                  const graphics::LegacyTextureView* texture_view,
                                  std::string* error)
{
    if (impl_ == nullptr || error == nullptr || logical_width == 0 || logical_height == 0 ||
        (command.topology == graphics::PrimitiveTopology::kTriangleStrip &&
         command.vertices.size() < 3) ||
        (command.topology == graphics::PrimitiveTopology::kLineList &&
         (command.vertices.size() < 2 || command.vertices.size() % 2 != 0)) ||
        !impl_->MakeCurrent(error))
    {
        return false;
    }
    RECT client = {};
    if (GetClientRect(impl_->window, &client) == FALSE || client.right <= 0 || client.bottom <= 0)
    {
        *error = "cannot query the Direct3D3 OpenGL client rectangle";
        return false;
    }
    if (!impl_->frame_started)
    {
        glViewport(0, 0, client.right, client.bottom);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        impl_->frame_started = true;
    }

    std::vector<GlVertex> vertices;
    vertices.reserve(command.vertices.size());
    for (const graphics::TransformedLitVertex& input : command.vertices)
    {
        GlVertex output = {};
        output.position[0] = input.x;
        output.position[1] = input.y;
        output.position[2] = input.z;
        output.position[3] = command.topology == graphics::PrimitiveTopology::kLineList &&
                                     input.reciprocal_w == 0.0f
                                 ? 1.0f
                                 : input.reciprocal_w;
        const std::array<float, 4> color = ArgbToFloats(input.diffuse_argb);
        for (std::size_t index = 0; index < color.size(); ++index)
        {
            output.color[index] = color[index];
        }
        output.texture[0] = input.texture_u;
        output.texture[1] = input.texture_v;
        vertices.push_back(output);
    }

    impl_->use_program(impl_->program);
    impl_->uniform_2f(impl_->get_uniform_location(impl_->program, "u_viewport"),
                     static_cast<float>(logical_width),
                     static_cast<float>(logical_height));
    const bool has_texture = texture_view != nullptr && texture_view->pixels != nullptr &&
                             texture_view->width != 0 && texture_view->height != 0;
    if (has_texture &&
        (texture_view->pitch < static_cast<std::uint64_t>(texture_view->width) *
                                   sizeof(std::uint16_t) ||
         texture_view->pitch % sizeof(std::uint16_t) != 0))
    {
        *error = "invalid RGB565 texture row pitch";
        return false;
    }
    const bool color_key_active = has_texture && state.color_key_enabled &&
                                  texture_view->source_color_key.enabled;
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_texture"), 0);
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_texture_enabled"),
                     has_texture ? 1 : 0);
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_color_key_enabled"),
                     color_key_active ? 1 : 0);
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_alpha_test_enabled"),
                     state.alpha_test_enabled ? 1 : 0);
    const GLint alpha_reference =
        impl_->get_uniform_location(impl_->program, "u_alpha_reference");
    if (alpha_reference >= 0)
    {
        impl_->uniform_1f(alpha_reference,
                         static_cast<float>(state.alpha_reference) / 255.0f);
    }
    if (has_texture)
    {
        if (texture_view->identity == 0 || texture_view->revision == 0)
        {
            *error = "RGB565 texture identity and revision must be nonzero";
            return false;
        }
        Impl::CachedTexture& cached = impl_->textures[texture_view->identity];
        if (cached.name == 0)
        {
            glGenTextures(1, &cached.name);
            if (cached.name == 0)
            {
                impl_->textures.erase(texture_view->identity);
                *error = "cannot create a cached Direct3D3 OpenGL texture";
                return false;
            }
            glBindTexture(GL_TEXTURE_2D, cached.name);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClampToEdge);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClampToEdge);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, cached.name);
        }
        graphics::Rgb565ColorKey effective_key = texture_view->source_color_key;
        effective_key.enabled = color_key_active;
        const bool key_changed = cached.color_key.enabled != effective_key.enabled ||
                                 cached.color_key.low != effective_key.low ||
                                 cached.color_key.high != effective_key.high;
        if (cached.revision != texture_view->revision || key_changed ||
            cached.width != texture_view->width || cached.height != texture_view->height)
        {
            const std::uint64_t pixel_count =
                static_cast<std::uint64_t>(texture_view->width) * texture_view->height;
            if (pixel_count > (std::numeric_limits<std::size_t>::max)() / 4)
            {
                *error = "RGB565 texture conversion size overflows host memory";
                return false;
            }
            std::vector<std::uint8_t> rgba(static_cast<std::size_t>(pixel_count) * 4);
            const auto* const source = static_cast<const std::uint8_t*>(texture_view->pixels);
            for (std::uint32_t y = 0; y < texture_view->height; ++y)
            {
                const auto* const row = reinterpret_cast<const std::uint16_t*>(
                    source + static_cast<std::size_t>(y) * texture_view->pitch);
                for (std::uint32_t x = 0; x < texture_view->width; ++x)
                {
                    const std::uint16_t pixel = row[x];
                    const std::size_t offset =
                        (static_cast<std::size_t>(y) * texture_view->width + x) * 4;
                    rgba[offset] = static_cast<std::uint8_t>(((pixel >> 11) & 0x1f) * 255 / 31);
                    rgba[offset + 1] =
                        static_cast<std::uint8_t>(((pixel >> 5) & 0x3f) * 255 / 63);
                    rgba[offset + 2] = static_cast<std::uint8_t>((pixel & 0x1f) * 255 / 31);
                    rgba[offset + 3] = graphics::IsRgb565ColorKeyMatch(pixel, effective_key)
                                           ? 0
                                           : 255;
                }
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            if (cached.width == texture_view->width && cached.height == texture_view->height)
            {
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                static_cast<GLsizei>(texture_view->width),
                                static_cast<GLsizei>(texture_view->height),
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                rgba.data());
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA,
                             static_cast<GLsizei>(texture_view->width),
                             static_cast<GLsizei>(texture_view->height),
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             rgba.data());
            }
            cached.width = texture_view->width;
            cached.height = texture_view->height;
            cached.revision = texture_view->revision;
            cached.color_key = effective_key;
        }
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,
                        state.minification_filter == graphics::TextureFilter::kLinear ? GL_LINEAR
                                                                                      : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MAG_FILTER,
                        state.magnification_filter == graphics::TextureFilter::kLinear ? GL_LINEAR
                                                                                       : GL_NEAREST);
    }

    if (state.alpha_blend_enabled)
    {
        const auto to_gl_blend = [](graphics::BlendFactor factor) {
            switch (factor)
            {
                case graphics::BlendFactor::kZero:
                    return GL_ZERO;
                case graphics::BlendFactor::kOne:
                    return GL_ONE;
                case graphics::BlendFactor::kSourceColor:
                    return GL_SRC_COLOR;
                case graphics::BlendFactor::kSourceAlpha:
                    return GL_SRC_ALPHA;
            }
            return GL_ONE;
        };
        glEnable(GL_BLEND);
        glBlendFunc(to_gl_blend(state.source_blend), to_gl_blend(state.destination_blend));
    }
    else
    {
        glDisable(GL_BLEND);
    }

    const GLsizei stride = sizeof(GlVertex);
    impl_->enable_vertex_attrib_array(0);
    impl_->enable_vertex_attrib_array(1);
    impl_->enable_vertex_attrib_array(2);
    impl_->vertex_attrib_pointer(0, 4, GL_FLOAT, GL_FALSE, stride, &vertices[0].position);
    impl_->vertex_attrib_pointer(1, 4, GL_FLOAT, GL_FALSE, stride, &vertices[0].color);
    impl_->vertex_attrib_pointer(2, 2, GL_FLOAT, GL_FALSE, stride, &vertices[0].texture);
    const GLenum primitive_mode = command.topology == graphics::PrimitiveTopology::kLineList
                                      ? GL_LINES
                                      : GL_TRIANGLE_STRIP;
    glDrawArrays(primitive_mode, 0, static_cast<GLsizei>(vertices.size()));
    impl_->disable_vertex_attrib_array(2);
    impl_->disable_vertex_attrib_array(1);
    impl_->disable_vertex_attrib_array(0);
    impl_->use_program(0);
    if (glGetError() != GL_NO_ERROR)
    {
        *error = "OpenGL rejected the Direct3D3 draw command";
        return false;
    }
    error->clear();
    return true;
}

void Direct3d3OpenGlBackend::DiscardTexture(std::uint64_t identity)
{
    if (impl_ == nullptr || identity == 0)
    {
        return;
    }
    const auto found = impl_->textures.find(identity);
    if (found == impl_->textures.end())
    {
        return;
    }
    std::string error;
    if (impl_->MakeCurrent(&error))
    {
        glDeleteTextures(1, &found->second.name);
        impl_->textures.erase(found);
    }
}

bool Direct3d3OpenGlBackend::Present(std::string* error)
{
    if (impl_ == nullptr || error == nullptr || !impl_->MakeCurrent(error))
    {
        return false;
    }
    if (SwapBuffers(impl_->dc) == FALSE)
    {
        *error = "cannot swap the Direct3D3 OpenGL buffers";
        return false;
    }
    impl_->frame_started = false;
    error->clear();
    return true;
}

}  // namespace re2dj::platform::windows
