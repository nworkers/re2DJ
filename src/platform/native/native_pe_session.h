#ifndef RE2DJ_PLATFORM_NATIVE_NATIVE_PE_SESSION_H_
#define RE2DJ_PLATFORM_NATIVE_NATIVE_PE_SESSION_H_

#include <cstdint>
#include <string>
#include <vector>

#include "native_import_thunks.h"
#include "native_pe_image.h"
#include "native_process_bootstrap.h"

namespace re2dj::platform::native
{

class NativePeSession
{
public:
    NativePeSession() = default;
    ~NativePeSession();

    NativePeSession(const NativePeSession&) = delete;
    NativePeSession& operator=(const NativePeSession&) = delete;

    bool Prepare(const std::vector<std::uint8_t>& file,
                 const exe::PeImageInfo& info,
                 std::uint32_t requested_base,
                 std::uintptr_t bridge_address,
                 std::uintptr_t cleanup_address,
                 std::string* error);
    bool RunTlsCallbacks(NativeGuestFault* fault, std::string* error);
    bool RunEntry(std::uint32_t* result, NativeGuestFault* fault, std::string* error);
    void Release();

    const NativePeImage& image() const;
    const NativeProcessBootstrap& bootstrap() const;
    const runtime::ImportGateTable& gates() const;
    runtime::ImportGateTable* mutable_gates();
    NativeImportThunkRegion* mutable_import_thunks();

private:
    NativePeImage image_;
    NativeImportThunkRegion thunks_;
    NativeProcessBootstrap bootstrap_;
    runtime::ImportGateTable gates_;
    const exe::PeImageInfo* info_ = nullptr;
};

}  // namespace re2dj::platform::native

#endif  // RE2DJ_PLATFORM_NATIVE_NATIVE_PE_SESSION_H_
