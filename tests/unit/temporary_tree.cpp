#include "temporary_tree.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <system_error>

namespace re2dj::test
{

namespace
{

// The clock alone is not enough: two trees constructed in the same tick would
// share a directory and delete each other's files on destruction.
std::atomic<unsigned> g_tree_counter{0};

}  // namespace

TemporaryTree::TemporaryTree()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const unsigned sequence = g_tree_counter.fetch_add(1);
    root_ = std::filesystem::temp_directory_path() /
            ("re2dj_test_" + std::to_string(static_cast<long long>(stamp)) + "_" +
             std::to_string(sequence));
    std::error_code code;
    std::filesystem::create_directories(root_, code);
}

TemporaryTree::~TemporaryTree()
{
    std::error_code code;
    std::filesystem::remove_all(root_, code);
}

void TemporaryTree::WriteText(const std::string& relative_path,
                              const std::string& text) const
{
    const std::filesystem::path target = root_ / std::filesystem::path(relative_path);
    std::error_code code;
    std::filesystem::create_directories(target.parent_path(), code);
    std::ofstream stream(target, std::ios::binary);
    stream << text;
}

void TemporaryTree::WriteBytes(const std::string& relative_path,
                               const std::vector<std::uint8_t>& bytes) const
{
    const std::filesystem::path target = root_ / std::filesystem::path(relative_path);
    std::error_code code;
    std::filesystem::create_directories(target.parent_path(), code);
    std::ofstream stream(target, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void TemporaryTree::MakeDirectory(const std::string& relative_path) const
{
    std::error_code code;
    std::filesystem::create_directories(root_ / std::filesystem::path(relative_path), code);
}

}  // namespace re2dj::test
