#include "re2dj/graphics/opengl_post_process.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <filesystem>

namespace re2dj::graphics
{

struct OpenGlPostProcess::Impl
{
    using CreateShader = GLuint(APIENTRY*)(GLenum);
    using ShaderSource = void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
    using CompileShader = void(APIENTRY*)(GLuint);
    using GetShaderiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteShader = void(APIENTRY*)(GLuint);
    using CreateProgram = GLuint(APIENTRY*)();
    using AttachShader = void(APIENTRY*)(GLuint, GLuint);
    using BindAttribLocation = void(APIENTRY*)(GLuint, GLuint, const char*);
    using LinkProgram = void(APIENTRY*)(GLuint);
    using GetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteProgram = void(APIENTRY*)(GLuint);
    using UseProgram = void(APIENTRY*)(GLuint);
    using GetUniformLocation = GLint(APIENTRY*)(GLuint, const char*);
    using Uniform1i = void(APIENTRY*)(GLint, GLint);
    using Uniform1f = void(APIENTRY*)(GLint, GLfloat);
    using Uniform2f = void(APIENTRY*)(GLint, GLfloat, GLfloat);
    using UniformMatrix4fv = void(APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);

    CreateShader create_shader = nullptr;
    ShaderSource shader_source = nullptr;
    CompileShader compile_shader = nullptr;
    GetShaderiv get_shader_iv = nullptr;
    GetShaderInfoLog get_shader_info_log = nullptr;
    DeleteShader delete_shader = nullptr;
    CreateProgram create_program = nullptr;
    AttachShader attach_shader = nullptr;
    BindAttribLocation bind_attrib_location = nullptr;
    LinkProgram link_program = nullptr;
    GetProgramiv get_program_iv = nullptr;
    GetProgramInfoLog get_program_info_log = nullptr;
    DeleteProgram delete_program = nullptr;
    UseProgram use_program = nullptr;
    GetUniformLocation get_uniform_location = nullptr;
    Uniform1i uniform_1i = nullptr;
    Uniform1f uniform_1f = nullptr;
    Uniform2f uniform_2f = nullptr;
    UniformMatrix4fv uniform_matrix_4fv = nullptr;

    bool initialized = false;
    std::filesystem::path directory;
    std::string directory_text;
    std::vector<PostShaderEntry> catalog;
    std::string active_id = kPostShaderNoneId;
    std::string last_error;
    std::vector<PostShaderParameter> parameters;
    std::vector<GLint> parameter_locations;

    GLuint program = 0;
    GLint mvp_matrix = -1;
    GLint texture = -1;
    GLint input_size = -1;
    GLint texture_size = -1;
    GLint output_size = -1;
    GLint frame_count = -1;
    GLint frame_direction = -1;
    std::uint32_t frames = 0;
};

namespace
{

constexpr GLenum kVertexShader = 0x8b31;
constexpr GLenum kFragmentShader = 0x8b30;
constexpr GLenum kCompileStatus = 0x8b81;
constexpr GLenum kLinkStatus = 0x8b82;
constexpr GLenum kInfoLogLength = 0x8b84;

// Orthographic projection of the unit square onto clip space, column-major.
constexpr GLfloat kUnitSquareProjection[16] = {
    2.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 2.0f, 0.0f, 0.0f,
    0.0f, 0.0f, -1.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 1.0f,
};

// SDL_FunctionPointer rather than void*: C++ does not promise that a function
// pointer fits an object pointer. Some drivers return small sentinel values
// for entry points they lack, which count as missing.
template <typename Function>
bool ResolveOpenGlFunction(const char* name, Function* function)
{
    const SDL_FunctionPointer address = SDL_GL_GetProcAddress(name);
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if (address == nullptr || value <= 3U || value == ~std::uintptr_t{0})
    {
        return false;
    }
    *function = reinterpret_cast<Function>(address);
    return true;
}

bool ResolveFunctions(OpenGlPostProcess::Impl* gl)
{
    return ResolveOpenGlFunction("glCreateShader", &gl->create_shader) &&
           ResolveOpenGlFunction("glShaderSource", &gl->shader_source) &&
           ResolveOpenGlFunction("glCompileShader", &gl->compile_shader) &&
           ResolveOpenGlFunction("glGetShaderiv", &gl->get_shader_iv) &&
           ResolveOpenGlFunction("glGetShaderInfoLog", &gl->get_shader_info_log) &&
           ResolveOpenGlFunction("glDeleteShader", &gl->delete_shader) &&
           ResolveOpenGlFunction("glCreateProgram", &gl->create_program) &&
           ResolveOpenGlFunction("glAttachShader", &gl->attach_shader) &&
           ResolveOpenGlFunction("glBindAttribLocation", &gl->bind_attrib_location) &&
           ResolveOpenGlFunction("glLinkProgram", &gl->link_program) &&
           ResolveOpenGlFunction("glGetProgramiv", &gl->get_program_iv) &&
           ResolveOpenGlFunction("glGetProgramInfoLog", &gl->get_program_info_log) &&
           ResolveOpenGlFunction("glDeleteProgram", &gl->delete_program) &&
           ResolveOpenGlFunction("glUseProgram", &gl->use_program) &&
           ResolveOpenGlFunction("glGetUniformLocation", &gl->get_uniform_location) &&
           ResolveOpenGlFunction("glUniform1i", &gl->uniform_1i) &&
           ResolveOpenGlFunction("glUniform1f", &gl->uniform_1f) &&
           ResolveOpenGlFunction("glUniform2f", &gl->uniform_2f) &&
           ResolveOpenGlFunction("glUniformMatrix4fv", &gl->uniform_matrix_4fv);
}

std::string TrimLog(std::string log)
{
    while (!log.empty() && (log.back() == '\0' || log.back() == '\n' || log.back() == '\r'))
    {
        log.pop_back();
    }
    return log;
}

std::string ShaderLog(OpenGlPostProcess::Impl* gl, GLuint shader)
{
    GLint length = 0;
    gl->get_shader_iv(shader, kInfoLogLength, &length);
    std::string log(length > 0 ? static_cast<std::size_t>(length) : 0U, '\0');
    if (length > 0)
    {
        gl->get_shader_info_log(shader, length, nullptr, log.data());
    }
    return TrimLog(std::move(log));
}

std::string ProgramLog(OpenGlPostProcess::Impl* gl, GLuint program)
{
    GLint length = 0;
    gl->get_program_iv(program, kInfoLogLength, &length);
    std::string log(length > 0 ? static_cast<std::size_t>(length) : 0U, '\0');
    if (length > 0)
    {
        gl->get_program_info_log(program, length, nullptr, log.data());
    }
    return TrimLog(std::move(log));
}

GLuint CompileStage(OpenGlPostProcess::Impl* gl, GLenum type, const std::string& source, std::string* error)
{
    const GLuint shader = gl->create_shader(type);
    if (shader == 0)
    {
        *error = "cannot create an OpenGL shader";
        return 0;
    }
    const char* text = source.c_str();
    gl->shader_source(shader, 1, &text, nullptr);
    gl->compile_shader(shader);
    GLint compiled = GL_FALSE;
    gl->get_shader_iv(shader, kCompileStatus, &compiled);
    if (compiled != GL_TRUE)
    {
        *error = std::string(type == kVertexShader ? "vertex" : "fragment") +
                 " stage failed to compile: " + ShaderLog(gl, shader);
        gl->delete_shader(shader);
        return 0;
    }
    return shader;
}

// Links the two stages with the attribute slots the backend's arrays use.
// Zero, with `error` set, when the link fails.
GLuint LinkProgram(OpenGlPostProcess::Impl* gl, GLuint vertex, GLuint fragment, std::string* error)
{
    const GLuint program = gl->create_program();
    if (program == 0)
    {
        *error = "cannot create an OpenGL program";
        return 0;
    }
    gl->attach_shader(program, vertex);
    gl->attach_shader(program, fragment);
    gl->bind_attrib_location(program, OpenGlPostProcess::kVertexCoordSlot, "VertexCoord");
    gl->bind_attrib_location(program, OpenGlPostProcess::kColorSlot, "COLOR");
    gl->bind_attrib_location(program, OpenGlPostProcess::kTexCoordSlot, "TexCoord");
    gl->link_program(program);
    GLint linked = GL_FALSE;
    gl->get_program_iv(program, kLinkStatus, &linked);
    if (linked != GL_TRUE)
    {
        *error = "link failed: " + ProgramLog(gl, program);
        gl->delete_program(program);
        return 0;
    }
    return program;
}

void ReleaseProgram(OpenGlPostProcess::Impl* gl)
{
    if (gl->program != 0)
    {
        gl->delete_program(gl->program);
        gl->program = 0;
    }
    gl->parameters.clear();
    gl->parameter_locations.clear();
    gl->active_id = kPostShaderNoneId;
}

std::filesystem::path ExecutableDirectory()
{
    const char* base = SDL_GetBasePath();
    return base != nullptr ? std::filesystem::path(base) : std::filesystem::path();
}

}  // namespace

OpenGlPostProcess::OpenGlPostProcess() : impl_(std::make_unique<Impl>()) {}

OpenGlPostProcess::~OpenGlPostProcess() = default;

bool OpenGlPostProcess::Initialize(std::string* message)
{
    Impl* gl = impl_.get();
    if (gl->initialized)
    {
        return true;
    }
    if (!ResolveFunctions(gl))
    {
        if (message != nullptr)
        {
            *message = "OpenGL shader entry points are unavailable";
        }
        return false;
    }
    gl->directory = ResolvePostShaderDirectory(ExecutableDirectory());
    gl->directory_text = gl->directory.string();
    gl->catalog = ListPostShaders(gl->directory);
    gl->initialized = true;
    return true;
}

void OpenGlPostProcess::Shutdown()
{
    Impl* gl = impl_.get();
    if (!gl->initialized)
    {
        return;
    }
    ReleaseProgram(gl);
    gl->initialized = false;
}

bool OpenGlPostProcess::active() const
{
    return impl_->program != 0;
}

const std::vector<PostShaderEntry>& OpenGlPostProcess::catalog() const
{
    return impl_->catalog;
}

const std::string& OpenGlPostProcess::shader_directory() const
{
    return impl_->directory_text;
}

const std::string& OpenGlPostProcess::active_id() const
{
    return impl_->active_id;
}

const std::string& OpenGlPostProcess::last_error() const
{
    return impl_->last_error;
}

std::vector<PostShaderParameter>& OpenGlPostProcess::parameters()
{
    return impl_->parameters;
}

bool OpenGlPostProcess::Select(std::string_view id)
{
    Impl* gl = impl_.get();
    gl->last_error.clear();
    if (!gl->initialized)
    {
        gl->last_error = "post-processing is not initialized";
        return false;
    }
    ReleaseProgram(gl);
    if (id.empty() || id == kPostShaderNoneId)
    {
        return true;
    }

    const PostShaderEntry* entry = FindPostShader(gl->catalog, id);
    std::string text;
    PostShaderProgramSource source;
    if (entry == nullptr)
    {
        gl->last_error = "no shader named '" + std::string(id) + "' in " + gl->directory_text + " or built in";
    }
    else if (LoadPostShaderText(*entry, &text, &gl->last_error) &&
             BuildPostShaderProgramSource(text, &source, &gl->last_error))
    {
        const GLuint vertex = CompileStage(gl, kVertexShader, source.vertex, &gl->last_error);
        const GLuint fragment =
            vertex == 0 ? 0 : CompileStage(gl, kFragmentShader, source.fragment, &gl->last_error);
        if (vertex != 0 && fragment != 0)
        {
            gl->program = LinkProgram(gl, vertex, fragment, &gl->last_error);
        }
        if (vertex != 0)
        {
            gl->delete_shader(vertex);
        }
        if (fragment != 0)
        {
            gl->delete_shader(fragment);
        }
    }
    if (gl->program == 0)
    {
        return false;
    }

    gl->active_id = std::string(id);
    gl->mvp_matrix = gl->get_uniform_location(gl->program, "MVPMatrix");
    gl->texture = gl->get_uniform_location(gl->program, "Texture");
    gl->input_size = gl->get_uniform_location(gl->program, "InputSize");
    gl->texture_size = gl->get_uniform_location(gl->program, "TextureSize");
    gl->output_size = gl->get_uniform_location(gl->program, "OutputSize");
    gl->frame_count = gl->get_uniform_location(gl->program, "FrameCount");
    gl->frame_direction = gl->get_uniform_location(gl->program, "FrameDirection");
    gl->parameters = std::move(source.parameters);
    for (const PostShaderParameter& parameter : gl->parameters)
    {
        gl->parameter_locations.push_back(gl->get_uniform_location(gl->program, parameter.name.c_str()));
    }
    gl->frames = 0;
    return true;
}

bool OpenGlPostProcess::Reload()
{
    Impl* gl = impl_.get();
    if (!gl->initialized)
    {
        return false;
    }
    const std::string id = gl->active_id;
    gl->catalog = ListPostShaders(gl->directory);
    return Select(id);
}

bool OpenGlPostProcess::UseForPresent(std::uint32_t input_width,
                                      std::uint32_t input_height,
                                      std::uint32_t output_width,
                                      std::uint32_t output_height)
{
    Impl* gl = impl_.get();
    if (gl->program == 0)
    {
        return false;
    }
    gl->use_program(gl->program);
    if (gl->mvp_matrix >= 0)
    {
        gl->uniform_matrix_4fv(gl->mvp_matrix, 1, GL_FALSE, kUnitSquareProjection);
    }
    if (gl->texture >= 0)
    {
        gl->uniform_1i(gl->texture, 0);
    }
    const auto input_x = static_cast<GLfloat>(input_width);
    const auto input_y = static_cast<GLfloat>(input_height);
    if (gl->input_size >= 0)
    {
        gl->uniform_2f(gl->input_size, input_x, input_y);
    }
    // The guest's render target is exactly its logical size, so the texture
    // size and the content size are the same, as libretro means them.
    if (gl->texture_size >= 0)
    {
        gl->uniform_2f(gl->texture_size, input_x, input_y);
    }
    if (gl->output_size >= 0)
    {
        gl->uniform_2f(gl->output_size, static_cast<GLfloat>(output_width), static_cast<GLfloat>(output_height));
    }
    if (gl->frame_count >= 0)
    {
        gl->uniform_1i(gl->frame_count, static_cast<GLint>(gl->frames));
    }
    if (gl->frame_direction >= 0)
    {
        gl->uniform_1i(gl->frame_direction, 1);
    }
    for (std::size_t index = 0; index < gl->parameters.size(); ++index)
    {
        if (gl->parameter_locations[index] >= 0)
        {
            gl->uniform_1f(gl->parameter_locations[index], gl->parameters[index].value);
        }
    }
    ++gl->frames;
    return true;
}

}  // namespace re2dj::graphics
