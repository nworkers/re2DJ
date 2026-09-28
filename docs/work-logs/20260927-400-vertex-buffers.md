# 작업 400 작업 로그 — DX7 vertex buffer / Task 400 work log — DX7 vertex buffers

설계: [20260927-400-vertex-buffers.md](../design/20260927-400-vertex-buffers.md)
작업 지시서: [20260927-400-vertex-buffers.md](../work-orders/20260927-400-vertex-buffers.md)

## 진행 / Progress

`D3DERR_VERTEXBUFFERLOCKED`를 처음에 `0x887602CE`(718)로 적었다. Windows 빌드의 static_assert가 이를 잡았고, SDK를 확인해 `MAKE_DDHRESULT(2062)` = `0x8876080E`로 고쳤다.

*`D3DERR_VERTEXBUFFERLOCKED` was first written as `0x887602CE` (718); the Windows build's static_assert caught it, and the SDK gave `MAKE_DDHRESULT(2062)` = `0x8876080E`.*

Linux 실제 실행(x64, XTest로 코인 세 번과 시작 두 번)의 흐름은 다음과 같았다.

1. `IDirect3D7::CreateVertexBuffer` → 0
2. `IDirect3DVertexBuffer7::Lock(flags 1)` → 0
3. `Unlock` → 0
4. 곡 정보(`System\SongInfo\songinfo.str`)를 읽고, `temp.abm`은 없음(-1)
5. 표면을 만들고 DC를 받은 뒤, 호출 760,332번째의 `gdi32!CreateSolidBrush(0x007F0000)`에서 멈춤

Windows도 `temp.abm`을 찾지 못한다. Windows의 오류는 3인데, host 임시 디렉터리 구조에서 생긴 값이다. Linux는 디렉터리가 있으므로 2다.

*In a real Linux run (x64; XTest sent three coins and two starts) the flow was as above. Windows cannot find `temp.abm` either; its error 3 comes from the host temporary directory layout, while Linux, whose directory exists, reports 2.*

## 변경 / Changes

- **core**:
  - `re2dj/directx/direct3d_vertex_buffer.h`(.cpp).
  - `abi.h`: `kD3dErrVertexBufferLocked`, `kDdErrNotLocked`, `kIidDirect3DVertexBuffer7`, `kIidDirect3DVertexBuffer`. Windows에서 static_assert로 맞춘다.

  ***core:***
  - *`re2dj/directx/direct3d_vertex_buffer.h` (.cpp).*
  - *`abi.h`: `kD3dErrVertexBufferLocked`, `kDdErrNotLocked`, `kIidDirect3DVertexBuffer7`, and `kIidDirect3DVertexBuffer`, pinned by static_assert on Windows.*
- **Windows facade**: `D3dCreateVertexBuffer`, `VbLock`, `VbUnlock`, `VbGetVertexBufferDesc`, `DeviceDrawPrimitiveVB`가 core 판정을 부른다. / ***Windows facade:** `D3dCreateVertexBuffer`, `VbLock`, `VbUnlock`, `VbGetVertexBufferDesc`, and `DeviceDrawPrimitiveVB` call the core's checks.*
- **Linux ddraw.dll**:
  - `ddraw_vertex_buffer7.cpp`(`IDirect3DVertexBuffer7` 메서드 9개, 버퍼 생성, `VertexBufferOf`).
  - `IDirect3D7::CreateVertexBuffer`.
  - `DrawVertices`, `DrawPrimitiveVB`, `DrawIndexedPrimitiveVB`.

  ***Linux ddraw.dll:***
  - *`ddraw_vertex_buffer7.cpp` (the nine `IDirect3DVertexBuffer7` methods, buffer creation, and `VertexBufferOf`).*
  - *`IDirect3D7::CreateVertexBuffer`.*
  - *`DrawVertices`, `DrawPrimitiveVB`, and `DrawIndexedPrimitiveVB`.*
- **테스트 / tests**:
  - core 판정과 Windows IID 대조(`directx_device_test.cpp`).
  - `ddraw_module_test.cpp`:
    - 생성 거절 4가지, 설명, QueryInterface, Optimize
    - 잠금 두 번과 잠긴 draw
    - `DrawPrimitiveVB`와 거절 3가지
    - index 그리기와 거절 3가지
    - 해제
    - export 개수 145

  ***Tests:***
  - *The core checks and the Windows IID comparison (`directx_device_test.cpp`).*
  - *`ddraw_module_test.cpp`:*
    - *four creation refusals, the description, QueryInterface, and Optimize*
    - *a double lock and a locked draw*
    - *`DrawPrimitiveVB` with three refusals*
    - *indexed draws with three refusals*
    - *release*
    - *an export count of 145*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `5059b00`와 60초씩 / real 4th vs base `5059b00`, 60 s each | ddraw 1,882줄, vertex buffer 기록 5줄이 같다 / *the 1,882 ddraw lines and the five vertex buffer lines match* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 4/4(unit checks 4,293) / *no warnings or errors, 4/4 each (4,293 unit checks)* |
| Linux `--call-limit 32768`, 두 폭 / both widths | 호출 32,768번에서 멈춤, hardlock 83 / *stops at 32,768 calls, hardlock 83* |
| 실제 4th, 코인·시작 / real 4th, coins and start | vertex buffer 생성·잠금을 지나 곡 정보 화면의 `CreateSolidBrush`에서 멈춤 / *past vertex buffer creation and locking, stopping at `CreateSolidBrush` on the song info screen* |

## 다음 / Next

표면 DC에 그리는 GDI다(`CreateSolidBrush`와 뒤따르는 브러시·글꼴·텍스트).

*Next is GDI drawing into surface DCs (`CreateSolidBrush` and the brushes, fonts, and text that follow).*
