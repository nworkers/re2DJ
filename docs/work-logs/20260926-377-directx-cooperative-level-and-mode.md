# 작업 377 작업 로그 — DirectX core 2단계: 협조 수준과 표시 모드 / Task 377 work log — DirectX core phase 2: cooperative level and display mode

설계: [20260926-377-directx-cooperative-level-and-mode.md](../design/20260926-377-directx-cooperative-level-and-mode.md)
작업 지시서: [20260926-377-directx-cooperative-level-and-mode.md](../work-orders/20260926-377-directx-cooperative-level-and-mode.md)

## 진행 / Progress

Windows facade의 `RootSetCooperativeLevel`/`RootSetDisplayMode`를 읽고 규칙을 core로 옮겼다. `RootFacade`의 mode 필드 사용처 17곳을 `display.mode`로 바꿨다. Linux x64로 확인하며 진행했다. `SetCooperativeLevel`과 `GetDisplayMode`가 통과하자 게임이 `user32!SetRect`에서 멈췄다. `SetRect`를 측정해 구현하자 `SetDisplayMode(640, 480, 16, 60, 0)`가 통과했고, 다음은 3단계의 `CreateSurface`였다.

*The rules of the Windows facade's `RootSetCooperativeLevel`/`RootSetDisplayMode` moved into the core, and `RootFacade`'s 17 uses of its mode fields became `display.mode`. Working on Linux x64, `SetCooperativeLevel` and `GetDisplayMode` passed and the game stopped at `user32!SetRect`; with `SetRect` measured and implemented, `SetDisplayMode(640, 480, 16, 60, 0)` passed and the next stop was phase 3's `CreateSurface`.*

Windows 회귀용 변경 전 build(`ad61747`)는 이번에 짧은 경로(`E:/r2base`)의 worktree에서 만들었다. 작업 374에서는 scratchpad 경로가 길어 worktree를 지우지 못했기 때문이다.

*The pre-change Windows build (`ad61747`) was made in a short-path worktree (`E:/r2base`), since Task 374's scratchpad path was too long for the worktree to be removed.*

## 변경 / Changes

- **core**: `directdraw_display.h/.cpp`(`DirectDrawDisplay`, `HostWindowPolicy`, `SetCooperativeLevel`, `IsSupportedDisplayMode`, `SetDisplayMode`)를 추가했다. `kDefaultDisplayMode`로 이름을 바꿨고, `kDdErrGeneric`, `kDdErrUnsupportedMode`를 더했다.
  ***Core:** `directdraw_display.h/.cpp` (`DirectDrawDisplay`, `HostWindowPolicy`, `SetCooperativeLevel`, `IsSupportedDisplayMode`, `SetDisplayMode`), the rename to `kDefaultDisplayMode`, and `kDdErrGeneric` and `kDdErrUnsupportedMode`.*
- **Windows**: `RootFacade::display`, `LegacyRootDisplay`를 두고, 두 메서드와 `IDirectDraw7::GetDisplayMode`가 core를 쓴다. SDK 값 검사 2개를 추가했다.
  ***Windows:** `RootFacade::display` and `LegacyRootDisplay`, the two methods and `IDirectDraw7::GetDisplayMode` on the core, and two more SDK value checks.*
- **Linux**: `GuestComState`/`GuestComObject::state`/`StateAs<T>()`, `ddraw::DirectDrawState`, `IDirectDraw7::SetCooperativeLevel`/`SetDisplayMode`, 객체 모드를 읽는 `GetDisplayMode`, `user32!SetRect`(구현 11개, 해석 전용 26개).
  ***Linux:** `GuestComState`/`GuestComObject::state`/`StateAs<T>()`, `ddraw::DirectDrawState`, `IDirectDraw7::SetCooperativeLevel`/`SetDisplayMode`, `GetDisplayMode` reading the object's mode, and `user32!SetRect` (11 implemented, 26 resolve-only).*
- **단위 테스트**: `directx_display_test.cpp`(창·정책 실패·flags, 모드 거부), ddraw(창 없음·모르는 창·성공, 모드 거부·설정·조회), `SetRect`.
  ***Unit tests:** `directx_display_test.cpp` (window, policy failure, flags, mode refusal), ddraw (no window, unknown window, success; mode refusal, set, and query), and `SetRect`.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`ad61747`)·후 각 30초 / real 4th on Windows, pre-change (`ad61747`) and post-change, 30 s each | 창 handle·PID·스레드·WndProc와 frame 시간(ms)을 가리면, 남는 차이는 FPS 제목 갱신 줄 5개의 위치가 한 줄씩 밀린 것(내용 같음)과 마지막 frame 한 줄뿐. `SetCooperativeLevel`이 적용한 style `0x14cf0000`, 초기화 기록, Flip 수 같음 / with window handles, PIDs, threads, WndProcs, and frame times (ms) masked, the only differences are five FPS title-update lines shifted by one line (same content) and one more final frame line; the style `0x14cf0000` `SetCooperativeLevel` applies, the initialization record, and the Flip count match |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,824번, 주소를 정규화하면 같음. `#1820 SetCooperativeLevel(hwnd, 0x813)` → DD_OK, `#1821 GetDisplayMode`, `#1822 SetRect(0, 0, 640, 480)`, `#1823 SetDisplayMode(640, 480, 16, 60, 0)` → DD_OK, `#1824 IDirectDraw7::CreateSurface`에서 정지. Hardlock 요청 수는 이전과 같음 / 1,824 calls, identical after address normalization; `#1820 SetCooperativeLevel(hwnd, 0x813)` → DD_OK, `#1821 GetDisplayMode`, `#1822 SetRect(0, 0, 640, 480)`, `#1823 SetDisplayMode(640, 480, 16, 60, 0)` → DD_OK, then a stop at `#1824 IDirectDraw7::CreateSurface`, with the Hardlock request totals unchanged |

## 다음 / Next

3단계, 표면이다. `CreateSurface`, 표면 설명과 backing, attach, lock, 그리고 Linux에서 `GetDC`로 texture를 올리는 데 필요한 GDI DC HLE를 다룬다.

*Phase 3, surfaces: `CreateSurface`, surface descriptions and backing, attachments, locking, and the GDI DC HLE Linux needs for texture uploads through `GetDC`.*
