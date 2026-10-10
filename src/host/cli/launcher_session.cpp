#include "launcher_session.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <spdlog/logger.h>

#include "re2dj/graphics/post_shader_catalog.h"
#include "re2dj/launcher/launcher_catalog.h"
#include "re2dj/launcher/launcher_settings.h"
#include "re2dj/logging/logging.h"
#include "re2dj/platform/sdl/launcher_window.h"
#include "re2dj/platform/self_process.h"
#include "re2dj/ui/launcher_screen.h"
#include "re2dj/version.h"

namespace re2dj::host
{
namespace
{

constexpr const char* kConfigDirectory = "cfg";

std::string JoinArguments(const std::vector<std::string>& arguments)
{
    std::string text;
    for (const std::string& argument : arguments)
    {
        text += ' ';
        text += argument;
    }
    return text;
}

}  // namespace

bool LauncherRequested(int argc, const char* launcher_variable)
{
    if (argc != 1)
    {
        return false;
    }
    return launcher_variable == nullptr || std::string_view(launcher_variable) != "0";
}

LauncherSessionResult RunLauncherSession()
{
    const std::shared_ptr<spdlog::logger> logger = logging::GetLogger();
    std::error_code code;
    const std::filesystem::path base_directory = std::filesystem::current_path(code);
    LauncherSessionResult session;
    std::string status;
    for (;;)
    {
        launcher::LauncherSettingsLoad stored = launcher::LoadLauncherSettings(kConfigDirectory);
        for (const std::string& warning : stored.warnings)
        {
            logger->warn("launcher: {}", warning);
        }

        ui::LauncherScreenModel model;
        model.title = VersionBanner("re2DJ", VersionString());
        model.catalog = launcher::BuildLauncherCatalog(base_directory);
        model.post_shaders = platform::sdl::ListLauncherPostShaders();
        model.default_post_shader = graphics::ChoosePostShader(nullptr, std::getenv(graphics::kPostShaderVariable));
        model.status = status;
        for (const launcher::LauncherEntry& entry : model.catalog)
        {
            logger->info("launcher: {:<12} {:<12} {}", entry.id, launcher::ProfileAvailabilityName(entry.availability),
                         launcher::IsLauncherEntryRunnable(entry) ? entry.location.string() : entry.reason);
        }

        const platform::sdl::LauncherWindowResult chosen =
            platform::sdl::RunLauncherWindow(model, stored.settings);
        if (!chosen.opened)
        {
            logger->warn("launcher: unavailable, {}", chosen.message);
            if (!session.ran)
            {
                return session;
            }
            session.exit_code = 1;
            return session;
        }
        session.ran = true;

        if (chosen.settings != stored.settings)
        {
            std::string error;
            if (launcher::SaveLauncherSettings(kConfigDirectory, chosen.settings, &error))
            {
                logger->info("launcher: settings saved to {}",
                             launcher::LauncherSettingsPath(kConfigDirectory).string());
            }
            else
            {
                logger->error("launcher: {}", error);
            }
        }
        if (chosen.action != ui::LauncherScreenAction::kStart)
        {
            logger->info("launcher: closed");
            session.exit_code = 0;
            return session;
        }

        const std::string& profile = chosen.settings.last_profile;
        const std::vector<std::string> arguments = launcher::BuildLaunchArguments(profile, chosen.settings);
        logger->info("launcher: starting re2dj{}", JoinArguments(arguments));
        int exit_code = 0;
        std::string error;
        if (platform::RunSelfAndWait(arguments, &exit_code, &error))
        {
            logger->info("launcher: {} ended with exit code {}", profile, exit_code);
            status = "Last run: " + profile + " ended with exit code " + std::to_string(exit_code) + ".";
        }
        else
        {
            logger->error("launcher: {}", error);
            status = "Last run: " + profile + " could not start: " + error;
        }
    }
}

}  // namespace re2dj::host
