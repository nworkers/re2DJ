#ifndef RE2DJ_GRAPHICS_POST_SHADER_CONTROL_H_
#define RE2DJ_GRAPHICS_POST_SHADER_CONTROL_H_

#include <string>
#include <string_view>
#include <vector>

#include "re2dj/graphics/post_shader_catalog.h"
#include "re2dj/graphics/post_shader_source.h"

namespace re2dj::graphics
{

// What the OSD needs from the post-processing pass (task 455), without GL, so
// the UI layer does not depend on the OpenGL backend that implements it.
//
// Every method runs on the presenting thread with the OpenGL context current:
// Select and Reload compile on the spot, which the OSD's draw point allows.
class PostShaderControl
{
public:
    virtual ~PostShaderControl() = default;

    // Built-ins and the shader directory's files, as listed at the last
    // Initialize or Reload.
    virtual const std::vector<PostShaderEntry>& catalog() const = 0;
    virtual const std::string& shader_directory() const = 0;

    // `none`, or the id of the shader currently drawn.
    virtual const std::string& active_id() const = 0;
    // Why the last Select or Reload left no shader, or empty.
    virtual const std::string& last_error() const = 0;

    // Compiles and selects `id`; `none` or an empty id deselects. A failure
    // leaves no shader selected, sets last_error, and returns false.
    virtual bool Select(std::string_view id) = 0;
    // Lists the directory again and recompiles the active shader, so an
    // edited file takes effect without restarting.
    virtual bool Reload() = 0;

    // The active shader's `#pragma parameter` values, adjustable in place.
    virtual std::vector<PostShaderParameter>& parameters() = 0;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_POST_SHADER_CONTROL_H_
