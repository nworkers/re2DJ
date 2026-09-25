#include "re2dj/hle/guest_files.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "re2dj/hle/modules/kernel32_module.h"
#include "re2dj/hle/win32_errors.h"
#include "re2dj/storage/guest_path.h"

#include "memory_services.h"
#include "test_support.h"

namespace
{

namespace hle = re2dj::hle;

// A read-only image held in memory, with case-insensitive lookup.
class MemorySource final : public hle::GuestFileSource
{
public:
    void AddFile(std::string path, std::string contents) { files_[std::move(path)] = std::move(contents); }
    void AddDirectory(std::string path) { directories_.push_back(std::move(path)); }

    bool Find(std::string_view relative_path, bool* directory, std::uint64_t* size) const override
    {
        for (const std::string& name : directories_)
        {
            if (re2dj::storage::EqualsIgnoreAsciiCase(name, relative_path))
            {
                *directory = true;
                *size = 0;
                return true;
            }
        }
        const std::string* contents = Lookup(relative_path);
        if (contents == nullptr)
        {
            return false;
        }
        *directory = false;
        *size = contents->size();
        return true;
    }
    bool ReadRange(std::string_view relative_path, std::uint64_t offset, void* destination,
                   std::size_t length) const override
    {
        const std::string* contents = Lookup(relative_path);
        if (contents == nullptr || offset + length > contents->size())
        {
            return false;
        }
        std::memcpy(destination, contents->data() + offset, length);
        return true;
    }
    bool Materialize(std::string_view relative_path, const std::filesystem::path& output) const override
    {
        const std::string* contents = Lookup(relative_path);
        std::ofstream stream(output, std::ios::binary);
        if (contents == nullptr || !stream)
        {
            return false;
        }
        stream << *contents;
        return static_cast<bool>(stream);
    }

private:
    const std::string* Lookup(std::string_view relative_path) const
    {
        for (const auto& [name, contents] : files_)
        {
            if (re2dj::storage::EqualsIgnoreAsciiCase(name, relative_path))
            {
                return &contents;
            }
        }
        return nullptr;
    }

    std::map<std::string, std::string> files_;
    std::vector<std::string> directories_;
};

struct Fixture
{
    Fixture()
    {
        overlay = std::filesystem::temp_directory_path() /
                  ("re2dj-guest-files-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::error_code code;
        std::filesystem::remove_all(overlay, code);
        auto source = std::make_unique<MemorySource>();
        source->AddFile("EZ2DJ/EZ2DJ.ini", "[General]\r\nDemoVolume=3\r\n");
        source->AddFile("EZ2DJ/DATA/SONG.EZ", "0123456789");
        source->AddDirectory("EZ2DJ/DATA");
        hle::GuestFileConfig config;
        config.chd_root = "EZ2DJ";
        config.guest_root = "D:\\ez2dj";
        config.overlay_root = overlay;
        std::string error;
        configured = files.Configure(config, std::move(source), &error);
    }
    ~Fixture()
    {
        std::error_code code;
        std::filesystem::remove_all(overlay, code);
    }

    std::string ReadAll(std::uint32_t handle)
    {
        std::vector<std::uint8_t> bytes;
        files.Read(handle, 4096, &bytes);
        return std::string(bytes.begin(), bytes.end());
    }

    std::filesystem::path overlay;
    hle::GuestFiles files;
    bool configured = false;
};

void CheckPathsAndReads(re2dj::test::Context& context)
{
    Fixture fixture;
    RE2DJ_CHECK(context, fixture.configured);
    auto& files = fixture.files;

    // Absolute, case-folded, and relative (to the guest root) names all map.
    const auto absolute = files.Open("D:\\ez2dj\\EZ2DJ.ini", true, false, hle::kOpenExisting);
    RE2DJ_CHECK(context, absolute.handle != 0);
    RE2DJ_CHECK_EQ(context, fixture.ReadAll(absolute.handle), std::string("[General]\r\nDemoVolume=3\r\n"));
    const auto folded = files.Open("d:\\EZ2DJ\\data\\song.ez", true, false, hle::kOpenExisting);
    const auto relative = files.Open("DATA\\SONG.EZ", true, false, hle::kOpenExisting);
    RE2DJ_CHECK(context, folded.handle != 0 && relative.handle != 0);

    // Size, seek from each origin, and a read that stops at the end.
    std::uint64_t value = 0;
    RE2DJ_CHECK_EQ(context, files.Size(folded.handle, &value), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, value, std::uint64_t{10});
    RE2DJ_CHECK_EQ(context, files.Seek(folded.handle, -3, 2, &value), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, value, std::uint64_t{7});
    RE2DJ_CHECK_EQ(context, fixture.ReadAll(folded.handle), std::string("789"));
    RE2DJ_CHECK_EQ(context, fixture.ReadAll(folded.handle), std::string());
    RE2DJ_CHECK_EQ(context, files.Seek(folded.handle, -20, 1, &value), hle::kWin32ErrorNegativeSeek);
    // A read-only handle cannot write.
    const std::uint8_t byte = 'x';
    RE2DJ_CHECK_EQ(context, files.Write(folded.handle, std::span<const std::uint8_t>(&byte, 1)),
                   hle::kWin32ErrorAccessDenied);

    // Missing, existing for CREATE_NEW, a directory, and outside the root.
    RE2DJ_CHECK_EQ(context, files.Open("NONE.TXT", true, false, hle::kOpenExisting).error,
                   hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, files.Open("EZ2DJ.ini", true, true, hle::kCreateNew).error,
                   hle::kWin32ErrorFileExists);
    RE2DJ_CHECK_EQ(context, files.Open("DATA", true, false, hle::kOpenExisting).error,
                   hle::kWin32ErrorAccessDenied);
    RE2DJ_CHECK(context, files.Open("C:\\WINDOWS\\WIN.INI", true, false, hle::kOpenExisting).outside_root);
    RE2DJ_CHECK(context, files.Close(absolute.handle));
    RE2DJ_CHECK(context, !files.Close(absolute.handle));
}

void CheckCopyOnWrite(re2dj::test::Context& context)
{
    Fixture fixture;
    auto& files = fixture.files;

    // Opening for write copies the CHD file into the overlay first.
    const auto writer = files.Open("EZ2DJ.ini", true, true, hle::kOpenExisting);
    RE2DJ_CHECK(context, writer.handle != 0);
    RE2DJ_CHECK(context, std::filesystem::is_regular_file(fixture.overlay / "EZ2DJ.ini"));
    const std::string patch = "[Changed]";
    RE2DJ_CHECK_EQ(context,
                   files.Write(writer.handle,
                               std::span<const std::uint8_t>(
                                   reinterpret_cast<const std::uint8_t*>(patch.data()), patch.size())),
                   hle::kWin32ErrorSuccess);
    files.Close(writer.handle);
    // Later reads come from the overlay.
    const auto reader = files.Open("EZ2DJ.ini", true, false, hle::kOpenExisting);
    RE2DJ_CHECK_EQ(context, fixture.ReadAll(reader.handle), std::string("[Changed]\r\nDemoVolume=3\r\n"));
    files.Close(reader.handle);

    // CREATE_ALWAYS on an existing file truncates it and reports
    // ERROR_ALREADY_EXISTS; a new file needs no CHD source.
    const auto again = files.Open("EZ2DJ.ini", true, true, hle::kCreateAlways);
    RE2DJ_CHECK_EQ(context, again.error, hle::kWin32ErrorAlreadyExists);
    std::uint64_t size = 1;
    files.Size(again.handle, &size);
    RE2DJ_CHECK_EQ(context, size, std::uint64_t{0});
    const auto created = files.Open("SAVE\\SCORE.DAT", false, true, hle::kCreateNew);
    RE2DJ_CHECK_EQ(context, created.error, hle::kWin32ErrorSuccess);
    RE2DJ_CHECK(context, std::filesystem::is_regular_file(fixture.overlay / "SAVE" / "SCORE.DAT"));
    // A write-only handle cannot read.
    std::vector<std::uint8_t> bytes;
    RE2DJ_CHECK_EQ(context, files.Read(created.handle, 1, &bytes), hle::kWin32ErrorAccessDenied);
}

// The kernel32 file exports over the same fixture.
void CheckKernel32FileExports(re2dj::test::Context& context)
{
    Fixture fixture;
    re2dj::test::MemoryServices services;
    services.SetFiles(&fixture.files);
    const auto descriptor = hle::modules::MakeKernel32ModuleDescriptor();
    const auto call = [&](std::string_view name, std::initializer_list<std::uint32_t> arguments)
    {
        return re2dj::test::CallModuleExport(context, services, descriptor, name, arguments);
    };
    constexpr std::uint32_t kName = re2dj::test::MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kBuffer = re2dj::test::MemoryServices::kBase + 0x100;
    constexpr std::uint32_t kCount = re2dj::test::MemoryServices::kBase + 0x80;
    constexpr std::uint32_t kHigh = re2dj::test::MemoryServices::kBase + 0x84;

    services.Put(kName, "D:\\ez2dj\\DATA\\SONG.EZ");
    const std::uint32_t handle = call("CreateFileA", {kName, 0x80000000U, 0, 0, 3, 0x80, 0}).eax;
    RE2DJ_CHECK(context, handle != 0 && handle != 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, call("GetFileSize", {handle, kHigh}).eax, 10U);
    RE2DJ_CHECK_EQ(context, services.U32(kHigh), 0U);
    RE2DJ_CHECK_EQ(context, call("SetFilePointer", {handle, 4, 0, 0}).eax, 4U);
    RE2DJ_CHECK_EQ(context, call("ReadFile", {handle, kBuffer, 3, kCount, 0}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kCount), 3U);
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer), std::uint8_t{'4'});
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 2), std::uint8_t{'6'});
    // A negative position fails with INVALID_SET_FILE_POINTER.
    RE2DJ_CHECK_EQ(context, call("SetFilePointer", {handle, 0xFFFFFFF0U, 0, 1}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorNegativeSeek);
    RE2DJ_CHECK_EQ(context, call("CloseHandle", {handle}).eax, 1U);
    // A missing file keeps ERROR_FILE_NOT_FOUND.
    services.Put(kName, "D:\\ez2dj\\NONE.TXT");
    RE2DJ_CHECK_EQ(context, call("CreateFileA", {kName, 0x80000000U, 0, 0, 3, 0x80, 0}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    // Writing goes to the overlay copy.
    services.Put(kName, "EZ2DJ.ini");
    const std::uint32_t writer = call("CreateFileA", {kName, 0xC0000000U, 0, 0, 3, 0x80, 0}).eax;
    services.Put(kBuffer, "[W]");
    RE2DJ_CHECK_EQ(context, call("WriteFile", {writer, kBuffer, 3, kCount, 0}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kCount), 3U);
    call("CloseHandle", {writer});
    std::ifstream stream(fixture.overlay / "EZ2DJ.ini", std::ios::binary);
    std::string first(3, '\0');
    stream.read(first.data(), 3);
    RE2DJ_CHECK_EQ(context, first, std::string("[W]"));
    // A path outside the guest root is not modelled.
    services.Put(kName, "C:\\WINDOWS\\WIN.INI");
    bool handled = true;
    re2dj::test::CallModuleExport(context, services, descriptor, "CreateFileA",
                                  {kName, 0x80000000U, 0, 0, 3, 0x80, 0}, &handled);
    RE2DJ_CHECK(context, !handled);
}

}  // namespace

void RunGuestFilesTests(re2dj::test::Context& context)
{
    CheckPathsAndReads(context);
    CheckCopyOnWrite(context);
    CheckKernel32FileExports(context);
}
