# 작업 374 작업 로그 — 공용 DirectX core와 첫 단계 / Task 374 work log — shared DirectX core, first phase

설계: [20260926-374-shared-directx-core.md](../design/20260926-374-shared-directx-core.md)
작업 지시서: [20260926-374-shared-directx-core.md](../work-orders/20260926-374-shared-directx-core.md)

## 진행 / Progress

작업 373 끝에 Direct3D 7 구현 방식을 사용자에게 물었고, 공용 core로 옮기기로 정했다. Windows facade의 구조를 읽었다. 진단, 호스트 창, SDL3/OpenGL backend, GDI DIB 표면이 한 파일에 얽혀 있어서 층별로 나누었다. 먼저 Linux가 다음으로 닿는 설명 층을 옮겼다.

*At the end of Task 373 the user was asked how to implement Direct3D 7 and chose the shared core. Reading the Windows facade showed diagnostics, the host window, the SDL3/OpenGL backend, and GDI DIB surfaces intertwined in one file, so the move was split by layer, starting with the descriptions Linux reaches next.*

Windows build에서 `static_assert`가 core의 `DDCAPS2_NOPAGELOCKREQUIRED` 값(`0x4000`)이 SDK(`0x0800`)와 다름을 잡았다. SDK header로 확인해 고쳤다. 나머지 상수, 크기, offset은 모두 일치했다.

*On the Windows build a `static_assert` caught the core's `DDCAPS2_NOPAGELOCKREQUIRED` (`0x4000`) disagreeing with the SDK (`0x0800`); checked against the SDK header and fixed. Every other constant, size, and offset matched.*

Linux에서는 `IDirect3D7`을 만들자 게임이 장치 설명을 `lstrcpynA`로 복사하려다 멈췄다. `lstrcpynA`의 경계를 측정해 구현했다. 그러자 진단의 `ReadGuestString`이 heap 문자열을 읽지 못해 실패했다. 이 함수는 image와 stack만 읽었다. `ReadGuestBytes`와 같은 범위를 쓰게 고쳤다. 그 뒤 두 드라이버 열거가 모두 통과했고, `ShowCursor(FALSE)` 다음 `SetCooperativeLevel`에서 멈췄다.

*On Linux, with `IDirect3D7` in place the game stopped copying a device description with `lstrcpynA`; its edge cases were measured and implemented. The diagnostic's `ReadGuestString` then failed on the heap string, as it read only the image and stack; it now uses `ReadGuestBytes`'s ranges. Both driver enumerations then completed, and after `ShowCursor(FALSE)` the run stopped at `SetCooperativeLevel`.*

## 변경 / Changes

- **`re2dj_directx`**(새 library): `abi.h`, `directdraw_description.h/.cpp`, `direct3d_description.h/.cpp`.
  ***`re2dj_directx`** (new library): `abi.h`, `directdraw_description.h/.cpp`, `direct3d_description.h/.cpp`.*
- **Windows**: `directx_abi_windows.h`(검사와 `CopyFromCore`)를 추가했다. `direct3d7_com_facade.cpp`, `directdraw7_com_facade.cpp`, `direct3d3_com_facade.cpp`(`IDirectDraw4::GetCaps`)의 채우기 코드를 core 호출로 바꿨다.
  ***Windows:** `directx_abi_windows.h` (the checks and `CopyFromCore`); the fill code in `direct3d7_com_facade.cpp`, `directdraw7_com_facade.cpp`, and `direct3d3_com_facade.cpp` (`IDirectDraw4::GetCaps`) now calls the core.*
- **Linux ddraw**: `IDirect3D7` 8개 메서드(구현 6개)와 `IDirectDraw7` 설명 메서드 6개를 추가했다. 파일은 `ddraw_module.cpp`(진입점, `IDirectDraw7`), `ddraw_direct3d7.cpp`(`IDirect3D7`), `ddraw_interfaces.h`, `facade_com.h/.cpp`(COM 도우미)로 나눴다.
  ***Linux ddraw:** `IDirect3D7`'s 8 methods (6 implemented) and 6 `IDirectDraw7` description methods, in `ddraw_module.cpp` (entry points, `IDirectDraw7`), `ddraw_direct3d7.cpp` (`IDirect3D7`), `ddraw_interfaces.h`, and `facade_com.h/.cpp` (COM helpers).*
- **HLE**: `GuestComObject::parent`, `kernel32!lstrcpynA`(구현 65개, 해석 전용 32개), `user32!ShowCursor`(구현 10개, 해석 전용 27개), `GuestUser::ShowCursor`.
  ***HLE:** `GuestComObject::parent`, `kernel32!lstrcpynA` (65 implemented, 32 resolve-only), `user32!ShowCursor` (10 implemented, 27 resolve-only), and `GuestUser::ShowCursor`.*
- **Linux 진단**: `ReadGuestString`이 `GuestRangeReadable`을 쓴다.
  ***Linux diagnostic:** `ReadGuestString` uses `GuestRangeReadable`.*
- **단위 테스트**: 다음을 검사한다. / ***Unit tests** check:*
  - `IDirectDraw7` 설명 메서드의 결과, 모드 열거 취소 / *the `IDirectDraw7` description results and a cancelled mode enumeration;*
  - `IDirect3D7`의 vtable, 장치 이름과 T&L caps, depth 형식, `E_NOINTERFACE` / *`IDirect3D7`'s vtable, the device names with T&L caps, the depth format, and `E_NOINTERFACE`;*
  - 부모 참조 해제 / *the parent reference's release;*
  - `lstrcpynA`의 경계, `ShowCursor`의 counter / *`lstrcpynA`'s edge cases and `ShowCursor`'s counter.*

  기존 `IID_IDirectDraw4` 테스트 byte를 실제 IID로 바로잡았다.
  *The existing `IID_IDirectDraw4` test bytes were corrected to the real IID.*

## 분리 / The split

첫 커밋에서는 core의 설명이 `description.h` 하나에 있었고, Linux의 `IDirectDraw7`과 `IDirect3D7`도 `ddraw_module.cpp` 한 파일에 있었다. 사용자가 Windows facade처럼 Direct3D를 나눠야 하지 않냐고 지적했다. 그래서 core는 DirectDraw와 Direct3D 설명으로 나누고, Linux는 인터페이스별 파일과 공용 COM 도우미로 나눴다. facade module(`ddraw.dll`)은 하나로 둔다. 게임은 Direct3D DLL을 import하지 않고, `IDirect3D7`을 DirectDraw 객체에서만 얻기 때문이다. 분리 뒤 Linux 두 폭 CTest 3/3, 실제 4th 1,820번(두 폭 같음), Windows x86 CTest 6/6이다. Windows는 include만 바뀌어 실제 실행을 다시 하지 않았다.

*The first commit had the core's descriptions in one `description.h` and Linux's `IDirectDraw7` and `IDirect3D7` in one `ddraw_module.cpp`. The user pointed out that Direct3D should be separate, as in the Windows facade, so the core now has DirectDraw and Direct3D descriptions apart and Linux has a file per interface plus shared COM helpers. The facade module stays one `ddraw.dll`, since the game imports no Direct3D DLL and gets `IDirect3D7` only from a DirectDraw object. After the split: Linux CTest 3/3 on both widths, the real 4th at 1,820 calls (identical on both widths), and Windows x86 CTest 6/6; Windows changed only includes, so the real run was not repeated.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | 경고·오류 없음(`static_assert` 통과), 6/6 / no warnings or errors (`static_assert`s pass), 6/6 |
| Windows 실제 4th, 변경 전(`9023091`, worktree)·후 각 30초 / real 4th on Windows, pre-change (`9023091`, worktree) and post-change, 30 s each | `.ddraw.log`의 DirectX 줄이 같음: 드라이버 4개, 장치 GUID 3개, devcaps `0x0008af51`/`0x0009af51`, texop `0x0000ffff`, tri-texture `0x0000008d`, 표시 모드와 GetCaps. 차이는 창 handle·PID·스레드·WndProc 주소와 비동기 창 sample 줄 순서뿐이고, 두 실행 모두 Flip까지 진행 / the `.ddraw.log` DirectX lines match: four drivers, the three device GUIDs, devcaps `0x0008af51`/`0x0009af51`, texop `0x0000ffff`, tri-texture `0x0000008d`, display modes and GetCaps; the only differences are window handles, PIDs, threads, WndProc addresses, and the order of asynchronous window samples, and both runs reach Flip |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,820번, 주소를 정규화하면 같음. 드라이버 두 개마다 DirectDraw 생성, `IDirect3D7`, `GetCaps`, 모드 15개, 장치 3개 열거, 해제. 그 뒤 `ShowCursor(FALSE)` → -1, `#1820 IDirectDraw7::SetCooperativeLevel(hwnd, 0x813)`에서 정지. Hardlock 요청 수는 이전과 같음 / 1,820 calls, identical after address normalization; for each of the two drivers a DirectDraw object, `IDirect3D7`, `GetCaps`, 15 modes, 3 devices, and the releases; then `ShowCursor(FALSE)` → -1 and a stop at `#1820 IDirectDraw7::SetCooperativeLevel(hwnd, 0x813)`, with the Hardlock request totals unchanged |

## 다음 / Next

2단계인 협조 수준과 표시 모드다. `SetCooperativeLevel`과 `SetDisplayMode`를 Windows 창 모드 정책과 함께 공용 core로 옮긴다.

*Phase 2, cooperative level and display mode: `SetCooperativeLevel` and `SetDisplayMode` move into the shared core together with the Windows window-mode policy boundary.*
