#include "native_pe_session.h"

#include <cstring>

namespace re2dj::platform::linux
{
namespace
{

std::uint32_t ReadU32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

}  // namespace

NativePeSession::~NativePeSession()
{
    Release();
}

bool NativePeSession::Prepare(const std::vector<std::uint8_t>& file,
                              const exe::PeImageInfo& info,
                              std::uint32_t requested_base,
                              std::uintptr_t bridge_address,
                              std::uintptr_t cleanup_address,
                              std::string* error)
{
    if (error == nullptr || info_ != nullptr ||
        !MapNativePe32Image(file, info, requested_base, &image_) ||
        !BindNativeImportThunks(info, image_.memory, image_.size, bridge_address,
                                cleanup_address, &gates_, &thunks_, error) ||
        !bootstrap_.Initialize(requested_base, error))
    {
        Release();
        if (error != nullptr && error->empty())
        {
            *error = "cannot prepare native PE session";
        }
        return false;
    }
    info_ = &info;
    return true;
}

bool NativePeSession::RunTlsCallbacks(NativeGuestFault* fault, std::string* error)
{
    if (info_ == nullptr || fault == nullptr || error == nullptr)
    {
        if (error != nullptr) *error = "invalid native PE session TLS arguments";
        return false;
    }
    const auto* directory = info_->Directory(exe::PeDirectoryIndex::kTls);
    if (directory == nullptr || directory->virtual_address == 0 || directory->size == 0)
    {
        return true;
    }
    if (directory->size < 24 || directory->virtual_address > image_.size ||
        24 > image_.size - directory->virtual_address)
    {
        *error = "invalid native PE TLS directory";
        return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(image_.memory);
    const std::uint32_t callbacks = ReadU32(bytes + directory->virtual_address + 12);
    const std::uint32_t base = image_.entry_point - info_->entry_point_rva;
    if (callbacks == 0)
    {
        return true;
    }
    if (callbacks < base || callbacks - base >= image_.size)
    {
        *error = "invalid native PE TLS callbacks";
        return false;
    }
    for (std::uint32_t index = 0; index <= image_.size / 4; ++index)
    {
        const std::uint32_t rva = callbacks - base + index * 4;
        if (rva > image_.size || 4 > image_.size - rva)
        {
            *error = "invalid native PE TLS callback table";
            return false;
        }
        const std::uint32_t callback = ReadU32(bytes + rva);
        if (callback == 0)
        {
            return true;
        }
        if (callback < base || callback - base >= image_.size ||
            !bootstrap_.RunTlsCallback(callback, base, fault, error))
        {
            if (error->empty()) *error = "invalid native PE TLS callback";
            return false;
        }
    }
    *error = "unterminated native PE TLS callback table";
    return false;
}

bool NativePeSession::RunEntry(std::uint32_t* result, NativeGuestFault* fault, std::string* error)
{
    if (info_ == nullptr)
    {
        if (error != nullptr) *error = "native PE session is not prepared";
        return false;
    }
    return bootstrap_.RunEntry(image_.entry_point, result, fault, error);
}

void NativePeSession::Release()
{
    ReleaseNativeImportThunks(&thunks_);
    ReleaseNativePeImage(&image_);
    gates_ = runtime::ImportGateTable(runtime::GuestAddress(runtime::kDefaultImportGateBase),
                                      runtime::kDefaultImportGateStride);
    info_ = nullptr;
}

const NativePeImage& NativePeSession::image() const { return image_; }
const NativeProcessBootstrap& NativePeSession::bootstrap() const { return bootstrap_; }
const runtime::ImportGateTable& NativePeSession::gates() const { return gates_; }

}  // namespace re2dj::platform::linux
