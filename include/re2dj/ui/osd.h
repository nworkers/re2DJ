#ifndef RE2DJ_UI_OSD_H_
#define RE2DJ_UI_OSD_H_

#include <cstdint>
#include <string>
#include <vector>

#include "re2dj/graphics/gl_renderer_identity.h"
#include "re2dj/graphics/post_shader_control.h"
#include "re2dj/graphics/present_overlay.h"

namespace re2dj::ui
{

// One on/off control on the on-screen display.
//
// The OSD never owns the state it shows. It asks for the current value every
// frame it is visible and writes back only when the user changes it, so a value
// changed by something else - the guest itself, say - is always shown as it
// actually is rather than as the OSD last left it.
struct OsdToggle
{
    const char* label = nullptr;
    // Returns false when the value cannot be read right now, in which case the
    // control is shown disabled.
    bool (*read)(void* context, bool* value) = nullptr;
    bool (*write)(void* context, bool value) = nullptr;
    void* context = nullptr;
};

// The on-screen display, drawn with Dear ImGui over each presented frame.
//
// Hidden until asked for, and building no UI frame while hidden, so a session
// that never opens it pays nothing beyond a flag check per present.
//
// It is platform-neutral. It takes input through its own queue instead of an
// ImGui platform backend: the window is owned by the guest, and each host feeds
// the queue from wherever it actually receives input. Queueing, toggling and
// adding controls may happen on any thread; drawing happens on the presenting
// thread with the OpenGL context current.
class Osd : public graphics::PresentOverlay
{
public:
    Osd();
    ~Osd() override;

    Osd(const Osd&) = delete;
    Osd& operator=(const Osd&) = delete;

    // Informational lines shown above the controls, in order.
    void SetInfoLines(const std::vector<std::string>& lines);
    // What draws the picture (#6), shown in its own section right below the
    // information lines. Absent until set.
    void SetRendererIdentity(const graphics::GlRendererIdentity& identity);
    void AddToggle(const OsdToggle& toggle);
    // The post-processing pass whose shader menu the OSD shows below the
    // toggles (task 455), or null to hide the menu. Not owned; it must outlive
    // the OSD or be removed first. Choosing a shader compiles it during the
    // draw, on the presenting thread, which the control requires.
    void SetPostShaderControl(graphics::PostShaderControl* control);

    void ToggleVisible();
    bool visible() const;

    // Window-pixel coordinates with the origin at the top left.
    void QueueMousePosition(float x, float y);
    // 0 is the primary button, 1 the secondary.
    void QueueMouseButton(int button, bool down);

    void DrawOverlay(int pixel_width, int pixel_height) override;

    // For the host's diagnostics, since this layer keeps no log. All are safe
    // to read from any thread.
    enum class RendererState
    {
        kNotStarted,
        kReady,
        kFailed,
    };
    RendererState renderer_state() const;
    // How many frames the OSD has actually drawn.
    std::uint32_t frames_drawn() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace re2dj::ui

#endif  // RE2DJ_UI_OSD_H_
