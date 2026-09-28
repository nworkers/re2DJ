# 작업 400 설계 — DX7 vertex buffer / Task 400 design — DX7 vertex buffers

선행: [작업 391 설계](20260926-391-directx-drawing.md), [작업 399 설계](20260927-399-find-files.md)

## 배경 / Background

작업 399 뒤 Linux의 4th는 두 경로에서 모두 `IDirect3D7::CreateVertexBuffer`에서 멈췄다. 하나는 데모 진행이고, 다른 하나는 코인·시작 뒤의 게임 화면 준비다.

Windows facade의 DX7 vertex buffer는 DX6 구현(`direct3d3_com_facade.cpp`)을 그대로 쓴다.
- 저장소는 공용 `graphics::LegacyVertexBuffer`다.
- Lock은 버퍼 전체를 준다.
- `DrawPrimitiveVB`는 범위와 잠금을 확인한 뒤 일반 `DrawPrimitive` 경로로 그린다.
- `DrawIndexedPrimitiveVB`는 index를 펼쳐 같은 경로로 그린다.

판정 규칙은 Windows facade 안에만 있었다.

*After Task 399 the 4th on Linux stopped at `IDirect3D7::CreateVertexBuffer` along both of its paths: the demo, and gameplay setup after coins and start.*

*The Windows facade's DX7 vertex buffers reuse the DX6 implementation (`direct3d3_com_facade.cpp`):*
- *The storage is the shared `graphics::LegacyVertexBuffer`.*
- *A lock gives the whole buffer.*
- *`DrawPrimitiveVB` checks the range and the lock, then draws through the ordinary `DrawPrimitive` path.*
- *`DrawIndexedPrimitiveVB` expands its indices and draws the same way.*

*The rules lived only inside the Windows facade.*

## 결정 / Decisions

1. **규칙은 core로.** `re2dj/directx/direct3d_vertex_buffer.h`에 `D3dVertexBufferDesc`와 다음 판정을 둔다.

   | 판정 / Check | 규칙 / Rule |
   | --- | --- |
   | `CheckCreateVertexBuffer` | out·설명이 없거나, 크기가 16 미만이거나, stride 없는 FVF면 `DDERR_INVALIDPARAMS` / *no out or description, a size under 16, or an FVF with no stride* |
   | `CheckLockVertexBuffer` | 데이터 포인터가 없으면 `INVALIDPARAMS`, 이미 잠겼으면 `D3DERR_VERTEXBUFFERLOCKED` / *no data pointer; already locked* |
   | `CheckUnlockVertexBuffer` | 잠기지 않았으면 `DDERR_NOTLOCKED` / *not locked* |
   | `CheckGetVertexBufferDesc` | 설명이 없거나 16 미만이면 `INVALIDPARAMS` / *no description, or under 16* |
   | `CheckDrawPrimitiveVB` | 개수 0이면 `INVALIDPARAMS`, 잠겼으면 `VERTEXBUFFERLOCKED`, 범위 밖이면 `INVALIDPARAMS` / *zero count; locked; out of range* |
   | `CheckDrawIndexedPrimitiveVB` | 시작 vertex가 0이 아니면 `UNSUPPORTED`, index 없음·0개면 `INVALIDPARAMS`, 잠겼으면 `VERTEXBUFFERLOCKED` / *nonzero start; no or zero indices; locked* |

   - `D3DERR_VERTEXBUFFERLOCKED`(`0x8876080E`), `DDERR_NOTLOCKED`는 SDK 값과 static_assert로 맞춘다. 처음 적은 값이 틀렸는데 이 static_assert가 잡았다.
   - IID(`IDirect3DVertexBuffer7`, `IDirect3DVertexBuffer`)는 Windows 단위 테스트가 dxguid와 대조한다.
   - Windows facade는 생성, Lock, Unlock, 설명, `DrawPrimitiveVB`의 잠금·범위에서 core를 부른다.

   ***The rules move to the core.** `re2dj/directx/direct3d_vertex_buffer.h` holds `D3dVertexBufferDesc` and the checks tabulated above.*
   - *`D3DERR_VERTEXBUFFERLOCKED` (`0x8876080E`) and `DDERR_NOTLOCKED` are pinned to the SDK by static_assert, which caught a wrong first value.*
   - *The IIDs (`IDirect3DVertexBuffer7`, `IDirect3DVertexBuffer`) are checked against dxguid by a Windows unit test.*
   - *The Windows facade calls the core for creation, Lock, Unlock, the description, and `DrawPrimitiveVB`'s lock and range.*
2. **Linux `IDirect3DVertexBuffer7`.** vertex는 0으로 채운 guest 메모리에 둔다. Lock은 그 주소와 크기를 준다. 출력은 Windows facade처럼 먼저 0으로 지운다.
   - QueryInterface는 IUnknown, VB7, VB에 자기 자신으로 답한다.
   - `ProcessVertices`, `Optimize`, `ProcessVerticesStrided`는 `E_NOTIMPL`(`DDERR_UNSUPPORTED`와 같은 값)이다.
   - 버퍼는 Direct3D 객체의 참조를 갖는다. Windows의 root 참조와 같다.

   ***Linux `IDirect3DVertexBuffer7`.** Vertices live in zeroed guest memory; a lock gives its address and size, with the outputs cleared first as in the Windows facade.*
   - *QueryInterface answers IUnknown, VB7, and VB with the object itself.*
   - *`ProcessVertices`, `Optimize`, and `ProcessVerticesStrided` are `E_NOTIMPL` (the value of `DDERR_UNSUPPORTED`).*
   - *A buffer holds its Direct3D object, as the Windows one holds its root.*
3. **Linux VB 그리기.** `DrawPrimitive`의 읽기 뒤 부분(decode, 변환, 텍스처, 상태, fade, host Draw)을 `DrawVertices`로 뺐다. 세 draw가 이것을 함께 쓴다.
   - `DrawPrimitiveVB`: 시작 vertex부터 읽는다.
   - `DrawIndexedPrimitiveVB`: 버퍼 전체와 index를 읽어 공용 `ExpandIndexedVertices`로 펼친다.
   - 이 장치의 DirectDraw 객체에 속하지 않은 버퍼는 `INVALIDPARAMS`다. Windows의 foreign 판정과 같다.

   ***Linux VB draws.** What `DrawPrimitive` does after reading (decode, transform, texture, state, fade, the host's Draw) becomes `DrawVertices`, which all three draws share:*
   - *`DrawPrimitiveVB` reads from the start vertex.*
   - *`DrawIndexedPrimitiveVB` reads the whole buffer and the indices and expands them with the shared `ExpandIndexedVertices`.*
   - *A buffer not of this device's DirectDraw object is `INVALIDPARAMS`, as Windows' foreign check.*

## 범위 밖 / Out of scope

- `ProcessVertices` 계열의 실제 변환, DX6 `IDirect3D3` 쪽. / *Real `ProcessVertices` transforms and the DX6 `IDirect3D3` side.*
- `gdi32!CreateSolidBrush`: 곡 정보 화면에서 표면 DC에 그리는 GDI다. 다음 작업이다. / *`gdi32!CreateSolidBrush`, GDI drawing into a surface DC on the song info screen: the next task.*
