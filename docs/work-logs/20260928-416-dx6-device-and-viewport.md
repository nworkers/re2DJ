# 작업 416 작업 로그 — DX6 장치·표면·viewport를 한 묶음으로 / Task 416 work log — the DX6 device, surfaces, and viewport in one batch

설계: [20260928-416-dx6-device-and-viewport.md](../design/20260928-416-dx6-device-and-viewport.md) · 지시서: [20260928-416-dx6-device-and-viewport.md](../work-orders/20260928-416-dx6-device-and-viewport.md)

## 2026-09-28

- Linux 1st를 반복 실행하며 멈춘 순서대로 구현했다.
  1. `IDirect3D3::EnumZBufferFormats`
  2. `IDirectDraw4::CreateSurface`, 그리고 `IDirectDrawSurface4` vtable
  3. `IDirect3D3::CreateDevice`, 그리고 `IDirect3DDevice3` vtable
  4. `IDirect3D3::CreateViewport`, 그리고 `IDirect3DViewport3` vtable
  5. `IDirect3DDevice3::AddViewport`, `IDirect3DViewport3::SetViewport2`, `IDirect3DDevice3::SetCurrentViewport`
  6. `IDirect3DDevice3::GetCaps`

  `EnumTextureFormats`는 DX7 handler를 공유해서 따로 멈추지 않았다.

  *Linux 1st was run repeatedly and each stop implemented in order: (1) `IDirect3D3::EnumZBufferFormats`; (2) `IDirectDraw4::CreateSurface` with the `IDirectDrawSurface4` vtable; (3) `IDirect3D3::CreateDevice` with the `IDirect3DDevice3` vtable; (4) `IDirect3D3::CreateViewport` with the `IDirect3DViewport3` vtable; (5) `IDirect3DDevice3::AddViewport`, `IDirect3DViewport3::SetViewport2`, `IDirect3DDevice3::SetCurrentViewport`; (6) `IDirect3DDevice3::GetCaps`. `EnumTextureFormats` shares the DX7 handler and did not stop.*
- 결과: Windows x86 CTest 6개 통과(단위 4853 checks, SDK 배치 `static_assert` 포함), Linux x64·x86 CTest 4개 통과(단위 4850 checks), 실패 0.
  *Windows x86: all 6 CTest tests pass (4853 unit checks, the SDK layout `static_assert`s included); Linux x64 and x86: all 4 CTest tests pass (4850 unit checks); no failures.*
- Windows facade의 `EnumZBufferFormats`, `GetCaps`, viewport 변환이 core를 쓰게 바뀌었으므로 1st를 30초 실행했다. runtime 기록은 전처럼 `FindDevice` → `CreateDevice` → `DrawPrimitive` 순서이고 오류가 없다.
  *As the Windows facade's `EnumZBufferFormats`, `GetCaps`, and viewport transform now use the core, 1st ran for 30 s; its runtime log still goes `FindDevice` → `CreateDevice` → `DrawPrimitive`, without errors.*
- Linux 1st(x64·x86 모두): DX6 초기화를 모두 지난다. 이어서 winmm mixer와 DirectSound 초기화를 거친 뒤, 호출 1027번째 `kernel32.dll!CreateThread`에서 멈춘다. DirectX 계열이 아니므로 이번 묶음은 여기서 끝낸다.
  *Linux 1st (both x64 and x86) gets through DX6 initialisation, then winmm mixer and DirectSound set-up, and stops at `kernel32.dll!CreateThread` (call 1027). That is not DirectX, so the batch ends here.*
