#include "re2dj/hle/guest_files.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <system_error>
#include <vector>

#include "re2dj/hle/guest_gdi.h"
#include "re2dj/hle/modules/gdi32_module.h"
#include "re2dj/hle/modules/kernel32_module.h"
#include "re2dj/hle/modules/user32_module.h"
#include "re2dj/hle/private_profile.h"
#include "re2dj/hle/win32_errors.h"
#include "re2dj/storage/guest_path.h"

#include "bitmap_file_bytes.h"
#include "memory_services.h"
#include "test_support.h"

namespace
{

namespace hle = re2dj::hle;

// A read-only image held in memory, with case-insensitive lookup.
class MemorySource final : public hle::GuestFileSource
{
public:
    void AddFile(std::string path, std::string contents)
    {
        order_.push_back(path);
        files_[std::move(path)] = std::move(contents);
    }
    void AddDirectory(std::string path)
    {
        order_.push_back(path);
        directories_.push_back(std::move(path));
    }

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
    // Entries directly under the directory, in the order they were added.
    bool ListDirectory(std::string_view relative_path, std::vector<re2dj::storage::Fat32Entry>* entries) const override
    {
        const std::string prefix = std::string(relative_path) + "/";
        const auto under = [&](const std::string& path) {
            return path.size() > prefix.size() &&
                   re2dj::storage::EqualsIgnoreAsciiCase(path.substr(0, prefix.size()), prefix);
        };
        const auto child = [&](const std::string& path) {
            return under(path) && path.find('/', prefix.size()) == std::string::npos;
        };
        // A directory is one added as such, or one some path lies under.
        bool found = false;
        for (const std::string& path : order_)
        {
            found = found || under(path) ||
                    (re2dj::storage::EqualsIgnoreAsciiCase(path, relative_path) &&
                     files_.find(path) == files_.end());
        }
        if (!found)
        {
            return false;
        }
        entries->clear();
        for (const std::string& path : order_)
        {
            if (!child(path)) continue;
            re2dj::storage::Fat32Entry entry;
            entry.name = path.substr(prefix.size());
            const auto file = files_.find(path);
            entry.directory = file == files_.end();
            entry.size = entry.directory ? 0 : static_cast<std::uint32_t>(file->second.size());
            entry.write_date = 0x5A21;  // 2025-01-01
            entry.write_time = 0x6000;  // 12:00:00
            entries->push_back(entry);
        }
        return true;
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
    std::vector<std::string> order_;
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
    // An open file is FILE_TYPE_DISK and leaves the last error alone.
    call("SetLastError", {1234});
    RE2DJ_CHECK_EQ(context, call("GetFileType", {handle}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, call("SetFilePointer", {handle, 4, 0, 0}).eax, 4U);
    RE2DJ_CHECK_EQ(context, call("ReadFile", {handle, kBuffer, 3, kCount, 0}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kCount), 3U);
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer), std::uint8_t{'4'});
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 2), std::uint8_t{'6'});
    // A negative position fails with INVALID_SET_FILE_POINTER.
    RE2DJ_CHECK_EQ(context, call("SetFilePointer", {handle, 0xFFFFFFF0U, 0, 1}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorNegativeSeek);
    RE2DJ_CHECK_EQ(context, call("CloseHandle", {handle}).eax, 1U);
    // A closed one is FILE_TYPE_UNKNOWN with ERROR_INVALID_HANDLE.
    RE2DJ_CHECK_EQ(context, call("GetFileType", {handle}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
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

// The current directory starts at the guest root, moves as Windows 11 moves
// it, and is what relative paths resolve against; the kernel32 exports follow
// the measured buffer and error rules.
void CheckCurrentDirectory(re2dj::test::Context& context)
{
    Fixture fixture;
    hle::GuestFiles& files = fixture.files;
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj"));
    bool outside = true;
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("data", &outside), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK(context, !outside);
    // The guest's own spelling is kept.
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj\\data"));
    const auto opened = files.Open("SONG.EZ", true, false, hle::kOpenExisting);
    RE2DJ_CHECK(context, opened.handle != 0);
    RE2DJ_CHECK_EQ(context, fixture.ReadAll(opened.handle), std::string("0123456789"));
    files.Close(opened.handle);
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("..", &outside), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj"));
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("DATA/", &outside), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj\\DATA"));
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory(".", &outside), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj\\DATA"));
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("D:\\ez2dj\\", &outside), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj"));

    // Failures leave it where it was.
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("Missing", &outside), hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("Missing\\Deeper", &outside), hle::kWin32ErrorPathNotFound);
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("EZ2DJ.ini", &outside), hle::kWin32ErrorDirectory);
    RE2DJ_CHECK_EQ(context, files.SetCurrentDirectory("", &outside), hle::kWin32ErrorInvalidName);
    RE2DJ_CHECK(context, !outside);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj"));
    // The drive's root lies outside what the model serves.
    files.SetCurrentDirectory("\\", &outside);
    RE2DJ_CHECK(context, outside);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj"));

    re2dj::test::MemoryServices services;
    services.SetFiles(&files);
    const auto descriptor = hle::modules::MakeKernel32ModuleDescriptor();
    const auto call = [&](std::string_view name, std::initializer_list<std::uint32_t> arguments)
    {
        return re2dj::test::CallModuleExport(context, services, descriptor, name, arguments);
    };
    constexpr std::uint32_t kName = re2dj::test::MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kBuffer = re2dj::test::MemoryServices::kBase + 0x100;
    call("SetLastError", {1234});
    RE2DJ_CHECK_EQ(context, call("GetCurrentDirectoryA", {0x104, kBuffer}).eax, 8U);
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 3), std::uint8_t{'e'});
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 8), std::uint8_t{0});
    // Too short: nothing written, the size needed returned.
    services.Byte(kBuffer + 0x40) = 0x41;
    RE2DJ_CHECK_EQ(context, call("GetCurrentDirectoryA", {8, kBuffer + 0x40}).eax, 9U);
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 0x40), std::uint8_t{0x41});
    RE2DJ_CHECK_EQ(context, call("GetCurrentDirectoryA", {0, 0}).eax, 9U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);

    services.Put(kName, "DATA");
    RE2DJ_CHECK_EQ(context, call("SetCurrentDirectoryA", {kName}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, files.CurrentDirectory(), std::string("D:\\ez2dj\\DATA"));
    services.Put(kName, "Missing");
    RE2DJ_CHECK_EQ(context, call("SetCurrentDirectoryA", {kName}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, call("SetCurrentDirectoryA", {0}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidParameter);
    services.Put(kName, "\\");
    bool handled = true;
    re2dj::test::CallModuleExport(context, services, descriptor, "SetCurrentDirectoryA", {kName}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// FindFirstFileA's search over the image: the directory part resolves like
// any path (the current directory included), matches come in the image's
// order, and failures carry Windows 11's errors; the kernel32 exports write
// WIN32_FIND_DATAA and follow the measured last-error rules.
void CheckFind(re2dj::test::Context& context)
{
    Fixture fixture;
    hle::GuestFiles& files = fixture.files;
    hle::GuestFiles::FindResult found = files.FindFirst("DATA\\*.*");
    RE2DJ_CHECK(context, found.handle != 0);
    RE2DJ_CHECK_EQ(context, std::string(found.first.file_name), std::string("SONG.EZ"));
    RE2DJ_CHECK_EQ(context, found.first.size_low, 10U);
    re2dj::storage::GuestFindData next;
    RE2DJ_CHECK_EQ(context, files.FindNext(found.handle, &next), hle::kWin32ErrorNoMoreFiles);
    RE2DJ_CHECK(context, files.FindClose(found.handle));
    RE2DJ_CHECK(context, !files.FindClose(found.handle));
    RE2DJ_CHECK_EQ(context, files.FindNext(found.handle, &next), hle::kWin32ErrorInvalidHandle);

    // The root's entries in order, then a pattern and a bare name.
    found = files.FindFirst("*");
    RE2DJ_CHECK_EQ(context, std::string(found.first.file_name), std::string("EZ2DJ.ini"));
    RE2DJ_CHECK_EQ(context, files.FindNext(found.handle, &next), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, std::string(next.file_name), std::string("DATA"));
    RE2DJ_CHECK_EQ(context, next.attributes, re2dj::storage::kFileAttributeDirectory);
    files.FindClose(found.handle);
    found = files.FindFirst("D:\\ez2dj\\data");
    RE2DJ_CHECK_EQ(context, std::string(found.first.file_name), std::string("DATA"));
    files.FindClose(found.handle);

    // Relative to the current directory.
    bool outside = false;
    files.SetCurrentDirectory("DATA", &outside);
    found = files.FindFirst("*.ez");
    RE2DJ_CHECK_EQ(context, std::string(found.first.file_name), std::string("SONG.EZ"));
    files.FindClose(found.handle);
    files.SetCurrentDirectory("..", &outside);

    // Windows 11's errors.
    RE2DJ_CHECK_EQ(context, files.FindFirst("DATA\\*.xyz").error, hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, files.FindFirst("Missing\\*.*").error, hle::kWin32ErrorPathNotFound);
    RE2DJ_CHECK_EQ(context, files.FindFirst("DATA\\").error, hle::kWin32ErrorInvalidParameter);
    RE2DJ_CHECK_EQ(context, files.FindFirst("").error, hle::kWin32ErrorPathNotFound);
    RE2DJ_CHECK(context, files.FindFirst("\\*.*").outside_root);
    RE2DJ_CHECK(context, files.FindFirst("C:\\WINDOWS\\*.*").outside_root);

    re2dj::test::MemoryServices services;
    services.SetFiles(&files);
    const auto descriptor = hle::modules::MakeKernel32ModuleDescriptor();
    const auto call = [&](std::string_view name, std::initializer_list<std::uint32_t> arguments)
    {
        return re2dj::test::CallModuleExport(context, services, descriptor, name, arguments);
    };
    constexpr std::uint32_t kName = re2dj::test::MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kData = re2dj::test::MemoryServices::kBase + 0x200;
    services.Put(kName, "DATA\\*.*");
    call("SetLastError", {1234});
    const std::uint32_t handle = call("FindFirstFileA", {kName, kData}).eax;
    RE2DJ_CHECK(context, handle != 0 && handle != 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, services.U32(kData), re2dj::storage::kFileAttributeNormal);
    RE2DJ_CHECK_EQ(context, services.U32(kData + 32), 10U);
    RE2DJ_CHECK_EQ(context, services.Byte(kData + 44), std::uint8_t{'S'});
    RE2DJ_CHECK_EQ(context, call("FindNextFileA", {handle, kData}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorNoMoreFiles);
    call("SetLastError", {1234});
    RE2DJ_CHECK_EQ(context, call("FindClose", {handle}).eax, 1U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, call("FindClose", {handle}).eax, 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    services.Put(kName, "DATA\\*.xyz");
    RE2DJ_CHECK_EQ(context, call("FindFirstFileA", {kName, kData}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, call("FindFirstFileA", {0, kData}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPathNotFound);
    services.Put(kName, "C:\\WINDOWS\\*.*");
    bool handled = true;
    re2dj::test::CallModuleExport(context, services, descriptor, "FindFirstFileA", {kName, kData}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// GetFileAttributesA: a path resolves like any other and is walked through
// the image with Windows 11's errors; the kernel32 export leaves the last
// error alone on success.
void CheckFileAttributes(re2dj::test::Context& context)
{
    namespace storage = re2dj::storage;
    Fixture fixture;
    hle::GuestFiles& files = fixture.files;
    bool outside = true;
    RE2DJ_CHECK_EQ(context, files.Attributes("EZ2DJ.ini", &outside).attributes, storage::kFileAttributeNormal);
    RE2DJ_CHECK(context, !outside);
    RE2DJ_CHECK_EQ(context, files.Attributes("data", &outside).attributes, storage::kFileAttributeDirectory);
    RE2DJ_CHECK_EQ(context, files.Attributes("DATA\\", &outside).attributes, storage::kFileAttributeDirectory);
    RE2DJ_CHECK_EQ(context, files.Attributes(".", &outside).attributes, storage::kFileAttributeDirectory);
    RE2DJ_CHECK_EQ(context, files.Attributes("D:\\ez2dj", &outside).attributes, storage::kFileAttributeDirectory);
    // Relative to the current directory, ".." included.
    files.SetCurrentDirectory("DATA", &outside);
    RE2DJ_CHECK_EQ(context, files.Attributes("song.ez", &outside).attributes, storage::kFileAttributeNormal);
    RE2DJ_CHECK_EQ(context, files.Attributes("..\\EZ2DJ.ini", &outside).attributes, storage::kFileAttributeNormal);
    files.SetCurrentDirectory("..", &outside);

    // Windows 11's errors.
    const auto error = [&](std::string_view path) {
        const storage::GuestFileAttributes result = files.Attributes(path, &outside);
        RE2DJ_CHECK_EQ(context, result.attributes, storage::kInvalidFileAttributes);
        return result.error;
    };
    RE2DJ_CHECK_EQ(context, error("Missing.bin"), hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, error("Missing\\SONG.EZ"), hle::kWin32ErrorPathNotFound);
    RE2DJ_CHECK_EQ(context, error("EZ2DJ.ini\\x"), hle::kWin32ErrorPathNotFound);
    RE2DJ_CHECK_EQ(context, error("EZ2DJ.ini\\"), hle::kWin32ErrorDirectory);
    RE2DJ_CHECK_EQ(context, error(""), hle::kWin32ErrorPathNotFound);
    RE2DJ_CHECK_EQ(context, error("DATA\\*.EZ"), hle::kWin32ErrorInvalidName);
    RE2DJ_CHECK(context, !outside);
    files.Attributes("C:\\WINDOWS\\WIN.INI", &outside);
    RE2DJ_CHECK(context, outside);

    re2dj::test::MemoryServices services;
    services.SetFiles(&files);
    const auto descriptor = hle::modules::MakeKernel32ModuleDescriptor();
    const auto call = [&](std::string_view name, std::initializer_list<std::uint32_t> arguments)
    {
        return re2dj::test::CallModuleExport(context, services, descriptor, name, arguments);
    };
    constexpr std::uint32_t kName = re2dj::test::MemoryServices::kBase + 0x10;
    services.Put(kName, "DATA\\SONG.EZ");
    call("SetLastError", {1234});
    RE2DJ_CHECK_EQ(context, call("GetFileAttributesA", {kName}).eax, storage::kFileAttributeNormal);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    services.Put(kName, "DATA\\NONE.EZ");
    RE2DJ_CHECK_EQ(context, call("GetFileAttributesA", {kName}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    call("SetLastError", {1234});
    RE2DJ_CHECK_EQ(context, call("GetFileAttributesA", {0}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPathNotFound);
    services.Put(kName, "");
    RE2DJ_CHECK_EQ(context, call("GetFileAttributesA", {kName}).eax, 0xFFFFFFFFU);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPathNotFound);
    services.Put(kName, "C:\\WINDOWS\\WIN.INI");
    bool handled = true;
    re2dj::test::CallModuleExport(context, services, descriptor, "GetFileAttributesA", {kName}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// The private profile rules as measured on Windows 11 (design 405).
void CheckPrivateProfile(re2dj::test::Context& context)
{
    constexpr std::string_view kIni =
        "; comment line\r\n"
        "[Main]\r\n"
        "plain=12\r\n"
        "spaced =  34  \r\n"
        "  indented=56\r\n"
        "hex=0x1A\r\n"
        "HEXUPPER=0X1b\r\n"
        "neg=-5\r\n"
        "plus=+7\r\n"
        "trail=12abc\r\n"
        "alpha=abc\r\n"
        "empty=\r\n"
        "huge=99999999999\r\n"
        "quoted=\"9\"\r\n"
        "squoted='8'\r\n"
        "dup=1\r\n"
        "dup=2\r\n"
        "octal=010\r\n"
        "negspace=- 3\r\n"
        ";hidden=15\r\n"
        "noequals\r\n"
        "[ Spaced Section ]\r\n"
        "a=21\r\n"
        "[main]\r\n"
        "later=22\r\n"
        "[Last]\r\n"
        "nolf=23";
    const auto number = [&](std::string_view section, std::string_view key) {
        const std::optional<std::string> value = hle::FindPrivateProfileValue(kIni, section, key);
        return value.has_value() ? hle::ParsePrivateProfileInt(*value, 77) : 0xDEADBEEFU;
    };
    RE2DJ_CHECK_EQ(context, number("Main", "plain"), 12U);
    RE2DJ_CHECK_EQ(context, number("MAIN", "PLAIN"), 12U);
    RE2DJ_CHECK_EQ(context, number("Main", "spaced"), 34U);
    RE2DJ_CHECK_EQ(context, number("Main", "indented"), 56U);
    RE2DJ_CHECK_EQ(context, number("Main", "hex"), 26U);
    RE2DJ_CHECK_EQ(context, number("Main", "HEXUPPER"), 0U);
    RE2DJ_CHECK_EQ(context, number("Main", "neg"), 0xFFFFFFFBU);
    RE2DJ_CHECK_EQ(context, number("Main", "plus"), 7U);
    RE2DJ_CHECK_EQ(context, number("Main", "trail"), 12U);
    RE2DJ_CHECK_EQ(context, number("Main", "alpha"), 0U);
    RE2DJ_CHECK_EQ(context, number("Main", "empty"), 77U);
    RE2DJ_CHECK_EQ(context, number("Main", "huge"), 1215752191U);
    RE2DJ_CHECK_EQ(context, number("Main", "quoted"), 9U);
    RE2DJ_CHECK_EQ(context, number("Main", "squoted"), 8U);
    RE2DJ_CHECK_EQ(context, number("Main", "dup"), 1U);
    RE2DJ_CHECK_EQ(context, number("Main", "octal"), 10U);
    RE2DJ_CHECK_EQ(context, number("Main", "negspace"), 0U);
    RE2DJ_CHECK_EQ(context, number(" Spaced Section ", "a"), 21U);
    RE2DJ_CHECK_EQ(context, number("Last", "nolf"), 23U);
    RE2DJ_CHECK(context, !hle::FindPrivateProfileValue(kIni, "Main", "hidden").has_value());
    RE2DJ_CHECK(context, !hle::FindPrivateProfileValue(kIni, "Main", ";hidden").has_value());
    RE2DJ_CHECK(context, !hle::FindPrivateProfileValue(kIni, "Main", "noequals").has_value());
    // Only a section's first occurrence is searched.
    RE2DJ_CHECK(context, !hle::FindPrivateProfileValue(kIni, "Main", "later").has_value());
    RE2DJ_CHECK(context, !hle::FindPrivateProfileValue(kIni, "Nope", "plain").has_value());
    // The product's DemoVolume.
    RE2DJ_CHECK(context, hle::PrivateProfileIntOverride("gameassignments", "demovolume", 3) == std::optional<std::uint32_t>(3));
    RE2DJ_CHECK(context, !hle::PrivateProfileIntOverride("GAMEASSIGNMENTS", "DemoVolume", 4).has_value());
    RE2DJ_CHECK(context, !hle::PrivateProfileIntOverride("GAMEASSIGNMENTS", "PlayCoins", 3).has_value());
}

// GetPrivateProfileIntA over the guest's files: relative to the current
// directory, with the measured last errors.
void CheckProfileIntExport(re2dj::test::Context& context)
{
    Fixture fixture;
    std::filesystem::create_directories(fixture.overlay);
    {
        std::ofstream ini(fixture.overlay / "bookkeeping.ini", std::ios::binary);
        ini << "[GAMEASSIGNMENTS]\r\nPlayCoins=2\r\nDemoVolume=1\r\n[STATISTICS]\r\nTOTALCOIN=12\r\n";
    }
    re2dj::test::MemoryServices services;
    services.SetFiles(&fixture.files);
    const auto descriptor = hle::modules::MakeKernel32ModuleDescriptor();
    const auto call = [&](std::initializer_list<std::uint32_t> arguments, bool* handled = nullptr) {
        return re2dj::test::CallModuleExport(context, services, descriptor, "GetPrivateProfileIntA", arguments, handled).eax;
    };
    constexpr std::uint32_t kSection = re2dj::test::MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kKey = re2dj::test::MemoryServices::kBase + 0x40;
    constexpr std::uint32_t kFile = re2dj::test::MemoryServices::kBase + 0x70;
    services.Put(kSection, "STATISTICS");
    services.Put(kKey, "totalcoin");
    services.Put(kFile, ".\\bookkeeping.ini");
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call({kSection, kKey, 5, kFile}), 12U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    services.Put(kKey, "SERVICECOIN");
    RE2DJ_CHECK_EQ(context, call({kSection, kKey, 5, kFile}), 5U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    // DemoVolume is the product's: 3, the last error untouched.
    services.Put(kSection, "GAMEASSIGNMENTS");
    services.Put(kKey, "DemoVolume");
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call({kSection, kKey, 5, kFile}), 3U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    // A missing file and a missing directory.
    services.Put(kKey, "PlayCoins");
    services.Put(kFile, ".\\none.ini");
    RE2DJ_CHECK_EQ(context, call({kSection, kKey, 5, kFile}), 5U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    services.Put(kFile, "nodir\\none.ini");
    RE2DJ_CHECK_EQ(context, call({kSection, kKey, 5, kFile}), 5U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPathNotFound);
    // A null key returns 0.
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call({kSection, 0, 5, kFile}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    // Not modelled: a bare name, which Windows looks up in its directory.
    services.Put(kFile, "bookkeeping.ini");
    bool handled = true;
    call({kSection, kKey, 5, kFile}, &handled);
    RE2DJ_CHECK(context, !handled);
}

// GetPrivateProfileStringA as measured on Windows 11: quotes, the buffer's
// cut and ERROR_MORE_DATA, the default, and the key and section lists.
void CheckProfileStringExport(re2dj::test::Context& context)
{
    Fixture fixture;
    std::filesystem::create_directories(fixture.overlay);
    {
        std::ofstream ini(fixture.overlay / "ez2dj.ini", std::ios::binary);
        ini << "[Main]\r\nplain=hello\r\ndq=\"quoted\"\r\ndqopen=\"open\r\nlong=0123456789\r\n[Other]\r\nx=1\r\n";
    }
    re2dj::test::MemoryServices services;
    services.SetFiles(&fixture.files);
    const auto descriptor = hle::modules::MakeKernel32ModuleDescriptor();
    constexpr std::uint32_t kSection = re2dj::test::MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kKey = re2dj::test::MemoryServices::kBase + 0x30;
    constexpr std::uint32_t kDefault = re2dj::test::MemoryServices::kBase + 0x50;
    constexpr std::uint32_t kFile = re2dj::test::MemoryServices::kBase + 0x70;
    constexpr std::uint32_t kBuffer = re2dj::test::MemoryServices::kBase + 0x3000;
    services.Put(kSection, "main");
    services.Put(kDefault, "def  ");
    services.Put(kFile, ".\\ez2dj.ini");
    const auto get_from = [&](const char* key, std::uint32_t size, std::uint32_t section, std::uint32_t default_address) {
        std::uint32_t key_address = 0;
        if (key != nullptr)
        {
            services.Put(kKey, key);
            key_address = kKey;
        }
        for (std::uint32_t offset = 0; offset < 64; ++offset)
        {
            services.Byte(kBuffer + offset) = 0xCC;
        }
        services.SetLastError(1234);
        return re2dj::test::CallModuleExport(context, services, descriptor, "GetPrivateProfileStringA",
                                             {section, key_address, default_address, kBuffer, size, kFile})
            .eax;
    };
    const auto get = [&](const char* key, std::uint32_t size) { return get_from(key, size, kSection, kDefault); };
    const auto text = [&](std::uint32_t length) {
        std::string value;
        for (std::uint32_t offset = 0; offset < length; ++offset)
        {
            value.push_back(static_cast<char>(services.Byte(kBuffer + offset)));
        }
        return value;
    };
    RE2DJ_CHECK_EQ(context, get("PLAIN", 32), 5U);
    RE2DJ_CHECK_EQ(context, text(6), std::string("hello\0", 6));
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    RE2DJ_CHECK_EQ(context, get("dq", 32), 6U);
    RE2DJ_CHECK_EQ(context, text(6), std::string("quoted"));
    RE2DJ_CHECK_EQ(context, get("dqopen", 32), 5U);
    RE2DJ_CHECK_EQ(context, text(5), std::string("\"open"));
    // A value that fills the buffer: cut to size - 1, ERROR_MORE_DATA even
    // when it just fits.
    RE2DJ_CHECK_EQ(context, get("long", 11), 10U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 234U);
    RE2DJ_CHECK_EQ(context, get("long", 10), 9U);
    RE2DJ_CHECK_EQ(context, text(10), std::string("012345678\0", 10));
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 10), std::uint8_t{0xCC});
    // The default without its trailing spaces, cut without ERROR_MORE_DATA.
    RE2DJ_CHECK_EQ(context, get("none", 32), 3U);
    RE2DJ_CHECK_EQ(context, text(4), std::string("def\0", 4));
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, get("none", 3), 2U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    RE2DJ_CHECK_EQ(context, get_from("none", 32, kSection, 0), 0U);
    // Key and section lists.
    RE2DJ_CHECK_EQ(context, get(nullptr, 64), 21U);
    RE2DJ_CHECK_EQ(context, text(22), std::string("plain\0dq\0dqopen\0long\0\0", 22));
    RE2DJ_CHECK_EQ(context, get(nullptr, 12), 10U);
    RE2DJ_CHECK_EQ(context, text(12), std::string("plain\0dq\0d\0\0", 12));
    RE2DJ_CHECK_EQ(context, services.LastError(), 234U);
    RE2DJ_CHECK_EQ(context, get_from(nullptr, 32, 0, kDefault), 11U);
    RE2DJ_CHECK_EQ(context, text(12), std::string("Main\0Other\0\0", 12));
    // GetPrivateProfileSectionNamesA lists the same way.
    const auto names = [&](std::uint32_t size) {
        services.SetLastError(1234);
        return re2dj::test::CallModuleExport(context, services, descriptor, "GetPrivateProfileSectionNamesA",
                                             {kBuffer, size, kFile})
            .eax;
    };
    RE2DJ_CHECK_EQ(context, names(32), 11U);
    RE2DJ_CHECK_EQ(context, text(12), std::string("Main\0Other\0\0", 12));
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    RE2DJ_CHECK_EQ(context, names(6), 4U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 234U);

    // A missing file gives the default with the open's error; the section
    // names are one NUL.
    services.Put(kFile, "nodir\\x.ini");
    RE2DJ_CHECK_EQ(context, get("plain", 32), 3U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPathNotFound);
    services.Byte(kBuffer + 1) = 0xCC;
    RE2DJ_CHECK_EQ(context, names(32), 0U);
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer), std::uint8_t{0});
    RE2DJ_CHECK_EQ(context, services.Byte(kBuffer + 1), std::uint8_t{0xCC});
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorPathNotFound);
}

// A directory dump as the file source: components match without case.
void CheckDirectorySource(re2dj::test::Context& context)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() /
                                       ("re2dj-guest-dir-" + std::to_string(reinterpret_cast<std::uintptr_t>(&context)));
    std::error_code code;
    std::filesystem::remove_all(root, code);
    std::filesystem::create_directories(root / "ez2dj" / "Songs");
    {
        std::ofstream(root / "ez2dj" / "Songs" / "music.ini", std::ios::binary) << "[a]\r\n";
        std::ofstream(root / "ez2dj" / "Ez2DJ.exe", std::ios::binary) << "MZ";
    }
    hle::GuestFiles files;
    hle::GuestFileConfig config;
    config.hdd_directory = root;
    config.chd_root = "ez2dj";
    config.guest_root = "D:\\ez2dj";
    config.overlay_root = root / "overlay";
    std::string error;
    RE2DJ_CHECK(context, files.Configure(config, &error));
    const hle::GuestFiles::OpenResult opened = files.Open("SONGS\\MUSIC.INI", true, false, hle::kOpenExisting);
    RE2DJ_CHECK(context, opened.handle != 0);
    std::vector<std::uint8_t> bytes;
    files.Read(opened.handle, 16, &bytes);
    RE2DJ_CHECK_EQ(context, std::string(bytes.begin(), bytes.end()), std::string("[a]\r\n"));
    files.Close(opened.handle);
    bool outside = false;
    RE2DJ_CHECK_EQ(context, files.Attributes("songs", &outside).attributes, re2dj::storage::kFileAttributeDirectory);
    // Entries list with names compared without case.
    hle::GuestFiles::FindResult found = files.FindFirst("*");
    RE2DJ_CHECK_EQ(context, std::string(found.first.file_name), std::string("Ez2DJ.exe"));
    re2dj::storage::GuestFindData next;
    RE2DJ_CHECK_EQ(context, files.FindNext(found.handle, &next), hle::kWin32ErrorSuccess);
    RE2DJ_CHECK_EQ(context, std::string(next.file_name), std::string("Songs"));
    files.FindClose(found.handle);
    std::filesystem::remove_all(root, code);
}

// LoadImageA and the gdi32 calls 1st SE's bitmap loader makes, as measured on
// Windows 11 (task 421).
void CheckBitmapFiles(re2dj::test::Context& context)
{
    Fixture fixture;
    std::filesystem::create_directories(fixture.overlay);
    const auto write = [&](const char* name, const std::vector<std::uint8_t>& bytes) {
        std::ofstream file(fixture.overlay / name, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    };
    write("t24.bmp", re2dj::test::Bitmap24File());
    write("t8.bmp", re2dj::test::Bitmap8File());
    const std::string text = "XX not a bitmap at all, just some bytes to fill the header area.......";
    write("bad.bmp", std::vector<std::uint8_t>(text.begin(), text.end()));

    using re2dj::test::MemoryServices;
    MemoryServices services;
    services.SetFiles(&fixture.files);
    const auto user32 = hle::modules::MakeUser32ModuleDescriptor();
    const auto gdi32 = hle::modules::MakeGdi32ModuleDescriptor();
    const auto call = [&](const hle::modules::GuestModuleDescriptor& module, const char* name,
                          std::initializer_list<std::uint32_t> arguments, bool* handled = nullptr) {
        return re2dj::test::CallModuleExport(context, services, module, name, arguments, handled).eax;
    };
    constexpr std::uint32_t kName = MemoryServices::kBase + 0x10;
    constexpr std::uint32_t kObject = MemoryServices::kBase + 0x80;
    constexpr std::uint32_t kLoad = 0x2010;  // LR_LOADFROMFILE | LR_CREATEDIBSECTION

    // A 24-bit file: bottom-up bits with zeroed padding, the last error 0.
    services.Put(kName, "t24.bmp");
    services.SetLastError(1234);
    const std::uint32_t h24 = call(user32, "LoadImageA", {0, kName, 0, 0, 0, kLoad});
    RE2DJ_CHECK(context, h24 != 0);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "GetObjectA", {h24, 24, kObject}), 24U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject), 0U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 4), 3U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 8), 2U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 12), 12U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 16), 0x00180001U);
    const std::uint32_t bits = services.U32(kObject + 20);
    RE2DJ_CHECK(context, bits != 0 && (bits & 0xFFFU) == 0);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, services.U32(bits), 0x00FF0000U);
    RE2DJ_CHECK_EQ(context, services.U32(bits + 8), 0x00000000U);
    RE2DJ_CHECK_EQ(context, services.U32(bits + 12), 0x3C0A141EU);
    RE2DJ_CHECK_EQ(context, services.U32(bits + 20), 0x00000046U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "GetObjectA", {h24, 30, kObject}), 24U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "GetObjectA", {h24, 10, kObject}), 0U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "GetObjectA", {h24, 0, 0}), 24U);
    bool handled = true;
    call(gdi32, "GetObjectA", {h24, 84, kObject}, &handled);
    RE2DJ_CHECK(context, !handled);

    // Missing files and directories give 2; a file that is no BMP gives 0.
    services.Put(kName, "missing.bmp");
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(user32, "LoadImageA", {0, kName, 0, 0, 0, kLoad}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    services.Put(kName, "nodir\\missing.bmp");
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(user32, "LoadImageA", {0, kName, 0, 0, 0, kLoad}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorFileNotFound);
    services.Put(kName, "bad.bmp");
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(user32, "LoadImageA", {0, kName, 0, 0, 0, kLoad}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 0U);
    // Other loads are not modelled.
    services.Put(kName, "t24.bmp");
    handled = true;
    call(user32, "LoadImageA", {0, kName, 0, 0, 0, 0x10}, &handled);
    RE2DJ_CHECK(context, !handled);

    services.Put(kName, "t8.bmp");
    const std::uint32_t h8 = call(user32, "LoadImageA", {0, kName, 0, 0, 0, kLoad});
    RE2DJ_CHECK_EQ(context, call(gdi32, "GetObjectA", {h8, 24, kObject}), 24U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 12), 4U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 16), 0x00080001U);

    // A memory DC starts with the default 1x1 bitmap.
    services.SetLastError(1234);
    const std::uint32_t dc = call(gdi32, "CreateCompatibleDC", {0});
    const std::uint32_t dc2 = call(gdi32, "CreateCompatibleDC", {0});
    RE2DJ_CHECK(context, dc != 0 && dc2 != 0 && dc != dc2);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "GetObjectA", {hle::GuestGdi::kDefaultBitmap, 24, kObject}), 24U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 4), 1U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 12), 2U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 16), 0x00010001U);
    RE2DJ_CHECK_EQ(context, services.U32(kObject + 20), 0U);

    // SelectObject: the previous bitmap; 0 for one another DC holds, for an
    // unknown object (last error kept), and for a bogus DC (6).
    RE2DJ_CHECK_EQ(context, call(gdi32, "SelectObject", {dc, h24}), hle::GuestGdi::kDefaultBitmap);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SelectObject", {dc2, h24}), 0U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SelectObject", {dc, h24}), h24);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SelectObject", {0x12345678U, h24}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "SelectObject", {dc2, 0x12345678U}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);

    // An RGB565 surface DC, 8x8, filled with EEEE.
    hle::GuestProcess& process = *services.Process();
    constexpr std::uint32_t kSurface = MemoryServices::kBase + 0x2000;
    const auto fill = [&]() {
        for (std::uint32_t offset = 0; offset < 8 * 8 * 2; ++offset) services.Byte(kSurface + offset) = 0xEE;
    };
    const auto pixel = [&](std::uint32_t x, std::uint32_t y) {
        return static_cast<std::uint16_t>(services.Byte(kSurface + y * 16 + x * 2) |
                                          (services.Byte(kSurface + y * 16 + x * 2 + 1) << 8));
    };
    hle::GuestBitmap surface;
    surface.width = 8;
    surface.height = 8;
    surface.bits_per_pixel = 16;
    surface.pitch = 16;
    surface.bits = kSurface;
    surface.masks = {0xF800U, 0x07E0U, 0x001FU};
    hle::GuestDc surface_dc;
    surface_dc.bitmap = process.gdi().AddBitmap(surface);
    const std::uint32_t target = process.gdi().AddDc(surface_dc);

    // 1:1 from 24 bits: the screen's top row on top, channels truncated.
    fill();
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "StretchBlt", {target, 0, 0, 3, 2, dc, 0, 0, 3, 2, 0x00CC0020U}), 1U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
    RE2DJ_CHECK_EQ(context, pixel(0, 0), std::uint16_t{0x08A3});
    RE2DJ_CHECK_EQ(context, pixel(1, 0), std::uint16_t{0x2987});
    RE2DJ_CHECK_EQ(context, pixel(2, 0), std::uint16_t{0x428B});
    RE2DJ_CHECK_EQ(context, pixel(3, 0), std::uint16_t{0xEEEE});
    RE2DJ_CHECK_EQ(context, pixel(0, 1), std::uint16_t{0xF800});
    RE2DJ_CHECK_EQ(context, pixel(1, 1), std::uint16_t{0x07E0});
    RE2DJ_CHECK_EQ(context, pixel(2, 1), std::uint16_t{0x001F});
    RE2DJ_CHECK_EQ(context, pixel(0, 2), std::uint16_t{0xEEEE});
    // Pixels outside the destination are clipped.
    fill();
    RE2DJ_CHECK_EQ(context, call(gdi32, "StretchBlt", {target, 6, 7, 3, 2, dc, 0, 0, 3, 2, 0x00CC0020U}), 1U);
    RE2DJ_CHECK_EQ(context, pixel(6, 7), std::uint16_t{0x08A3});
    RE2DJ_CHECK_EQ(context, pixel(7, 7), std::uint16_t{0x2987});
    RE2DJ_CHECK_EQ(context, pixel(5, 7), std::uint16_t{0xEEEE});
    // Stretching is not modelled; bogus DCs fail with 6.
    handled = true;
    call(gdi32, "StretchBlt", {target, 0, 0, 4, 4, dc, 0, 0, 3, 2, 0x00CC0020U}, &handled);
    RE2DJ_CHECK(context, !handled);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "StretchBlt", {target, 0, 0, 3, 2, 0x12345678U, 0, 0, 3, 2, 0x00CC0020U}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), hle::kWin32ErrorInvalidHandle);

    // 8 bits through the color table.
    RE2DJ_CHECK_EQ(context, call(gdi32, "SelectObject", {dc, h8}), h24);
    fill();
    RE2DJ_CHECK_EQ(context, call(gdi32, "StretchBlt", {target, 0, 0, 2, 2, dc, 0, 0, 2, 2, 0x00CC0020U}), 1U);
    RE2DJ_CHECK_EQ(context, pixel(0, 0), std::uint16_t{0xC9AB});
    RE2DJ_CHECK_EQ(context, pixel(1, 0), std::uint16_t{0xF81F});
    RE2DJ_CHECK_EQ(context, pixel(0, 1), std::uint16_t{0x07E0});
    RE2DJ_CHECK_EQ(context, pixel(1, 1), std::uint16_t{0x07E0});

    // An unselected bitmap goes at once with its pages; a selected one waits
    // for its DC.
    RE2DJ_CHECK(context, process.PrivateCommitted(bits, 1));
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {h24}), 1U);
    RE2DJ_CHECK(context, !process.PrivateCommitted(bits, 1));
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {h24}), 0U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {h8}), 1U);
    RE2DJ_CHECK(context, process.gdi().FindBitmap(h8) != nullptr);
    services.SetLastError(1234);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteDC", {dc}), 1U);
    RE2DJ_CHECK(context, process.gdi().FindBitmap(h8) == nullptr);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteObject", {h8}), 0U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteDC", {dc}), 0U);
    RE2DJ_CHECK_EQ(context, call(gdi32, "DeleteDC", {0}), 0U);
    RE2DJ_CHECK_EQ(context, services.LastError(), 1234U);
}

}  // namespace

void RunGuestFilesTests(re2dj::test::Context& context)
{
    CheckBitmapFiles(context);
    CheckPrivateProfile(context);
    CheckProfileIntExport(context);
    CheckProfileStringExport(context);
    CheckDirectorySource(context);
    CheckPathsAndReads(context);
    CheckCopyOnWrite(context);
    CheckKernel32FileExports(context);
    CheckCurrentDirectory(context);
    CheckFind(context);
    CheckFileAttributes(context);
}
