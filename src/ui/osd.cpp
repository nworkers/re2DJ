#include "re2dj/ui/osd.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

namespace re2dj::ui
{
namespace
{

struct QueuedInput
{
    enum class Kind
    {
        kMousePosition,
        kMouseButton,
    };
    Kind kind = Kind::kMousePosition;
    float x = 0.0f;
    float y = 0.0f;
    int button = 0;
    bool down = false;
};

// The guest's logical frame is 480 pixels tall. Scaling the UI by how many of
// those the window shows keeps its size proportional to the game at any window
// scale, which a fixed pixel size would not.
constexpr float kLogicalHeight = 480.0f;
// Kept below one font pixel per logical pixel so the display stays compact over
// the game instead of growing with the window scale at full rate.
constexpr float kFontScaleFactor = 0.75f;
constexpr float kMinimumFontScale = 0.75f;

// The backend creates an OpenGL 2.1 compatibility context, whose GLSL is 1.20.
constexpr char kGlslVersion[] = "#version 120";

// The shader list, Reload, the last error and the active shader's parameters
// (task 455).
void DrawPostShaderMenu(graphics::PostShaderControl* control)
{
    ImGui::SeparatorText("Screen shader");
    const std::string current = control->active_id();
    std::string chosen;
    if (ImGui::BeginCombo("Shader", current.c_str()))
    {
        if (ImGui::Selectable(graphics::kPostShaderNoneId, current == graphics::kPostShaderNoneId))
        {
            chosen = graphics::kPostShaderNoneId;
        }
        for (const graphics::PostShaderEntry& entry : control->catalog())
        {
            const std::string label = entry.builtin ? entry.id + "  (built-in)" : entry.id;
            if (ImGui::Selectable(label.c_str(), current == entry.id))
            {
                chosen = entry.id;
            }
        }
        ImGui::EndCombo();
    }
    // Compiled after the combo closes so the menu never draws half a switch.
    if (!chosen.empty() && chosen != current)
    {
        control->Select(chosen);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        control->Reload();
    }
    if (!control->last_error().empty())
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", control->last_error().c_str());
        ImGui::PopTextWrapPos();
    }
    for (graphics::PostShaderParameter& parameter : control->parameters())
    {
        const std::string& label = parameter.description.empty() ? parameter.name : parameter.description;
        ImGui::PushID(parameter.name.c_str());
        ImGui::SliderFloat(label.c_str(), &parameter.value, parameter.minimum, parameter.maximum, "%.2f");
        ImGui::PopID();
    }
    ImGui::TextDisabled("Files: %s/*.glsl. Changes last for this run;", control->shader_directory().c_str());
    ImGui::TextDisabled("--post-shader or RE2DJ_POST_SHADER sets the start.");
}

}  // namespace

struct Osd::Impl
{
    std::atomic<bool> visible{false};
    std::atomic<int> renderer_state{0};
    std::atomic<std::uint32_t> frames_drawn{0};

    std::mutex mutex;
    std::vector<QueuedInput> input;
    std::vector<OsdToggle> toggles;
    std::vector<std::string> info_lines;
    graphics::PostShaderControl* post_shader_control = nullptr;

    // Touched only on the presenting thread.
    ImGuiContext* context = nullptr;
    bool renderer_ready = false;
    // Set once initialization has failed, so a broken renderer is not retried
    // on every present.
    bool renderer_failed = false;
    std::chrono::steady_clock::time_point last_frame;

    bool EnsureRenderer()
    {
        if (renderer_ready)
        {
            return true;
        }
        if (renderer_failed)
        {
            return false;
        }
        IMGUI_CHECKVERSION();
        context = ImGui::CreateContext();
        ImGui::SetCurrentContext(context);
        ImGuiIO& io = ImGui::GetIO();
        // No imgui.ini or log file: the process runs from the user's working
        // directory, and a UI library must not leave files there.
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ImGui::StyleColorsDark();
        if (!ImGui_ImplOpenGL3_Init(kGlslVersion))
        {
            ImGui::DestroyContext(context);
            context = nullptr;
            renderer_failed = true;
            renderer_state.store(static_cast<int>(RendererState::kFailed));
            return false;
        }
        last_frame = std::chrono::steady_clock::now();
        renderer_ready = true;
        renderer_state.store(static_cast<int>(RendererState::kReady));
        return true;
    }
};

Osd::Osd() : impl_(new Impl) {}

// The renderer's objects belong to an OpenGL context that may already be gone
// by the time this runs, so they are not released here. In practice the OSD
// lives as long as the process.
Osd::~Osd()
{
    delete impl_;
}

void Osd::SetInfoLines(const std::vector<std::string>& lines)
{
    const std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->info_lines = lines;
}

void Osd::AddToggle(const OsdToggle& toggle)
{
    if (toggle.label == nullptr || toggle.read == nullptr || toggle.write == nullptr)
    {
        return;
    }
    const std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->toggles.push_back(toggle);
}

void Osd::SetPostShaderControl(graphics::PostShaderControl* control)
{
    const std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->post_shader_control = control;
}

void Osd::ToggleVisible()
{
    const bool now_visible = !impl_->visible.load();
    impl_->visible.store(now_visible);
    if (!now_visible)
    {
        // Input queued while it was shown must not replay the next time it opens.
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->input.clear();
    }
}

bool Osd::visible() const
{
    return impl_->visible.load();
}

void Osd::QueueMousePosition(float x, float y)
{
    if (!visible())
    {
        return;
    }
    QueuedInput event;
    event.kind = QueuedInput::Kind::kMousePosition;
    event.x = x;
    event.y = y;
    const std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->input.push_back(event);
}

void Osd::QueueMouseButton(int button, bool down)
{
    if (!visible() || button < 0 || button >= ImGuiMouseButton_COUNT)
    {
        return;
    }
    QueuedInput event;
    event.kind = QueuedInput::Kind::kMouseButton;
    event.button = button;
    event.down = down;
    const std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->input.push_back(event);
}

void Osd::DrawOverlay(int pixel_width, int pixel_height)
{
    if (!visible() || pixel_width <= 0 || pixel_height <= 0 || !impl_->EnsureRenderer())
    {
        return;
    }
    ImGui::SetCurrentContext(impl_->context);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(pixel_width), static_cast<float>(pixel_height));
    const auto now = std::chrono::steady_clock::now();
    io.DeltaTime = std::max(std::chrono::duration<float>(now - impl_->last_frame).count(), 1.0e-4f);
    impl_->last_frame = now;
    ImGui::GetStyle().FontScaleMain =
        std::max(kMinimumFontScale,
                 static_cast<float>(pixel_height) / kLogicalHeight * kFontScaleFactor);

    std::vector<QueuedInput> input;
    std::vector<OsdToggle> toggles;
    std::vector<std::string> info_lines;
    graphics::PostShaderControl* post_shader_control = nullptr;
    {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        input.swap(impl_->input);
        toggles = impl_->toggles;
        info_lines = impl_->info_lines;
        post_shader_control = impl_->post_shader_control;
    }
    for (const QueuedInput& event : input)
    {
        if (event.kind == QueuedInput::Kind::kMousePosition)
        {
            io.AddMousePosEvent(event.x, event.y);
        }
        else
        {
            io.AddMouseButtonEvent(event.button, event.down);
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    // Pinned across the full width of the window, with its height following
    // the content. The width is held by a constraint because auto-resize would
    // otherwise shrink it to the content as well.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(io.DisplaySize.x, 0.0f),
                                        ImVec2(io.DisplaySize.x, FLT_MAX));
    if (ImGui::Begin("re2DJ",
                     nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings))
    {
        for (const std::string& line : info_lines)
        {
            ImGui::TextUnformatted(line.c_str());
        }
        if (!toggles.empty())
        {
            ImGui::Separator();
        }
        for (std::size_t index = 0; index < toggles.size(); ++index)
        {
            const OsdToggle& toggle = toggles[index];
            ImGui::PushID(static_cast<int>(index));
            bool value = false;
            const bool readable = toggle.read(toggle.context, &value);
            ImGui::BeginDisabled(!readable);
            if (ImGui::Checkbox(toggle.label, &value))
            {
                toggle.write(toggle.context, value);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (post_shader_control != nullptr)
        {
            DrawPostShaderMenu(post_shader_control);
        }
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    impl_->frames_drawn.fetch_add(1);
}

Osd::RendererState Osd::renderer_state() const
{
    return static_cast<RendererState>(impl_->renderer_state.load());
}

std::uint32_t Osd::frames_drawn() const
{
    return impl_->frames_drawn.load();
}

}  // namespace re2dj::ui
