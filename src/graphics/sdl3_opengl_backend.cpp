#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_EMSCRIPTEN)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "re2dj/graphics/presentation_filter.h"
#include "re2dj/graphics/sdl3_opengl_backend.h"

namespace re2dj::graphics
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
using VertexAttribPointerFunction = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                                                    const void*);
using DeleteTexturesFunction = void(APIENTRY*)(GLsizei, const GLuint*);
using ViewportFunction = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
using DisableFunction = void(APIENTRY*)(GLenum);
using DepthFuncFunction = void(APIENTRY*)(GLenum);
using DepthMaskFunction = void(APIENTRY*)(GLboolean);
using ClearColorFunction = void(APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
using ClearFunction = void(APIENTRY*)(GLbitfield);
using GenTexturesFunction = void(APIENTRY*)(GLsizei, GLuint*);
using BindTextureFunction = void(APIENTRY*)(GLenum, GLuint);
using TexParameteriFunction = void(APIENTRY*)(GLenum, GLenum, GLint);
using PixelStoreiFunction = void(APIENTRY*)(GLenum, GLint);
using TexSubImage2dFunction = void(APIENTRY*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum,
                                              GLenum, const void*);
using TexImage2dFunction = void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
                                           GLenum, const void*);
using EnableFunction = void(APIENTRY*)(GLenum);
using CullFaceFunction = void(APIENTRY*)(GLenum);
using FrontFaceFunction = void(APIENTRY*)(GLenum);
using BlendFuncFunction = void(APIENTRY*)(GLenum, GLenum);
using DrawArraysFunction = void(APIENTRY*)(GLenum, GLint, GLsizei);
using GetErrorFunction = GLenum(APIENTRY*)();
using GenFramebuffersFunction = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteFramebuffersFunction = void(APIENTRY*)(GLsizei, const GLuint*);
using BindFramebufferFunction = void(APIENTRY*)(GLenum, GLuint);
using CheckFramebufferStatusFunction = GLenum(APIENTRY*)(GLenum);
using FramebufferTexture2dFunction = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using GenRenderbuffersFunction = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteRenderbuffersFunction = void(APIENTRY*)(GLsizei, const GLuint*);
using BindRenderbufferFunction = void(APIENTRY*)(GLenum, GLuint);
using RenderbufferStorageFunction = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
using FramebufferRenderbufferFunction = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);

constexpr GLenum kVertexShader = 0x8b31;
constexpr GLenum kFragmentShader = 0x8b30;
constexpr GLenum kCompileStatus = 0x8b81;
constexpr GLenum kLinkStatus = 0x8b82;
constexpr GLenum kClampToEdge = 0x812f;
constexpr GLenum kFramebuffer = 0x8d40;
constexpr GLenum kRenderbuffer = 0x8d41;
constexpr GLenum kColorAttachment0 = 0x8ce0;
constexpr GLenum kDepthAttachment = 0x8d00;
constexpr GLenum kFramebufferComplete = 0x8cd5;
constexpr GLenum kDepthComponent16 = 0x81a5;
constexpr GLenum kRgb565 = 0x8d62;

template <typename Function>
bool LoadGlFunction(const char* name, Function* function)
{
    *function = reinterpret_cast<Function>(SDL_GL_GetProcAddress(name));
    return *function != nullptr;
}

template <typename Function>
bool LoadGlFunction(const char* name, const char* extension_name, Function* function)
{
    if (LoadGlFunction(name, function))
    {
        return true;
    }
    return LoadGlFunction(extension_name, function);
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
            static_cast<float>((argb >> 8) & 0xff) * scale, static_cast<float>(argb & 0xff) * scale,
            static_cast<float>((argb >> 24) & 0xff) * scale};
}

}  // namespace

struct Sdl3OpenGlBackend::Impl
{
    struct CachedTexture
    {
        GLuint name = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t revision = 0;
        Rgb565ColorKey color_key;
        // Last values actually handed to glTexParameteri for this texture, so a
        // draw that keeps the same sampler state issues no parameter calls.
        GLenum minification_filter = 0;
        GLenum magnification_filter = 0;
        GLenum address_u = 0;
        GLenum address_v = 0;
    };

    // Uniform locations are fixed once the program links, so they are resolved
    // there instead of being looked up by name on every draw.
    struct UniformLocations
    {
        GLint viewport = -1;
        GLint texture = -1;
        GLint texture_enabled = -1;
        GLint color_key_enabled = -1;
        GLint alpha_test_enabled = -1;
        GLint alpha_reference = -1;
    };

    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool owns_video_subsystem = false;
    GLuint program = 0;
    UniformLocations uniforms;
    GLuint render_framebuffer = 0;
    GLuint render_color_texture = 0;
    GLuint render_depth_renderbuffer = 0;
    // Set when the guest presents by flipping. The colour buffer then carries
    // over from one frame to the next instead of being cleared. One buffer is
    // kept rather than one per buffer in the guest's chain: rotating two would
    // reproduce the age of a real back buffer, but a guest that redraws only
    // part of the screen then alternates between two diverging images, which
    // reads as a 30 Hz flicker the original does not have.
    bool retain_between_frames = false;
    // What SDL reported after the present-sync policy was applied. Reported to
    // the host rather than logged here, since this layer has no log.
    int applied_swap_interval = 0;
    std::unordered_map<std::uint64_t, CachedTexture> textures;
    bool frame_started = false;
    std::uint32_t logical_width = 0;
    std::uint32_t logical_height = 0;
    // This backend is the only OpenGL consumer in the process, so once the
    // context is current it stays current and the per-draw call is redundant.
    bool context_current = false;
    // Reused across draws so building the vertex payload allocates nothing.
    std::vector<GlVertex> vertex_scratch;
    bool draw_diagnostics = false;
    std::uint64_t draws_error_checked = 0;

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
    DeleteTexturesFunction delete_textures = nullptr;
    ViewportFunction viewport = nullptr;
    DisableFunction disable = nullptr;
    DepthFuncFunction depth_func = nullptr;
    DepthMaskFunction depth_mask = nullptr;
    ClearColorFunction clear_color = nullptr;
    ClearFunction clear = nullptr;
    GenTexturesFunction gen_textures = nullptr;
    BindTextureFunction bind_texture = nullptr;
    TexParameteriFunction tex_parameter_i = nullptr;
    PixelStoreiFunction pixel_store_i = nullptr;
    TexSubImage2dFunction tex_sub_image_2d = nullptr;
    TexImage2dFunction tex_image_2d = nullptr;
    EnableFunction enable = nullptr;
    CullFaceFunction cull_face = nullptr;
    FrontFaceFunction front_face = nullptr;
    BlendFuncFunction blend_func = nullptr;
    DrawArraysFunction draw_arrays = nullptr;
    GetErrorFunction get_error = nullptr;
    GenFramebuffersFunction gen_framebuffers = nullptr;
    DeleteFramebuffersFunction delete_framebuffers = nullptr;
    BindFramebufferFunction bind_framebuffer = nullptr;
    CheckFramebufferStatusFunction check_framebuffer_status = nullptr;
    FramebufferTexture2dFunction framebuffer_texture_2d = nullptr;
    GenRenderbuffersFunction gen_renderbuffers = nullptr;
    DeleteRenderbuffersFunction delete_renderbuffers = nullptr;
    BindRenderbufferFunction bind_renderbuffer = nullptr;
    RenderbufferStorageFunction renderbuffer_storage = nullptr;
    FramebufferRenderbufferFunction framebuffer_renderbuffer = nullptr;

    bool MakeCurrent(std::string* error)
    {
        if (context_current)
        {
            return true;
        }
        if (!SDL_GL_MakeCurrent(window, context))
        {
            *error = std::string("cannot make the SDL3 OpenGL context current: ") + SDL_GetError();
            return false;
        }
        context_current = true;
        return true;
    }

    // Per-draw error checking is a pipeline synchronization point in some
    // drivers. It stays on for the first draws, which is where a broken state
    // setup shows up, and afterwards only while diagnostics are enabled.
    bool ShouldCheckDrawError()
    {
        constexpr std::uint64_t kInitialCheckedDraws = 256;
        if (draw_diagnostics)
        {
            return true;
        }
        if (draws_error_checked >= kInitialCheckedDraws)
        {
            return false;
        }
        ++draws_error_checked;
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
        get_shader_info_log(shader, static_cast<GLsizei>(message.size() - 1), &length,
                            message.data());
        delete_shader(shader);
        *error = std::string("OpenGL shader compilation failed: ") + message.data();
        return 0;
    }

    void DestroyRenderTarget()
    {
        if (render_depth_renderbuffer != 0 && delete_renderbuffers != nullptr)
        {
            delete_renderbuffers(1, &render_depth_renderbuffer);
        }
        if (render_color_texture != 0 && delete_textures != nullptr)
        {
            delete_textures(1, &render_color_texture);
        }
        if (render_framebuffer != 0 && delete_framebuffers != nullptr)
        {
            delete_framebuffers(1, &render_framebuffer);
        }
        render_depth_renderbuffer = 0;
        render_color_texture = 0;
        render_framebuffer = 0;
    }

    bool CreateRenderTarget(std::uint32_t width, std::uint32_t height, std::string* error)
    {
        logical_width = width;
        logical_height = height;
        gen_textures(1, &render_color_texture);
        if (render_color_texture == 0)
        {
            *error = "cannot create RGB565 OpenGL render-target texture";
            return false;
        }
        bind_texture(GL_TEXTURE_2D, render_color_texture);
        tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClampToEdge);
        tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClampToEdge);
        tex_image_2d(GL_TEXTURE_2D,
                     0,
                     static_cast<GLint>(kRgb565),
                     static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height),
                     0,
                     GL_RGB,
                     GL_UNSIGNED_SHORT_5_6_5,
                     nullptr);

        gen_framebuffers(1, &render_framebuffer);
        gen_renderbuffers(1, &render_depth_renderbuffer);
        if (render_framebuffer == 0 || render_depth_renderbuffer == 0)
        {
            DestroyRenderTarget();
            *error = "cannot create OpenGL render-target framebuffer attachments";
            return false;
        }
        bind_framebuffer(kFramebuffer, render_framebuffer);
        framebuffer_texture_2d(
            kFramebuffer, kColorAttachment0, GL_TEXTURE_2D, render_color_texture, 0);
        bind_renderbuffer(kRenderbuffer, render_depth_renderbuffer);
        renderbuffer_storage(kRenderbuffer,
                             kDepthComponent16,
                             static_cast<GLsizei>(width),
                             static_cast<GLsizei>(height));
        framebuffer_renderbuffer(
            kFramebuffer, kDepthAttachment, kRenderbuffer, render_depth_renderbuffer);
        const GLenum status = check_framebuffer_status(kFramebuffer);
        if (status == kFramebufferComplete)
        {
            // A retained target is never cleared implicitly again, so the
            // undefined contents glTexImage2D leaves behind have to go now.
            viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
            depth_mask(GL_TRUE);
            clear_color(0.0f, 0.0f, 0.0f, 1.0f);
            clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        bind_framebuffer(kFramebuffer, 0);
        if (status != kFramebufferComplete)
        {
            DestroyRenderTarget();
            *error = "OpenGL RGB565 render-target framebuffer is incomplete";
            return false;
        }
        return true;
    }

    bool CreateProgram(std::string* error)
    {
#if defined(SDL_PLATFORM_EMSCRIPTEN)
        constexpr char shader_version[] = "#version 100\n";
        constexpr char fragment_precision[] = "precision mediump float;\n";
#else
        constexpr char shader_version[] = "#version 120\n";
        constexpr char fragment_precision[] = "";
#endif
        constexpr char vertex_source[] =
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
            "  gl_Position = vec4(ndc * clip_w, (a_position.z * 2.0 - 1.0) * "
            "clip_w, clip_w);\n"
            "  v_color = a_color;\n"
            "  v_texture = a_texture;\n"
            "}\n";
        constexpr char fragment_source[] =
            "uniform sampler2D u_texture;\n"
            "uniform int u_texture_enabled;\n"
            "uniform int u_color_key_enabled;\n"
            "uniform int u_alpha_test_enabled;\n"
            "uniform float u_alpha_reference;\n"
            "varying vec4 v_color;\n"
            "varying vec2 v_texture;\n"
            "void main() {\n"
            "  vec4 texel = u_texture_enabled != 0 ? texture2D(u_texture, "
            "v_texture) : vec4(1.0);\n"
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

        const std::string complete_vertex_source = std::string(shader_version) + vertex_source;
        const std::string complete_fragment_source =
            std::string(shader_version) + fragment_precision + fragment_source;
        const GLuint vertex_shader = Compile(kVertexShader, complete_vertex_source.c_str(), error);
        if (vertex_shader == 0)
        {
            return false;
        }
        const GLuint fragment_shader =
            Compile(kFragmentShader, complete_fragment_source.c_str(), error);
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
            uniforms.viewport = get_uniform_location(program, "u_viewport");
            uniforms.texture = get_uniform_location(program, "u_texture");
            uniforms.texture_enabled = get_uniform_location(program, "u_texture_enabled");
            uniforms.color_key_enabled = get_uniform_location(program, "u_color_key_enabled");
            uniforms.alpha_test_enabled = get_uniform_location(program, "u_alpha_test_enabled");
            uniforms.alpha_reference = get_uniform_location(program, "u_alpha_reference");
            return true;
        }
        std::array<char, 512> message = {};
        GLsizei length = 0;
        get_program_info_log(program, static_cast<GLsizei>(message.size() - 1), &length,
                             message.data());
        delete_program(program);
        program = 0;
        *error = std::string("OpenGL program link failed: ") + message.data();
        return false;
    }
};

Sdl3OpenGlBackend::Sdl3OpenGlBackend() = default;

Sdl3OpenGlBackend::~Sdl3OpenGlBackend()
{
    if (impl_ == nullptr)
    {
        return;
    }
    if (impl_->context != nullptr && impl_->window != nullptr &&
        SDL_GL_MakeCurrent(impl_->window, impl_->context))
    {
        impl_->DestroyRenderTarget();
        for (const auto& [identity, texture] : impl_->textures)
        {
            (void)identity;
            if (impl_->delete_textures != nullptr)
            {
                impl_->delete_textures(1, &texture.name);
            }
        }
        if (impl_->program != 0 && impl_->delete_program != nullptr)
        {
            impl_->delete_program(impl_->program);
        }
        SDL_GL_MakeCurrent(impl_->window, nullptr);
    }
    if (impl_->context != nullptr)
    {
        SDL_GL_DestroyContext(impl_->context);
    }
    if (impl_->window != nullptr)
    {
        SDL_DestroyWindow(impl_->window);
    }
    if (impl_->owns_video_subsystem)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    delete impl_;
}

namespace
{

// Requests the policy's swap interval and reports what the driver granted.
//
// Failing to set an interval is deliberately not an initialization failure:
// the context still draws, and a host that cannot get the interval it asked
// for is better off running at the driver's than not running at all. The
// caller reads back the applied value rather than assuming the request took.
void ApplyPresentSync(PresentSync present_sync, int* applied_interval)
{
    switch (present_sync)
    {
    case PresentSync::kImmediate:
        SDL_GL_SetSwapInterval(0);
        break;
    case PresentSync::kAdaptive:
        // Late-swap tearing is commonly refused; vertical sync is the
        // conservative fallback because it is what the policy degrades to.
        if (!SDL_GL_SetSwapInterval(-1))
        {
            SDL_GL_SetSwapInterval(1);
        }
        break;
    case PresentSync::kVerticalSync:
    default:
        SDL_GL_SetSwapInterval(1);
        break;
    }
    int interval = 0;
    if (!SDL_GL_GetSwapInterval(&interval))
    {
        interval = 0;
    }
    *applied_interval = interval;
}

}  // namespace

int Sdl3OpenGlBackend::applied_swap_interval() const
{
    return impl_ == nullptr ? 0 : impl_->applied_swap_interval;
}

bool Sdl3OpenGlBackend::Initialize(const Sdl3OpenGlWindowConfig& config, std::string* error)
{
    if (impl_ != nullptr || error == nullptr || config.width == 0 || config.height == 0 ||
        config.title == nullptr)
    {
        return false;
    }
    auto* const impl = new (std::nothrow) Impl;
    if (impl == nullptr)
    {
        *error = "cannot allocate the SDL3 OpenGL backend";
        return false;
    }
    impl_ = impl;
    impl->owns_video_subsystem = (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0;
    if (impl->owns_video_subsystem && !SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        *error = std::string("cannot initialize SDL3 video: ") + SDL_GetError();
        return false;
    }

    SDL_GL_ResetAttributes();
#if defined(SDL_PLATFORM_EMSCRIPTEN)
    const bool attributes_set =
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) &&
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16) &&
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#else
    const bool attributes_set =
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1) &&
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16) &&
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif
    if (!attributes_set)
    {
        *error = std::string("cannot configure the SDL3 OpenGL context: ") + SDL_GetError();
        return false;
    }

    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties == 0)
    {
        *error = std::string("cannot allocate SDL3 window properties: ") + SDL_GetError();
        return false;
    }
    bool properties_set =
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, config.title) &&
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
                              static_cast<Sint64>(config.width)) &&
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                              static_cast<Sint64>(config.height)) &&
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    if (config.native_window != nullptr)
    {
#if defined(SDL_PLATFORM_WINDOWS)
        properties_set =
            properties_set &&
            SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER,
                                   config.native_window);
#else
        properties_set = false;
        SDL_SetError("native window wrapping is only implemented for Win32");
#endif
    }
    if (properties_set)
    {
        impl->window = SDL_CreateWindowWithProperties(properties);
    }
    SDL_DestroyProperties(properties);
    if (!properties_set || impl->window == nullptr)
    {
        *error = std::string("cannot create the SDL3 OpenGL window: ") + SDL_GetError();
        return false;
    }

    impl->context = SDL_GL_CreateContext(impl->window);
    if (impl->context == nullptr || !impl->MakeCurrent(error))
    {
        return false;
    }

    ApplyPresentSync(config.present_sync, &impl->applied_swap_interval);

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
        LoadGlFunction("glVertexAttribPointer", &impl->vertex_attrib_pointer) &&
        LoadGlFunction("glDeleteTextures", &impl->delete_textures) &&
        LoadGlFunction("glViewport", &impl->viewport) &&
        LoadGlFunction("glDisable", &impl->disable) &&
        LoadGlFunction("glDepthFunc", &impl->depth_func) &&
        LoadGlFunction("glDepthMask", &impl->depth_mask) &&
        LoadGlFunction("glClearColor", &impl->clear_color) &&
        LoadGlFunction("glClear", &impl->clear) &&
        LoadGlFunction("glGenTextures", &impl->gen_textures) &&
        LoadGlFunction("glBindTexture", &impl->bind_texture) &&
        LoadGlFunction("glTexParameteri", &impl->tex_parameter_i) &&
        LoadGlFunction("glPixelStorei", &impl->pixel_store_i) &&
        LoadGlFunction("glTexSubImage2D", &impl->tex_sub_image_2d) &&
        LoadGlFunction("glTexImage2D", &impl->tex_image_2d) &&
        LoadGlFunction("glEnable", &impl->enable) &&
        LoadGlFunction("glCullFace", &impl->cull_face) &&
        LoadGlFunction("glFrontFace", &impl->front_face) &&
        LoadGlFunction("glBlendFunc", &impl->blend_func) &&
        LoadGlFunction("glDrawArrays", &impl->draw_arrays) &&
        LoadGlFunction("glGetError", &impl->get_error) &&
        LoadGlFunction("glGenFramebuffers", "glGenFramebuffersEXT", &impl->gen_framebuffers) &&
        LoadGlFunction("glDeleteFramebuffers",
                       "glDeleteFramebuffersEXT",
                       &impl->delete_framebuffers) &&
        LoadGlFunction("glBindFramebuffer", "glBindFramebufferEXT", &impl->bind_framebuffer) &&
        LoadGlFunction("glCheckFramebufferStatus",
                       "glCheckFramebufferStatusEXT",
                       &impl->check_framebuffer_status) &&
        LoadGlFunction("glFramebufferTexture2D",
                       "glFramebufferTexture2DEXT",
                       &impl->framebuffer_texture_2d) &&
        LoadGlFunction("glGenRenderbuffers", "glGenRenderbuffersEXT", &impl->gen_renderbuffers) &&
        LoadGlFunction("glDeleteRenderbuffers",
                       "glDeleteRenderbuffersEXT",
                       &impl->delete_renderbuffers) &&
        LoadGlFunction("glBindRenderbuffer", "glBindRenderbufferEXT", &impl->bind_renderbuffer) &&
        LoadGlFunction("glRenderbufferStorage",
                       "glRenderbufferStorageEXT",
                       &impl->renderbuffer_storage) &&
        LoadGlFunction("glFramebufferRenderbuffer",
                       "glFramebufferRenderbufferEXT",
                       &impl->framebuffer_renderbuffer);
    if (!loaded)
    {
        *error = "required OpenGL shader entry points are unavailable";
        return false;
    }
    if (!impl->CreateProgram(error))
    {
        return false;
    }
    if (!impl->CreateRenderTarget(config.width, config.height, error))
    {
        return false;
    }
    impl->retain_between_frames = config.retain_between_frames;
    // Every draw in this backend uses the same three attributes with the same
    // layout, and there is no vertex array object in this profile, so the
    // enable state is global and only has to be established once.
    impl->enable_vertex_attrib_array(0);
    impl->enable_vertex_attrib_array(1);
    impl->enable_vertex_attrib_array(2);
    impl->draw_diagnostics = config.draw_diagnostics;
    error->clear();
    return true;
}

bool Sdl3OpenGlBackend::Draw(const LegacyDrawCommand& command,
                             const LegacyFixedFunctionState& state, std::uint32_t logical_width,
                             std::uint32_t logical_height, const LegacyTextureView* texture_view,
                             std::string* error)
{
    if (impl_ == nullptr || error == nullptr || logical_width == 0 || logical_height == 0 ||
        (command.topology == PrimitiveTopology::kTriangleStrip && command.vertices.size() < 3) ||
        (command.topology == PrimitiveTopology::kTriangleList &&
         (command.vertices.size() < 3 || command.vertices.size() % 3 != 0)) ||
        (command.topology == PrimitiveTopology::kLineList &&
         (command.vertices.size() < 2 || command.vertices.size() % 2 != 0)) ||
        !impl_->MakeCurrent(error))
    {
        return false;
    }
    if (logical_width != impl_->logical_width || logical_height != impl_->logical_height ||
        impl_->render_framebuffer == 0 || impl_->render_color_texture == 0)
    {
        *error = "logical draw size does not match the OpenGL RGB565 render target";
        return false;
    }
    impl_->bind_framebuffer(kFramebuffer, impl_->render_framebuffer);
    if (!impl_->frame_started)
    {
        impl_->viewport(0,
                        0,
                        static_cast<GLsizei>(impl_->logical_width),
                        static_cast<GLsizei>(impl_->logical_height));
        impl_->depth_mask(GL_TRUE);
        // Depth is scratch that nothing reads across frames, so it always goes.
        // Colour stays when the guest presents by flipping: it keeps drawing
        // into buffers it owns, and a frame in which it redraws only part of
        // the screen needs the rest still there.
        GLbitfield mask = GL_DEPTH_BUFFER_BIT;
        if (!impl_->retain_between_frames)
        {
            impl_->clear_color(0.0f, 0.0f, 0.0f, 1.0f);
            mask |= GL_COLOR_BUFFER_BIT;
        }
        impl_->clear(mask);
        impl_->frame_started = true;
    }

    std::vector<GlVertex>& vertices = impl_->vertex_scratch;
    vertices.clear();
    vertices.reserve(command.vertices.size());
    for (const TransformedLitVertex& input : command.vertices)
    {
        GlVertex output = {};
        output.position[0] = input.x;
        output.position[1] = input.y;
        output.position[2] = ResolveLegacyClipDepth(state, input.z);
        output.position[3] =
            command.topology == PrimitiveTopology::kLineList && input.reciprocal_w == 0.0f
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
    impl_->uniform_2f(impl_->uniforms.viewport,
                      static_cast<float>(logical_width), static_cast<float>(logical_height));
    const bool has_texture = texture_view != nullptr && texture_view->pixels != nullptr &&
                             texture_view->width != 0 && texture_view->height != 0;
    if (has_texture && (texture_view->pitch < static_cast<std::uint64_t>(texture_view->width) *
                                                  sizeof(std::uint16_t) ||
                        texture_view->pitch % sizeof(std::uint16_t) != 0))
    {
        *error = "invalid RGB565 texture row pitch";
        return false;
    }
    const bool color_key_active =
        has_texture && state.color_key_enabled && texture_view->source_color_key.enabled;
    const auto to_gl_address = [](TextureAddressMode mode) -> GLenum {
        switch (mode)
        {
        case TextureAddressMode::kWrap:
            return GL_REPEAT;
        case TextureAddressMode::kMirror:
            return GL_MIRRORED_REPEAT;
        case TextureAddressMode::kClamp:
            return kClampToEdge;
        }
        return kClampToEdge;
    };
    impl_->uniform_1i(impl_->uniforms.texture, 0);
    impl_->uniform_1i(impl_->uniforms.texture_enabled, has_texture ? 1 : 0);
    impl_->uniform_1i(impl_->uniforms.color_key_enabled, color_key_active ? 1 : 0);
    impl_->uniform_1i(impl_->uniforms.alpha_test_enabled, state.alpha_test_enabled ? 1 : 0);
    if (impl_->uniforms.alpha_reference >= 0)
    {
        impl_->uniform_1f(impl_->uniforms.alpha_reference,
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
            impl_->gen_textures(1, &cached.name);
            if (cached.name == 0)
            {
                impl_->textures.erase(texture_view->identity);
                *error = "cannot create a cached Direct3D3 OpenGL texture";
                return false;
            }
            impl_->bind_texture(GL_TEXTURE_2D, cached.name);
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            cached.minification_filter = GL_NEAREST;
            cached.magnification_filter = GL_NEAREST;
            cached.address_u = GL_REPEAT;
            cached.address_v = GL_REPEAT;
        }
        else
        {
            impl_->bind_texture(GL_TEXTURE_2D, cached.name);
        }
        // Sampler state lives on the texture object, so the values already
        // applied to this texture do not have to be re-sent every draw.
        const GLenum address_u = to_gl_address(state.address_u);
        if (cached.address_u != address_u)
        {
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_u);
            cached.address_u = address_u;
        }
        const GLenum address_v = to_gl_address(state.address_v);
        if (cached.address_v != address_v)
        {
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_v);
            cached.address_v = address_v;
        }
        Rgb565ColorKey effective_key = texture_view->source_color_key;
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
                    rgba[offset + 1] = static_cast<std::uint8_t>(((pixel >> 5) & 0x3f) * 255 / 63);
                    rgba[offset + 2] = static_cast<std::uint8_t>((pixel & 0x1f) * 255 / 31);
                    rgba[offset + 3] = IsRgb565ColorKeyMatch(pixel, effective_key) ? 0 : 255;
                }
            }
            impl_->pixel_store_i(GL_UNPACK_ALIGNMENT, 4);
            if (cached.width == texture_view->width && cached.height == texture_view->height)
            {
                impl_->tex_sub_image_2d(GL_TEXTURE_2D, 0, 0, 0,
                                        static_cast<GLsizei>(texture_view->width),
                                        static_cast<GLsizei>(texture_view->height), GL_RGBA,
                                        GL_UNSIGNED_BYTE, rgba.data());
            }
            else
            {
                impl_->tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA,
                                    static_cast<GLsizei>(texture_view->width),
                                    static_cast<GLsizei>(texture_view->height), 0, GL_RGBA,
                                    GL_UNSIGNED_BYTE, rgba.data());
            }
            cached.width = texture_view->width;
            cached.height = texture_view->height;
            cached.revision = texture_view->revision;
            cached.color_key = effective_key;
        }
        const GLenum minification_filter =
            state.minification_filter == TextureFilter::kLinear ? GL_LINEAR : GL_NEAREST;
        if (cached.minification_filter != minification_filter)
        {
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minification_filter);
            cached.minification_filter = minification_filter;
        }
        const GLenum magnification_filter =
            state.magnification_filter == TextureFilter::kLinear ? GL_LINEAR : GL_NEAREST;
        if (cached.magnification_filter != magnification_filter)
        {
            impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magnification_filter);
            cached.magnification_filter = magnification_filter;
        }
    }

    if (state.depth_test_enabled)
    {
        const auto to_gl_compare = [](CompareFunction function) {
            switch (function)
            {
            case CompareFunction::kNever:
                return GL_NEVER;
            case CompareFunction::kLess:
                return GL_LESS;
            case CompareFunction::kEqual:
                return GL_EQUAL;
            case CompareFunction::kLessEqual:
                return GL_LEQUAL;
            case CompareFunction::kGreater:
                return GL_GREATER;
            case CompareFunction::kNotEqual:
                return GL_NOTEQUAL;
            case CompareFunction::kGreaterEqual:
                return GL_GEQUAL;
            case CompareFunction::kAlways:
                return GL_ALWAYS;
            }
            return GL_LEQUAL;
        };
        impl_->enable(GL_DEPTH_TEST);
        impl_->depth_func(to_gl_compare(state.depth_function));
    }
    else
    {
        impl_->disable(GL_DEPTH_TEST);
    }
    impl_->depth_mask(state.depth_write_enabled ? GL_TRUE : GL_FALSE);

    if (command.topology == PrimitiveTopology::kLineList ||
        state.cull_mode == CullMode::kNone)
    {
        impl_->disable(GL_CULL_FACE);
    }
    else
    {
        // The vertex shader converts guest top-left screen Y into OpenGL clip
        // coordinates, so the preserved winding maps to the opposite API
        // winding.
        impl_->enable(GL_CULL_FACE);
        impl_->cull_face(GL_BACK);
        impl_->front_face(state.cull_mode == CullMode::kClockwise ? GL_CCW : GL_CW);
    }

    if (state.alpha_blend_enabled)
    {
        const auto to_gl_blend = [](BlendFactor factor) {
            switch (factor)
            {
            case BlendFactor::kZero:
                return GL_ZERO;
            case BlendFactor::kOne:
                return GL_ONE;
            case BlendFactor::kSourceColor:
                return GL_SRC_COLOR;
            case BlendFactor::kInverseSourceColor:
                return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::kSourceAlpha:
                return GL_SRC_ALPHA;
            case BlendFactor::kInverseSourceAlpha:
                return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::kDestinationAlpha:
                return GL_DST_ALPHA;
            case BlendFactor::kInverseDestinationAlpha:
                return GL_ONE_MINUS_DST_ALPHA;
            case BlendFactor::kDestinationColor:
                return GL_DST_COLOR;
            case BlendFactor::kInverseDestinationColor:
                return GL_ONE_MINUS_DST_COLOR;
            }
            return GL_ONE;
        };
        impl_->enable(GL_BLEND);
        impl_->blend_func(to_gl_blend(state.source_blend), to_gl_blend(state.destination_blend));
    }
    else
    {
        impl_->disable(GL_BLEND);
    }

    const GLsizei stride = sizeof(GlVertex);
    impl_->vertex_attrib_pointer(0, 4, GL_FLOAT, GL_FALSE, stride, &vertices[0].position);
    impl_->vertex_attrib_pointer(1, 4, GL_FLOAT, GL_FALSE, stride, &vertices[0].color);
    impl_->vertex_attrib_pointer(2, 2, GL_FLOAT, GL_FALSE, stride, &vertices[0].texture);
    const GLenum primitive_mode = command.topology == PrimitiveTopology::kLineList
                                      ? GL_LINES
                                      : command.topology == PrimitiveTopology::kTriangleList
                                            ? GL_TRIANGLES
                                            : GL_TRIANGLE_STRIP;
    impl_->draw_arrays(primitive_mode, 0, static_cast<GLsizei>(vertices.size()));
    if (impl_->ShouldCheckDrawError() && impl_->get_error() != GL_NO_ERROR)
    {
        *error = "OpenGL rejected the Direct3D3 draw command";
        return false;
    }
    error->clear();
    return true;
}

void Sdl3OpenGlBackend::DiscardTexture(std::uint64_t identity)
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
        impl_->delete_textures(1, &found->second.name);
        impl_->textures.erase(found);
    }
}

bool Sdl3OpenGlBackend::ClearRenderTarget(std::uint16_t rgb565_color, std::string* error)
{
    if (impl_ == nullptr || error == nullptr)
    {
        return false;
    }
    if (!impl_->MakeCurrent(error))
    {
        return false;
    }
    if (impl_->render_framebuffer == 0 || impl_->render_color_texture == 0)
    {
        *error = "OpenGL RGB565 render target is unavailable";
        return false;
    }
    impl_->bind_framebuffer(kFramebuffer, impl_->render_framebuffer);
    impl_->viewport(0,
                    0,
                    static_cast<GLsizei>(impl_->logical_width),
                    static_cast<GLsizei>(impl_->logical_height));
    const float red = static_cast<float>((rgb565_color >> 11) & 0x1f) / 31.0f;
    const float green = static_cast<float>((rgb565_color >> 5) & 0x3f) / 63.0f;
    const float blue = static_cast<float>(rgb565_color & 0x1f) / 31.0f;
    impl_->depth_mask(GL_TRUE);
    impl_->clear_color(red, green, blue, 1.0f);
    impl_->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // The frame owns its depth clear from here, so a later first draw does not
    // wipe what this just put down.
    impl_->frame_started = true;
    error->clear();
    return true;
}

bool Sdl3OpenGlBackend::Present(std::string* error)
{
    if (impl_ == nullptr || error == nullptr)
    {
        return false;
    }
    SDL_Event event = {};
    while (SDL_PollEvent(&event))
    {
    }
    if (!impl_->MakeCurrent(error))
    {
        return false;
    }
    int pixel_width = 0;
    int pixel_height = 0;
    if (!SDL_GetWindowSizeInPixels(impl_->window, &pixel_width, &pixel_height) ||
        pixel_width <= 0 || pixel_height <= 0)
    {
        *error = std::string("cannot query the SDL3 OpenGL window size: ") + SDL_GetError();
        return false;
    }
    if (impl_->render_framebuffer == 0 || impl_->render_color_texture == 0)
    {
        *error = "OpenGL RGB565 render target is unavailable";
        return false;
    }
    impl_->bind_framebuffer(kFramebuffer, 0);
    impl_->disable(GL_BLEND);
    impl_->disable(GL_DEPTH_TEST);
    impl_->disable(GL_CULL_FACE);
    impl_->depth_mask(GL_FALSE);
    impl_->viewport(0, 0, pixel_width, pixel_height);
    impl_->clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    impl_->clear(GL_COLOR_BUFFER_BIT);
    int presentation_width = pixel_width;
    int presentation_height = pixel_height;
    if (static_cast<std::int64_t>(pixel_width) * impl_->logical_height >
        static_cast<std::int64_t>(pixel_height) * impl_->logical_width)
    {
        presentation_height = pixel_height;
        presentation_width = static_cast<int>(
            (static_cast<std::int64_t>(pixel_height) * impl_->logical_width) /
            impl_->logical_height);
    }
    else
    {
        presentation_width = pixel_width;
        presentation_height = static_cast<int>(
            (static_cast<std::int64_t>(pixel_width) * impl_->logical_height) /
            impl_->logical_width);
    }
    const int presentation_x = (pixel_width - presentation_width) / 2;
    const int presentation_y = (pixel_height - presentation_height) / 2;
    const PresentationFilter presentation_filter = SelectPresentationFilter(
        impl_->logical_width,
        impl_->logical_height,
        static_cast<std::uint32_t>(presentation_width),
        static_cast<std::uint32_t>(presentation_height));
    impl_->viewport(presentation_x,
                    presentation_y,
                    presentation_width,
                    presentation_height);
    impl_->use_program(impl_->program);
    impl_->uniform_2f(impl_->uniforms.viewport,
                      static_cast<float>(impl_->logical_width),
                      static_cast<float>(impl_->logical_height));
    impl_->uniform_1i(impl_->uniforms.texture, 0);
    impl_->uniform_1i(impl_->uniforms.texture_enabled, 1);
    impl_->uniform_1i(impl_->uniforms.color_key_enabled, 0);
    impl_->uniform_1i(impl_->uniforms.alpha_test_enabled, 0);
    if (impl_->uniforms.alpha_reference >= 0)
    {
        impl_->uniform_1f(impl_->uniforms.alpha_reference, 0.0f);
    }
    impl_->bind_texture(GL_TEXTURE_2D, impl_->render_color_texture);
    const GLint gl_presentation_filter = presentation_filter == PresentationFilter::kNearest
                                             ? static_cast<GLint>(GL_NEAREST)
                                             : static_cast<GLint>(GL_LINEAR);
    impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_presentation_filter);
    impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_presentation_filter);
    impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClampToEdge);
    impl_->tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClampToEdge);
    std::array<GlVertex, 4> vertices = {};
    const std::array<float, 4> white = {1.0f, 1.0f, 1.0f, 1.0f};
    const auto set_vertex = [&vertices, &white](std::size_t index,
                                                float x,
                                                float y,
                                                float u,
                                                float v) {
        vertices[index].position[0] = x;
        vertices[index].position[1] = y;
        vertices[index].position[2] = 0.0f;
        vertices[index].position[3] = 1.0f;
        for (std::size_t component = 0; component < white.size(); ++component)
        {
            vertices[index].color[component] = white[component];
        }
        vertices[index].texture[0] = u;
        vertices[index].texture[1] = v;
    };
    const float logical_width = static_cast<float>(impl_->logical_width);
    const float logical_height = static_cast<float>(impl_->logical_height);
    // The FBO texture uses OpenGL's lower-left origin, while the guest's
    // logical screen coordinates use a top-left origin.
    set_vertex(0, 0.0f, 0.0f, 0.0f, 1.0f);
    set_vertex(1, logical_width, 0.0f, 1.0f, 1.0f);
    set_vertex(2, 0.0f, logical_height, 0.0f, 0.0f);
    set_vertex(3, logical_width, logical_height, 1.0f, 0.0f);
    impl_->vertex_attrib_pointer(0,
                                 4,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 static_cast<GLsizei>(sizeof(GlVertex)),
                                 vertices.data()->position);
    impl_->vertex_attrib_pointer(1,
                                 4,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 static_cast<GLsizei>(sizeof(GlVertex)),
                                 vertices.data()->color);
    impl_->vertex_attrib_pointer(2,
                                 2,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 static_cast<GLsizei>(sizeof(GlVertex)),
                                 vertices.data()->texture);
    impl_->draw_arrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertices.size()));
    // Presentation runs once per frame, so this check stays unconditional and
    // keeps catching a persistently broken GL state even when the per-draw
    // check in Draw is off.
    if (impl_->get_error() != GL_NO_ERROR)
    {
        *error = "OpenGL RGB565 render-target presentation failed";
        return false;
    }
    if (!SDL_GL_SwapWindow(impl_->window))
    {
        *error = std::string("cannot swap the SDL3 OpenGL buffers: ") + SDL_GetError();
        return false;
    }
    impl_->frame_started = false;
    error->clear();
    return true;
}

}  // namespace re2dj::graphics
