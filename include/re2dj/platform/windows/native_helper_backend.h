#ifndef RE2DJ_PLATFORM_WINDOWS_NATIVE_HELPER_BACKEND_H_
#define RE2DJ_PLATFORM_WINDOWS_NATIVE_HELPER_BACKEND_H_

#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include "re2dj/runtime/execution_backend.h"

namespace re2dj::platform::windows
{

class NativeHelperBackend final : public runtime::ExecutionBackend
{
public:
    explicit NativeHelperBackend(std::filesystem::path helper_path);
    ~NativeHelperBackend() override;

    NativeHelperBackend(const NativeHelperBackend&) = delete;
    NativeHelperBackend& operator=(const NativeHelperBackend&) = delete;

    bool PrepareImage(std::span<const std::uint8_t> file_bytes,
                      const exe::PeImageInfo& info,
                      runtime::GuestAddress requested_base,
                      runtime::LoadedPeImage* loaded,
                      std::string* error) override;
    bool Start(std::string* error) override;
    bool WaitForEvent(runtime::ExecutionEvent* event, std::string* error) override;
    bool ReadMemory(runtime::GuestAddress address,
                    std::span<std::uint8_t> bytes,
                    std::string* error) override;
    bool WriteMemory(runtime::GuestAddress address,
                     std::span<const std::uint8_t> bytes,
                     std::string* error) override;
    bool CompleteImport(const runtime::ImportCompletion& completion,
                        std::string* error) override;
    void RequestStop() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_NATIVE_HELPER_BACKEND_H_
