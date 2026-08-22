#ifndef RE2DJ_RUNTIME_PE_LOADER_H_
#define RE2DJ_RUNTIME_PE_LOADER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "re2dj/exe/pe_image.h"
#include "re2dj/runtime/address_space.h"

namespace re2dj::runtime
{

inline constexpr std::uint32_t kDefaultImportGateBase = 0xF0000000;
inline constexpr std::uint32_t kDefaultImportGateStride = 16;

struct ImportGate
{
    std::string module;
    std::string name;
    std::uint16_t ordinal = 0;
    bool by_ordinal = false;
    GuestAddress address;
};

class ImportGateTable
{
public:
    explicit ImportGateTable(
        GuestAddress base = GuestAddress(kDefaultImportGateBase),
        std::uint32_t stride = kDefaultImportGateStride);

    bool BindByName(std::string_view module,
                    std::string_view name,
                    GuestAddress* address,
                    std::string* error);
    bool BindByOrdinal(std::string_view module,
                       std::uint16_t ordinal,
                       GuestAddress* address,
                       std::string* error);

    const std::vector<ImportGate>& gates() const
    {
        return gates_;
    }

private:
    bool Bind(std::string_view module,
              std::string_view name,
              std::uint16_t ordinal,
              bool by_ordinal,
              GuestAddress* address,
              std::string* error);

    GuestAddress base_;
    std::uint32_t stride_ = 0;
    std::vector<ImportGate> gates_;
};

struct LoadedPeImage
{
    GuestAddress load_base;
    GuestAddress entry_point;
    std::uint32_t tls_directory_rva = 0;
    std::uint32_t tls_directory_size = 0;
    std::vector<ImportGate> imports;
};

// Maps a complete PE32 file image into guest memory. A zero requested base uses
// the preferred ImageBase. The operation is transactional: failures preserve
// the supplied address space and gate table.
bool LoadPe32Image(const std::uint8_t* file_bytes,
                   std::size_t file_size,
                   const exe::PeImageInfo& info,
                   GuestAddress requested_base,
                   AddressSpace* address_space,
                   ImportGateTable* gates,
                   LoadedPeImage* loaded,
                   std::string* error);

}  // namespace re2dj::runtime

#endif  // RE2DJ_RUNTIME_PE_LOADER_H_
