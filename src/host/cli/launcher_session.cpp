#include "launcher_session.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <spdlog/logger.h>

#include "re2dj/graphics/post_shader_catalog.h"
#include "re2dj/launcher/launcher_catalog.h"
#include "re2dj/launcher/launcher_settings.h"
#include "re2dj/logging/logging.h"
#include "re2dj/platform/https_download.h"
#include "re2dj/platform/sdl/launcher_window.h"
#include "re2dj/platform/self_process.h"
#include "re2dj/ui/launcher_screen.h"
#include "re2dj/update/launcher_updater.h"
#include "re2dj/update/semantic_version.h"
#include "re2dj/update/update_install.h"
#include "re2dj/version.h"

namespace re2dj::host
{
namespace
{

constexpr const char* kConfigDirectory = "cfg";
// The update check's switches (#17): off for one run, and, for testing an
// update from an older build, the version to take this one for.
constexpr const char* kUpdateCheckVariable = "RE2DJ_UPDATE_CHECK";
constexpr const char* kUpdateCurrentVersionVariable = "RE2DJ_UPDATE_CURRENT_VERSION";

// The update check (#17), started at once on its own thread, unless
// [Launcher] check_updates is 0 or RE2DJ_UPDATE_CHECK is 0.
std::unique_ptr<update::LauncherUpdater> CreateUpdater(const launcher::LauncherSettings& settings,
                                                       const std::filesystem::path& install_folder,
                                                       const std::shared_ptr<spdlog::logger>& logger)
{
    if (!settings.check_updates.value_or(true))
    {
        logger->info("update: check off ([Launcher] check_updates = 0)");
        return nullptr;
    }
    const char* check = std::getenv(kUpdateCheckVariable);
    if (check != nullptr && std::string_view(check) == "0")
    {
        logger->info("update: check off ({}=0)", kUpdateCheckVariable);
        return nullptr;
    }
    std::optional<update::SemanticVersion> build_version = update::ParseSemanticVersion(update::BuildVersionText());
    if (const char* pretend = std::getenv(kUpdateCurrentVersionVariable))
    {
        const std::optional<update::SemanticVersion> pretended = update::ParseSemanticVersion(pretend);
        if (pretended.has_value())
        {
            logger->warn("update: treating this build as v{} ({})", update::FormatSemanticVersion(*pretended),
                         kUpdateCurrentVersionVariable);
            build_version = pretended;
        }
    }
    if (!build_version.has_value() || install_folder.empty())
    {
        return nullptr;
    }
    update::LauncherUpdaterConfig config;
    config.build_version = *build_version;
    config.install_folder = install_folder;
    config.platform = RE2DJ_HOST_OS_LABEL;
    config.architecture = RE2DJ_HOST_ARCH_LABEL;
    config.user_agent = "re2DJ/" + std::string(VersionString()) + " (" + BuildLabel() + ")";
    config.fetch = &platform::HttpsDownloadToFile;
    config.log = [logger](const std::string& line) { logger->info("{}", line); };
    auto updater = std::make_unique<update::LauncherUpdater>(std::move(config));
    updater->StartCheck();
    return updater;
}

// Starts the freshly installed program in this one's place (#17). Linux
// replaces the process image, keeping the process id a game launcher such as
// Steam tracks; Windows runs the new program as a child and passes its exit
// code on. The test override described the old build, so it goes first.
int RestartAfterUpdate(const std::filesystem::path& executable, const std::shared_ptr<spdlog::logger>& logger)
{
    platform::ClearEnvironmentVariable(kUpdateCurrentVersionVariable);
    logger->info("update: restarting {}", executable.string());
    logger->flush();
    std::string error;
    platform::ReplaceSelfProcess(executable, &error);
    int exit_code = 0;
    if (!platform::RunExecutableAndWait(executable, {}, &exit_code, &error))
    {
        logger->error("update: the updated program did not start ({}); start re2DJ again", error);
        return 1;
    }
    return exit_code;
}

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

    // Files an earlier update renamed aside go now, and the update check
    // starts while the launcher opens (#17). The executable's path is taken
    // before an install renames it.
    const std::filesystem::path executable = platform::SelfExecutablePath();
    const std::filesystem::path install_folder = executable.parent_path();
    if (!install_folder.empty())
    {
        if (const std::size_t removed = update::RemovePreviousInstallLeftovers(install_folder); removed != 0U)
        {
            logger->info("update: removed {} files left by the previous update", removed);
        }
    }
    const std::unique_ptr<update::LauncherUpdater> updater =
        CreateUpdater(launcher::LoadLauncherSettings(kConfigDirectory).settings, install_folder, logger);

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
        model.updater = updater.get();
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
        if (chosen.action == ui::LauncherScreenAction::kInstallUpdate && updater != nullptr)
        {
            std::string install_error;
            if (updater->Install(&install_error))
            {
                return {true, RestartAfterUpdate(executable, logger)};
            }
            // The old files are back in place; the launcher reopens and shows
            // the failure with a Retry.
            logger->warn("update: not installed: {}", install_error);
            continue;
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
