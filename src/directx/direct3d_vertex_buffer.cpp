#include "re2dj/directx/direct3d_vertex_buffer.h"

#include "re2dj/graphics/legacy_vertex_buffer.h"

namespace re2dj::directx
{

std::uint32_t CheckCreateVertexBuffer(bool has_out, bool has_description, const D3dVertexBufferDesc& description,
                                      std::uint32_t* stride)
{
    if (!has_out || !has_description || description.size < sizeof(D3dVertexBufferDesc))
    {
        return kDdErrInvalidParams;
    }
    graphics::LegacyVertexBufferDesc legacy;
    legacy.size = sizeof(D3dVertexBufferDesc);
    legacy.caps = description.caps;
    legacy.fvf = description.fvf;
    legacy.vertex_count = description.vertex_count;
    const auto buffer = graphics::LegacyVertexBuffer::Create(legacy);
    if (buffer == nullptr)
    {
        return kDdErrInvalidParams;
    }
    *stride = buffer->stride();
    return kDdOk;
}

std::uint32_t CheckLockVertexBuffer(bool has_data, bool locked, std::uint32_t bytes)
{
    if (!has_data)
    {
        return kDdErrInvalidParams;
    }
    return locked || bytes == 0 ? kD3dErrVertexBufferLocked : kDdOk;
}

std::uint32_t CheckUnlockVertexBuffer(bool locked)
{
    return locked ? kDdOk : kDdErrNotLocked;
}

std::uint32_t CheckGetVertexBufferDesc(bool has_description, std::uint32_t size)
{
    return has_description && size >= sizeof(D3dVertexBufferDesc) ? kDdOk : kDdErrInvalidParams;
}

std::uint32_t CheckDrawPrimitiveVB(std::uint32_t vertex_count,
                                   bool locked,
                                   std::uint32_t start_vertex,
                                   std::uint32_t buffer_vertices)
{
    if (vertex_count == 0)
    {
        return kDdErrInvalidParams;
    }
    if (locked)
    {
        return kD3dErrVertexBufferLocked;
    }
    if (start_vertex > buffer_vertices || vertex_count > buffer_vertices - start_vertex)
    {
        return kDdErrInvalidParams;
    }
    return kDdOk;
}

std::uint32_t CheckDrawIndexedPrimitiveVB(std::uint32_t start_vertex,
                                          bool has_indices,
                                          std::uint32_t index_count,
                                          bool locked)
{
    if (start_vertex != 0)
    {
        return kDdErrUnsupported;
    }
    if (!has_indices || index_count == 0)
    {
        return kDdErrInvalidParams;
    }
    return locked ? kD3dErrVertexBufferLocked : kDdOk;
}

}  // namespace re2dj::directx
