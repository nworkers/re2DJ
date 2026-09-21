#include <cstdlib>
#include <cstdint>
#include <fstream>

#include <errno.h>
#include <unistd.h>

#include "../native_helper_protocol.h"

namespace
{

namespace protocol = re2dj::platform::native_protocol;

bool ReadExact(void* destination, std::uint32_t size)
{
    auto* bytes = static_cast<std::uint8_t*>(destination);
    std::uint32_t consumed = 0;
    while (consumed < size)
    {
        const ssize_t result = read(STDIN_FILENO, bytes + consumed, size - consumed);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result <= 0)
        {
            return false;
        }
        consumed += static_cast<std::uint32_t>(result);
    }
    return true;
}

bool WriteExact(const void* source, std::uint32_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    std::uint32_t consumed = 0;
    while (consumed < size)
    {
        const ssize_t result = write(STDOUT_FILENO, bytes + consumed, size - consumed);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result <= 0)
        {
            return false;
        }
        consumed += static_cast<std::uint32_t>(result);
    }
    return true;
}

bool ReceiveHeader(protocol::MessageHeader* header)
{
    return ReadExact(header, sizeof(*header)) && header->magic == protocol::kMagic &&
           header->version == protocol::kVersion &&
           header->payload_size <= protocol::kMaximumPayloadSize;
}

bool SendHelloResult()
{
    protocol::MessageHeader header;
    header.type = static_cast<std::uint32_t>(protocol::MessageType::kHelloResult);
    header.payload_size = sizeof(protocol::HelloResult);
    protocol::HelloResult result;
    result.supported_features = protocol::kFeatureImportMetadata;
    return WriteExact(&header, sizeof(header)) && WriteExact(&result, sizeof(result));
}

bool WriteStatus(const char* status)
{
    const char* path = std::getenv("RE2DJ_CAPABILITY_REJECTION_STATUS");
    if (path == nullptr)
    {
        return false;
    }
    std::ofstream output(path);
    output << status << '\n';
    return output.good();
}

}  // namespace

int main()
{
    protocol::MessageHeader header;
    if (!ReceiveHeader(&header) ||
        header.type != static_cast<std::uint32_t>(protocol::MessageType::kHello) ||
        header.payload_size != sizeof(protocol::HelloRequest))
    {
        return 1;
    }
    protocol::HelloRequest request;
    if (!ReadExact(&request, sizeof(request)) || !SendHelloResult())
    {
        return 2;
    }
    if (!ReceiveHeader(&header))
    {
        return WriteStatus("eof-before-load-image") ? 0 : 3;
    }
    if (header.type == static_cast<std::uint32_t>(protocol::MessageType::kLoadImage))
    {
        WriteStatus("unexpected-load-image");
        return 4;
    }
    WriteStatus("unexpected-packet");
    return 5;
}
