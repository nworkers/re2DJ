#ifndef RE2DJ_RUNTIME_ADDRESS_SPACE_H_
#define RE2DJ_RUNTIME_ADDRESS_SPACE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace re2dj::runtime
{

inline constexpr std::uint32_t kGuestPageSize = 4096;

class GuestAddress
{
public:
    constexpr GuestAddress() = default;
    constexpr explicit GuestAddress(std::uint32_t value) : value_(value) {}

    constexpr std::uint32_t value() const
    {
        return value_;
    }

    constexpr GuestAddress operator+(std::uint32_t offset) const
    {
        return GuestAddress(value_ + offset);
    }

    constexpr bool operator==(GuestAddress other) const
    {
        return value_ == other.value_;
    }

    constexpr bool operator!=(GuestAddress other) const
    {
        return !(*this == other);
    }

private:
    std::uint32_t value_ = 0;
};

enum class MemoryAccess : std::uint8_t
{
    kNone = 0,
    kRead = 1,
    kWrite = 2,
    kExecute = 4,
};

constexpr MemoryAccess operator|(MemoryAccess left, MemoryAccess right)
{
    return static_cast<MemoryAccess>(static_cast<std::uint8_t>(left) |
                                     static_cast<std::uint8_t>(right));
}

class AddressSpace
{
public:
    bool Map(GuestAddress base, std::uint32_t size, MemoryAccess access, std::string* error);
    bool IsMapped(GuestAddress address, std::uint32_t size, MemoryAccess access) const;

    bool Read8(GuestAddress address, std::uint8_t* value) const;
    bool Read16(GuestAddress address, std::uint16_t* value) const;
    bool Read32(GuestAddress address, std::uint32_t* value) const;
    bool Write8(GuestAddress address, std::uint8_t value);
    bool Write16(GuestAddress address, std::uint16_t value);
    bool Write32(GuestAddress address, std::uint32_t value);
    bool WriteBytes(GuestAddress address, const std::uint8_t* bytes, std::size_t size);

private:
    struct Region
    {
        std::uint32_t base = 0;
        std::vector<std::uint8_t> bytes;
        MemoryAccess access = MemoryAccess::kNone;
    };

    const Region* Find(GuestAddress address, std::uint32_t size, MemoryAccess access) const;
    Region* Find(GuestAddress address, std::uint32_t size, MemoryAccess access);

    std::vector<Region> regions_;
};

}  // namespace re2dj::runtime

#endif  // RE2DJ_RUNTIME_ADDRESS_SPACE_H_
