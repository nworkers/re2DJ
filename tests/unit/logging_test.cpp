#include "test_support.h"

#include <fstream>
#include <iterator>
#include <string>

#include <spdlog/logger.h>

#include "re2dj/logging/logging.h"
#include "temporary_tree.h"

void RunLoggingTests(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    const std::filesystem::path requested = tree.root() / "runtime.log";
    std::filesystem::path actual;
    std::string error;
    re2dj::logging::LoggerOptions options;
    options.name = "logging-test";
    options.file_path = requested;
    RE2DJ_CHECK(context, re2dj::logging::Initialize(options, &actual, &error));
    RE2DJ_CHECK_EQ(context, actual, requested);

    re2dj::logging::GetLogger()->info("startup marker");
    re2dj::logging::Fatal("HLE_UNIMPLEMENTED", "module=kernel32.dll export=Missing");

    // Read while the logger is still alive to verify per-message flushing.
    std::ifstream stream(requested, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
    re2dj::logging::Shutdown();
    RE2DJ_CHECK(context, contents.find("[    info] [logging-test] startup marker") !=
                              std::string::npos);
    RE2DJ_CHECK(context, contents.find("[critical] [logging-test] FATAL HLE_UNIMPLEMENTED ") !=
                              std::string::npos);
    RE2DJ_CHECK(context, contents.find("module=kernel32.dll export=Missing") !=
                              std::string::npos);
}
