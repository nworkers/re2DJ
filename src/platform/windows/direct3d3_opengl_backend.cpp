#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
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
using Uniform2fFunction = void(APIENTRY*)(GLint, GLfloat, GLfloat);
using Uniform3fFunction = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
using EnableVertexAttribArrayFunction = void(APIENTRY*)(GLuint);
using DisableVertexAttribArrayFunction = void(APIENTRY*)(GLuint);
using VertexAttribPointerFunction =
    void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

constexpr GLenum kUnsignedShort565 = 0x8363;
constexpr GLenum kVertexShader = 0x8b31;
constexpr GLenum kFragmentShader = 0x8b30;
constexpr GLenum kCompileStatus = 0x8b81;
constexpr GLenum kLinkStatus = 0x8b82;

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

std::array<float, 3> Rgb565ToFloats(std::uint16_t color)
{
    return {static_cast<float>((color >> 11) & 0x1f) / 31.0f,
            static_cast<float>((color >> 5) & 0x3f) / 63.0f,
            static_cast<float>(color & 0x1f) / 31.0f};
}

}  // namespace

struct Direct3d3OpenGlBackend::Impl
{
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC context = nullptr;
    GLuint program = 0;
    GLuint texture = 0;
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
    Uniform2fFunction uniform_2f = nullptr;
    Uniform3fFunction uniform_3f = nullptr;
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
            "uniform vec3 u_color_key;\n"
            "varying vec4 v_color;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  vec4 texel = u_texture_enabled != 0 ? texture2D(u_texture, v_texture) : vec4(1.0);\n"
            "  if (u_color_key_enabled != 0 &&\n"
            "      all(lessThanEqual(abs(texel.rgb - u_color_key), vec3(0.017)))) discard;\n"
            "  gl_FragColor = texel * v_color;\n"
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
        if (impl_->texture != 0)
        {
            glDeleteTextures(1, &impl_->texture);
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
        LoadGlFunction("glUniform2f", &impl->uniform_2f) &&
        LoadGlFunction("glUniform3f", &impl->uniform_3f) &&
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
    glGenTextures(1, &impl->texture);
    if (impl->texture == 0)
    {
        *error = "cannot create the Direct3D3 OpenGL texture";
        return false;
    }
    error->clear();
    return true;
}

bool Direct3d3OpenGlBackend::Draw(const graphics::LegacyDrawCommand& command,
                                  std::uint32_t logical_width,
                                  std::uint32_t logical_height,
                                  const Rgb565TextureView* texture_view,
                                  std::string* error)
{
    if (impl_ == nullptr || error == nullptr || logical_width == 0 || logical_height == 0 ||
        command.topology != graphics::PrimitiveTopology::kTriangleStrip ||
        command.vertices.size() < 3 || !impl_->MakeCurrent(error))
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
        output.position[3] = input.reciprocal_w;
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
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_texture"), 0);
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_texture_enabled"),
                     has_texture ? 1 : 0);
    impl_->uniform_1i(impl_->get_uniform_location(impl_->program, "u_color_key_enabled"),
                     has_texture && texture_view->has_source_color_key ? 1 : 0);
    if (has_texture && texture_view->has_source_color_key)
    {
        const std::array<float, 3> key = Rgb565ToFloats(texture_view->source_color_key);
        impl_->uniform_3f(impl_->get_uniform_location(impl_->program, "u_color_key"),
                         key[0], key[1], key[2]);
    }
    if (has_texture)
    {
        glBindTexture(GL_TEXTURE_2D, impl_->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,
                      static_cast<GLint>(texture_view->pitch / sizeof(std::uint16_t)));
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGB,
                     static_cast<GLsizei>(texture_view->width),
                     static_cast<GLsizei>(texture_view->height),
                     0,
                     GL_RGB,
                     kUnsignedShort565,
                     texture_view->pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    const GLsizei stride = sizeof(GlVertex);
    impl_->enable_vertex_attrib_array(0);
    impl_->enable_vertex_attrib_array(1);
    impl_->enable_vertex_attrib_array(2);
    impl_->vertex_attrib_pointer(0, 4, GL_FLOAT, GL_FALSE, stride, &vertices[0].position);
    impl_->vertex_attrib_pointer(1, 4, GL_FLOAT, GL_FALSE, stride, &vertices[0].color);
    impl_->vertex_attrib_pointer(2, 2, GL_FLOAT, GL_FALSE, stride, &vertices[0].texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertices.size()));
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
