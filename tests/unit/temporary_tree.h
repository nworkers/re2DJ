#ifndef RE2DJ_TESTS_UNIT_TEMPORARY_TREE_H_
#define RE2DJ_TESTS_UNIT_TEMPORARY_TREE_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace re2dj::test
{

// A throwaway stand-in for a user's HDD dump, created under the system
// temporary directory and removed on destruction.
//
// Real dumps never enter the repository, so every fixture the suite needs is
// built at run time rather than checked in.
class TemporaryTree
{
public:
    TemporaryTree();
    ~TemporaryTree();

    TemporaryTree(const TemporaryTree&) = delete;
    TemporaryTree& operator=(const TemporaryTree&) = delete;

    const std::filesystem::path& root() const
    {
        return root_;
    }

    // Creates any missing parent directories, then writes the file.
    void WriteText(const std::string& relative_path, const std::string& text) const;
    void WriteBytes(const std::string& relative_path,
                    const std::vector<std::uint8_t>& bytes) const;
    void MakeDirectory(const std::string& relative_path) const;

private:
    std::filesystem::path root_;
};

}  // namespace re2dj::test

#endif  // RE2DJ_TESTS_UNIT_TEMPORARY_TREE_H_
