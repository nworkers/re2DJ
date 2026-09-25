# 작업 372 작업 로그 — 게스트 호출과 window 생성 / Task 372 work log — guest calls and window creation

설계: [20260926-372-guest-callbacks-and-window.md](../design/20260926-372-guest-callbacks-and-window.md)
작업 지시서: [20260926-372-guest-callbacks-and-window.md](../work-orders/20260926-372-guest-callbacks-and-window.md)

## 진행 / Progress

`LoadIconA`와 `LoadCursorA`를 먼저 구현하고, 정지 지점을 따라 `GetStockObject`, `RegisterClassA`까지 갔다. API log에서 `RegisterClassA`의 WNDCLASSA를 보니 style `0x23`, WndProc `0x00406baa`, `hInstance` NULL, 검은 brush였다. 그다음 `CreateWindowExA`의 인자(`WS_EX_APPWINDOW`, `WS_POPUP | WS_VISIBLE`, 640×480, `hInstance` = exe)를 그대로 Windows에서 재현했다. window procedure가 받는 메시지, CREATESTRUCT, NCCALCSIZE rect, WINDOWPOS를 측정했다.

*`LoadIconA` and `LoadCursorA` came first, then `GetStockObject` and `RegisterClassA` by following the stops. The API log showed `RegisterClassA`'s WNDCLASSA: style `0x23`, WndProc `0x00406baa`, a NULL `hInstance`, and a black brush. `CreateWindowExA`'s arguments (`WS_EX_APPWINDOW`, `WS_POPUP | WS_VISIBLE`, 640×480, `hInstance` = the exe) were then reproduced on Windows, measuring the messages the window procedure receives with the CREATESTRUCT, NCCALCSIZE rectangle, and WINDOWPOS.*

첫 실행은 class를 찾지 못해 멈췄다. `hInstance` NULL로 등록한 class를 exe handle로 찾는 경우를 Windows에서 측정했고, 찾아진다는 것을 확인했다. 그 뒤 x86에서 WndProc가 메시지 14개와 중첩된 `WM_SETFOCUS`를 받았다. x64도 중첩 전환으로 같은 결과를 냈다. `UpdateWindow`를 측정해 구현하자 정지 지점이 `ddraw!DirectDrawEnumerateExA`로 옮겨 갔다.

*The first run stopped on the class lookup; Windows was measured to find a class registered with a NULL `hInstance` from the exe's handle. On x86 the WndProc then received the 14 messages and a nested `WM_SETFOCUS`, and x64 gave the same through its nested transition. With `UpdateWindow` measured and implemented, the stop moved to `ddraw!DirectDrawEnumerateExA`.*

`windows.h`의 `RegisterClass`·`FindWindow` 매크로와 겹치지 않도록 `GuestUser`의 method 이름을 `AddClass`와 `LookupWindow`로 정했다. 측정 script를 shell heredoc으로 고치다 escape가 두 번 깨졌다. 그래서 측정 probe는 파일 하나(동작 목록을 인자로 받음)로 다시 썼다.

*`GuestUser`'s methods are named `AddClass` and `LookupWindow` to stay clear of `windows.h`'s `RegisterClass` and `FindWindow` macros. Editing measurement scripts through shell heredocs broke their escapes twice, so the probe was rewritten as one file taking a list of actions.*

## 변경 / Changes

- **게스트 호출**: `native_import_bridge.h`의 `CallNativeGuestStdcall`을 x86 `native_import_bridge.cpp`(import 깊이 추적, `CallGuestStdcallWords`)와 x64 `native_compat_mode.cpp`(`import_stack_pointer`, 중첩 `NativeCompatEnterGuest`)에 구현했다.
  ***Guest calls:** `native_import_bridge.h`'s `CallNativeGuestStdcall`, implemented in x86 `native_import_bridge.cpp` (import depth, `CallGuestStdcallWords`) and x64 `native_compat_mode.cpp` (`import_stack_pointer`, a nested `NativeCompatEnterGuest`).*
- **서비스·기록**: `GuestCall`, `ImportCallServices::CallGuest`, `ApiCallEvent::Kind::kCallGuest`를 추가했다. `NativeKernel32Diagnostic::Dispatch`는 지역 기록을 쓰고, API log는 머리·결과로 나누어 깊이만큼 들여쓴다. 기록 인자 상한은 13이다.
  ***Services and record:** `GuestCall`, `ImportCallServices::CallGuest`, `ApiCallEvent::Kind::kCallGuest`; `NativeKernel32Diagnostic::Dispatch` uses a local record, and the API log is split into head and outcome, indented by depth; the logged argument limit is 13.*
- **`guest_user.h/.cpp`**(새 파일): icon, cursor, class, window, 활성·focus window. `GuestProcess::user()`가 이것을 준다.
  ***`guest_user.h/.cpp`** (new): icons, cursors, classes, windows, and the active and focus windows, through `GuestProcess::user()`.*
- **user32**: `LoadIconA`, `LoadCursorA`, `RegisterClassA`, `CreateWindowExA`, `DefWindowProcA`, `UpdateWindow`를 구현했고 `GetActiveWindow`를 갱신했다(구현 9개, 해석 전용 28개). Win32 오류 1410, 1813, 1814를 추가했다.
  ***user32:** `LoadIconA`, `LoadCursorA`, `RegisterClassA`, `CreateWindowExA`, `DefWindowProcA`, and `UpdateWindow`, with `GetActiveWindow` updated (9 implemented, 28 resolve-only); Win32 errors 1410, 1813, and 1814.*
- **`gdi32_module.h/.cpp`**(새 파일): `GetStockObject`와 해석 전용 11개.
  ***`gdi32_module.h/.cpp`** (new): `GetStockObject` and 11 resolve-only exports.*
- **단위 테스트**: `MemoryServices`에 가짜 게스트 호출(data를 test memory에 둠, 중첩 가능)을 추가했다. 다음을 검사한다.
  ***Unit tests:** `MemoryServices` gains a fake guest call (data in test memory, nestable), and the tests check:*
  - system icon·cursor와 오류 1813/1814 / *system icons and cursors with errors 1813/1814;*
  - class 중복과 NULL `hInstance` / *a duplicate class and a NULL `hInstance`;*
  - `CreateWindowExA`의 메시지 15개 순서, `WM_SIZE`, WINDOWPOS flag, DefWindowProc 결과, style, 활성·focus / *`CreateWindowExA`'s 15 messages in order, `WM_SIZE`, the WINDOWPOS flags, DefWindowProc's results, the style, and the active and focus windows;*
  - `UpdateWindow` 한 번만, 모델 밖 모양·메시지의 정지 / *a single `UpdateWindow` paint, and stops on shapes and messages outside the model;*
  - stock object, `call` 사건 형식 / *stock objects and the `call` event format.*

  해석 전용 module 테스트는 gdi32가 빠진 구성(5개 DLL, 18개)으로 바꿨다.
  *The resolve-only module test now expects the set without gdi32 (five DLLs, 18 exports).*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux helper, probe, 기존 진단 네 개 / diagnostics | 이전과 같음 / as before |
| Windows x86 build, CTest | 오류·경고 없음, 6/6 / no errors or warnings, 6/6 |
| 실제 4th, 두 폭 / real 4th, both widths | 호출 1,729번, 주소를 정규화하면 같음. `CreateWindowExA`가 hwnd `0x00010014`를 돌려주는 동안 WndProc `0x00406baa`가 메시지 14개(와 중첩된 `WM_SETFOCUS`)를 받아 `DefWindowProcA`로 넘김. `UpdateWindow`의 `WM_PAINT` 뒤 `#1729 ddraw!DirectDrawEnumerateExA`에서 정지. Hardlock 요청 수는 이전과 같음 / 1,729 calls, identical after address normalization; while `CreateWindowExA` returns hwnd `0x00010014`, WndProc `0x00406baa` receives the 14 messages (and a nested `WM_SETFOCUS`) and passes them to `DefWindowProcA`; after `UpdateWindow`'s `WM_PAINT` the run stops at `#1729 ddraw!DirectDrawEnumerateExA`, with the Hardlock request totals unchanged |

## 다음 / Next

DirectDraw다. `DirectDrawEnumerateExA`부터 시작한다. 게스트는 그 callback을 통해 장치를 고르므로 이번에 만든 게스트 호출을 그대로 쓴다.

*DirectDraw, from `DirectDrawEnumerateExA`, whose callback the guest uses to pick a device, so it reuses this task's guest calls.*
