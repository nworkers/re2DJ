#include "re2dj/ui/launcher_screen.h"

#include <cstddef>

#include "imgui.h"
#include "re2dj/graphics/post_shader_catalog.h"

namespace re2dj::ui
{
namespace
{

constexpr ImVec4 kWarningColor(1.0f, 0.45f, 0.35f, 1.0f);
constexpr ImVec4 kReadyColor(0.45f, 0.85f, 0.45f, 1.0f);

// The stored profile when it is still runnable, otherwise the first runnable
// one, otherwise the first row.
int InitialSelection(const std::vector<launcher::LauncherEntry>& catalog, const std::string& last_profile)
{
    int first_runnable = -1;
    for (std::size_t index = 0; index < catalog.size(); ++index)
    {
        const launcher::LauncherEntry& entry = catalog[index];
        if (!launcher::IsLauncherEntryRunnable(entry))
        {
            continue;
        }
        if (entry.id == last_profile)
        {
            return static_cast<int>(index);
        }
        if (first_runnable < 0)
        {
            first_runnable = static_cast<int>(index);
        }
    }
    return first_runnable >= 0 ? first_runnable : 0;
}

// A press that should start the row, not just select it: a double click, or
// Enter, Space or the gamepad's confirm button on the focused row.
bool StartRequested()
{
    return ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
           ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
           ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
}

// The profile list. True when a row asked to start.
bool DrawProfileTable(const LauncherScreenModel& model, LauncherScreenState* state, const ImVec2& size)
{
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                       ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("profiles", 3, kFlags, size))
    {
        return false;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("ez2dj1stse__").x);
    ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Several CHDs__").x);
    ImGui::TableHeadersRow();

    bool start = false;
    for (std::size_t index = 0; index < model.catalog.size(); ++index)
    {
        const launcher::LauncherEntry& entry = model.catalog[index];
        const bool runnable = launcher::IsLauncherEntryRunnable(entry);
        const bool selected = state->selected == static_cast<int>(index);
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (!state->focus_placed && selected)
        {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetScrollHereY();
            state->focus_placed = true;
        }
        if (!runnable)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }
        constexpr ImGuiSelectableFlags kRowFlags =
            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
        if (ImGui::Selectable(entry.id.c_str(), selected, kRowFlags))
        {
            state->selected = static_cast<int>(index);
            start = runnable && StartRequested();
        }
        // Moving with the arrows or the pad selects the row it lands on.
        if (ImGui::IsItemFocused())
        {
            state->selected = static_cast<int>(index);
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.display_name.c_str());
        ImGui::TableSetColumnIndex(2);
        if (runnable)
        {
            ImGui::TextColored(kReadyColor, "%s", launcher::ProfileAvailabilityName(entry.availability));
        }
        else
        {
            ImGui::TextUnformatted(launcher::ProfileAvailabilityName(entry.availability));
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
    return start;
}

const char* ColorDepthLabel(graphics::ColorDepth depth)
{
    return depth == graphics::ColorDepth::k32 ? "32-bit" : "16-bit (original)";
}

void DrawOptions(const LauncherScreenModel& model,
                 const launcher::LauncherEntry* entry,
                 launcher::LauncherSettings* settings)
{
    ImGui::SeparatorText("Options");

    bool fullscreen = settings->fullscreen.value_or(false);
    if (ImGui::Checkbox("Fullscreen", &fullscreen))
    {
        settings->fullscreen = fullscreen;
    }

    const graphics::ColorDepth depth = settings->color_depth.value_or(graphics::ColorDepth::k16);
    if (ImGui::BeginCombo("Colour depth", ColorDepthLabel(depth)))
    {
        for (const graphics::ColorDepth candidate : {graphics::ColorDepth::k16, graphics::ColorDepth::k32})
        {
            if (ImGui::Selectable(ColorDepthLabel(candidate), candidate == depth))
            {
                settings->color_depth = candidate;
            }
        }
        ImGui::EndCombo();
    }

    const std::string shader = settings->post_shader.value_or(model.default_post_shader);
    if (ImGui::BeginCombo("Screen shader", shader.c_str()))
    {
        if (ImGui::Selectable(graphics::kPostShaderNoneId, shader == graphics::kPostShaderNoneId))
        {
            settings->post_shader = graphics::kPostShaderNoneId;
        }
        for (const std::string& id : model.post_shaders)
        {
            if (ImGui::Selectable(id.c_str(), shader == id))
            {
                settings->post_shader = id;
            }
        }
        ImGui::EndCombo();
    }

    const float default_gain = entry != nullptr ? entry->default_audio_gain_db : 0.0f;
    float gain = settings->audio_gain_db.value_or(default_gain);
    if (ImGui::SliderFloat("Sound gain", &gain, launcher::kLauncherMinimumGainDb, launcher::kLauncherMaximumGainDb,
                           "%+.1f dB"))
    {
        settings->audio_gain_db = gain;
    }
    if (settings->audio_gain_db.has_value())
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Default"))
        {
            settings->audio_gain_db.reset();
        }
    }
}

}  // namespace

LauncherScreenAction DrawLauncherScreen(const LauncherScreenModel& model, LauncherScreenState* state)
{
    if (state->selected < 0 || state->selected >= static_cast<int>(model.catalog.size()))
    {
        state->selected = InitialSelection(model.catalog, state->settings.last_profile);
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                              ImGuiWindowFlags_NoSavedSettings |
                                              ImGuiWindowFlags_NoBringToFrontOnFocus;
    LauncherScreenAction action = LauncherScreenAction::kNone;
    if (ImGui::Begin("re2DJ launcher", nullptr, kWindowFlags))
    {
        ImGui::TextUnformatted(model.title.c_str());
        ImGui::TextDisabled("Choose a game and start it. Enter or a double click starts the row; Esc quits.");
        ImGui::Spacing();

        // Room under the list for the detail line, the options, the status
        // and the buttons.
        const float style_lines = ImGui::GetTextLineHeightWithSpacing() * 4.0f;
        const float frame_lines = ImGui::GetFrameHeightWithSpacing() * 6.0f;
        if (DrawProfileTable(model, state, ImVec2(0.0f, -(style_lines + frame_lines))))
        {
            action = LauncherScreenAction::kStart;
        }
        const launcher::LauncherEntry* entry =
            model.catalog.empty() ? nullptr : &model.catalog[static_cast<std::size_t>(state->selected)];
        const bool runnable = entry != nullptr && launcher::IsLauncherEntryRunnable(*entry);

        if (entry != nullptr)
        {
            if (runnable)
            {
                ImGui::TextDisabled("%s", entry->location.string().c_str());
            }
            else
            {
                ImGui::TextColored(kWarningColor, "%s", entry->reason.c_str());
            }
        }

        DrawOptions(model, entry, &state->settings);

        ImGui::Spacing();
        if (!model.status.empty())
        {
            ImGui::TextDisabled("%s", model.status.c_str());
        }
        else
        {
            ImGui::NewLine();
        }

        const ImVec2 button_size(ImGui::CalcTextSize("Start").x * 3.0f, 0.0f);
        ImGui::BeginDisabled(!runnable);
        if (ImGui::Button("Start", button_size))
        {
            action = LauncherScreenAction::kStart;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Quit", button_size) ||
            (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)))
        {
            action = LauncherScreenAction::kQuit;
        }
    }
    ImGui::End();

    if (action == LauncherScreenAction::kStart)
    {
        const launcher::LauncherEntry* entry =
            model.catalog.empty() ? nullptr : &model.catalog[static_cast<std::size_t>(state->selected)];
        if (entry == nullptr || !launcher::IsLauncherEntryRunnable(*entry))
        {
            return LauncherScreenAction::kNone;
        }
        state->settings.last_profile = entry->id;
    }
    return action;
}

}  // namespace re2dj::ui
