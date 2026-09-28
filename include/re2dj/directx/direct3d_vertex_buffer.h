#ifndef RE2DJ_DIRECTX_DIRECT3D_VERTEX_BUFFER_H_
#define RE2DJ_DIRECTX_DIRECT3D_VERTEX_BUFFER_H_

#include <cstdint>

#include "re2dj/directx/abi.h"

// Direct3D vertex buffers as both hosts answer for them: the Windows
// facade's rules for IDirect3D7::CreateVertexBuffer, IDirect3DVertexBuffer7,
// and the device's DrawPrimitiveVB and DrawIndexedPrimitiveVB. A buffer's
// storage is the shared graphics::LegacyVertexBuffer on Windows and guest
// memory on Linux; a lock always covers the whole buffer.
namespace re2dj::directx
{

// D3DVERTEXBUFFERDESC.
struct D3dVertexBufferDesc
{
    std::uint32_t size = 0;
    std::uint32_t caps = 0;
    std::uint32_t fvf = 0;
    std::uint32_t vertex_count = 0;
};
static_assert(sizeof(D3dVertexBufferDesc) == 16);

// CreateVertexBuffer: no out pointer, no description, a description smaller
// than D3DVERTEXBUFFERDESC, or a format without a vertex stride is
// DDERR_INVALIDPARAMS. On success, stride is the vertex size in bytes.
std::uint32_t CheckCreateVertexBuffer(bool has_out, bool has_description, const D3dVertexBufferDesc& description,
                                      std::uint32_t* stride);

// Lock: no data pointer is DDERR_INVALIDPARAMS, a buffer already locked (or
// with no storage) D3DERR_VERTEXBUFFERLOCKED. Unlock: one not locked is
// DDERR_NOTLOCKED.
std::uint32_t CheckLockVertexBuffer(bool has_data, bool locked, std::uint32_t bytes);
std::uint32_t CheckUnlockVertexBuffer(bool locked);

// GetVertexBufferDesc: no description, or one smaller than
// D3DVERTEXBUFFERDESC, is DDERR_INVALIDPARAMS. The buffer reports its own
// description with dwSize the structure's.
std::uint32_t CheckGetVertexBufferDesc(bool has_description, std::uint32_t size);

// DrawPrimitiveVB: no vertices (or no buffer of the device) is
// DDERR_INVALIDPARAMS, a locked buffer D3DERR_VERTEXBUFFERLOCKED, and a range
// past the buffer's end DDERR_INVALIDPARAMS. What remains draws as
// DrawPrimitive draws the same vertices.
std::uint32_t CheckDrawPrimitiveVB(std::uint32_t vertex_count,
                                   bool locked,
                                   std::uint32_t start_vertex,
                                   std::uint32_t buffer_vertices);

// DrawIndexedPrimitiveVB: a start vertex other than 0 is DDERR_UNSUPPORTED
// (the facade indexes the whole buffer); no indices or none of them (or no
// buffer of the device) DDERR_INVALIDPARAMS; a locked buffer
// D3DERR_VERTEXBUFFERLOCKED. The indexed vertices then draw as
// DrawPrimitive draws them; an index past the buffer is DDERR_INVALIDPARAMS.
std::uint32_t CheckDrawIndexedPrimitiveVB(std::uint32_t start_vertex,
                                          bool has_indices,
                                          std::uint32_t index_count,
                                          bool locked);

}  // namespace re2dj::directx

#endif  // RE2DJ_DIRECTX_DIRECT3D_VERTEX_BUFFER_H_
