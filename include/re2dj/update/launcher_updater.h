#ifndef RE2DJ_UPDATE_LAUNCHER_UPDATER_H_
#define RE2DJ_UPDATE_LAUNCHER_UPDATER_H_

#include "re2dj/platform/https_download.h"
#include "re2dj/update/release_info.h"
#include "re2dj/update/semantic_version.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace re2dj::update
{

// #17 (rePIU #48). Where an update stands, for the launcher to draw.
enum class UpdateStage
{
    kIdle,
    kChecking,
    kUpToDate,
    kAvailable,
    kDownloading,
    kVerifying,
    kStaged,
    kFailed,
};

struct UpdateSnapshot
{
    UpdateStage stage = UpdateStage::kIdle;
    // The newer release, once one is known ("0.0.215"); empty otherwise.
    std::string latest_version;
    std::string page_url;
    // Whether this install may be replaced (see CanInstallInto), and why not.
    bool installable = false;
    std::string not_installable_reason;
    std::uint64_t bytes_expected = 0;
    std::uint64_t bytes_received = 0;
    // Why the last step failed.
    std::string message;
};

using HttpsFetch = std::function<platform::HttpsDownloadResult(
    const platform::HttpsDownloadRequest&, const std::filesystem::path&,
    std::string*)>;

struct LauncherUpdaterConfig
{
    SemanticVersion build_version;
    std::filesystem::path install_folder;
    // RE2DJ_HOST_OS_LABEL and RE2DJ_HOST_ARCH_LABEL (version.h).
    std::string platform;
    std::string architecture;
    std::string repository = "reexec/re2DJ";
    std::string user_agent = "re2DJ";
    // The fetch: the host's `platform::HttpsDownloadToFile`, which the
    // caller passes since the shared core links no host code, or a test's.
    HttpsFetch fetch;
    // One line per step, for the loader's log. May be empty.
    std::function<void(const std::string&)> log;
};

// Checks the latest release and, when asked, downloads, verifies and stages
// it, each on a background thread so the launcher never waits. Installing the
// staged files is left to the caller's thread, once the launcher's window is
// closed (Install).
class LauncherUpdater
{
public:
    explicit LauncherUpdater(LauncherUpdaterConfig config);
    // Stops a transfer in progress and waits for the worker.
    ~LauncherUpdater();

    LauncherUpdater(const LauncherUpdater&) = delete;
    LauncherUpdater& operator=(const LauncherUpdater&) = delete;

    void StartCheck();
    // From kAvailable (or a failed download) when installable; false otherwise.
    bool StartDownload();
    [[nodiscard]] UpdateSnapshot Snapshot() const;

    // Replaces the install's files with the staged ones. Only from kStaged.
    bool Install(std::string* error);

    // The folder downloads and staging use, inside the install folder.
    [[nodiscard]] std::filesystem::path WorkFolder() const;

private:
    void RunCheck();
    void RunDownload();
    void Fail(const std::string& message);
    void Log(const std::string& line) const;
    void JoinWorker();

    LauncherUpdaterConfig config_;
    mutable std::mutex mutex_;
    UpdateSnapshot snapshot_;
    ReleaseAsset asset_;
    std::filesystem::path download_path_;
    std::filesystem::path staging_path_;
    std::vector<std::filesystem::path> staged_files_;
    std::atomic<bool> cancel_{false};
    std::thread worker_;
};

}  // namespace re2dj::update

#endif  // RE2DJ_UPDATE_LAUNCHER_UPDATER_H_
