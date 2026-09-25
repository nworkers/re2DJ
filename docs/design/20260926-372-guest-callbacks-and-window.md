# 작업 372 설계 — 게스트 호출과 window 생성 / Task 372 design — guest calls and window creation

선행: [작업 370 설계](20260925-370-guest-api-call-log.md), [작업 371 설계](20260926-371-winmm-timing-and-guest-files.md)

## 배경 / Background

작업 371 뒤 실제 4th는 WinMain의 `LoadIconA(NULL, "EZ2DJ")`에서 멈췄다. 이어지는 호출은 `LoadCursorA`, `GetStockObject`, `RegisterClassA`, `CreateWindowExA`, `UpdateWindow`다. Windows에서 `CreateWindowExA`와 `UpdateWindow`는 반환하기 전에 게스트의 window procedure를 여러 번 부른다. 그래서 host handler 안에서 게스트 함수를 끝까지 실행하고 그 반환값을 받는 경로가 먼저 있어야 한다. 이 경로는 두 폭에 모두 있어야 한다.

*After Task 371 the real 4th stopped at WinMain's `LoadIconA(NULL, "EZ2DJ")`, followed by `LoadCursorA`, `GetStockObject`, `RegisterClassA`, `CreateWindowExA`, and `UpdateWindow`. On Windows, `CreateWindowExA` and `UpdateWindow` call the guest's window procedure several times before they return, so a host handler first needs a way to run a guest function to completion and take its result, on both widths.*

## 측정 / Measurements

이 host(Windows 11, 32비트 PowerShell, WOW64)에서 P/Invoke로 게임과 같은 인자를 써서 측정했다.

*Measured on this host (Windows 11, 32-bit PowerShell, WOW64) through P/Invoke with the game's own arguments.*

| 호출 / Call | 결과 / Result |
| --- | --- |
| `LoadIconA(NULL, "EZ2DJ")`, `LoadIconA(NULL, 1)` | NULL, last error 1813 (`ERROR_RESOURCE_TYPE_NOT_FOUND`) |
| `LoadCursorA(NULL, "EZ2DJ")`, `LoadCursorA(NULL, 1)` | NULL, last error 1814 (`ERROR_RESOURCE_NAME_NOT_FOUND`) |
| `LoadIconA(NULL, IDI_APPLICATION)`, `LoadCursorA(NULL, IDC_ARROW)` | 매번 같은 handle, last error 그대로 / the same handle every time, last error untouched |
| `GetStockObject(0..19)` | 고정 handle(`BLACK_BRUSH` = `0x00900011`), 9와 20 이상은 NULL, last error 그대로 / fixed handles (`BLACK_BRUSH` = `0x00900011`), NULL for 9 and from 20, last error untouched |
| `RegisterClassA` 두 번 / twice | atom `0xC000` 이상, 두 번째는 0과 1410(`ERROR_CLASS_ALREADY_EXISTS`) / an atom from `0xC000`, then 0 with 1410 |
| `hInstance` NULL로 등록 → `CreateWindowExA`의 `hInstance` = exe / registered with NULL, created with the exe's handle | 찾음 / found |

`CreateWindowExA(WS_EX_APPWINDOW, "EZ2DJ", "EZ2DJ", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, …)`이 window procedure에 보내는 메시지와, DefWindowProc에 넘겼을 때의 결과는 다음과 같다.

*The messages `CreateWindowExA(WS_EX_APPWINDOW, "EZ2DJ", "EZ2DJ", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, …)` sends the window procedure, with DefWindowProc's results:*

| 순서 / # | 메시지 / Message | wParam, lParam | DefWindowProc |
| --- | --- | --- | --- |
| 1 | `WM_NCCREATE` | 0, `CREATESTRUCTA*` | 1 |
| 2 | `WM_NCCALCSIZE` | FALSE, `RECT*` {0, 0, 640, 480}, 그대로 / unchanged | 0 |
| 3 | `WM_CREATE` | 0, `CREATESTRUCTA*` | 0 |
| 4 | `WM_SIZE` | 0, `0x01E00280` | 0 |
| 5 | `WM_MOVE` | 0, 0 | 0 |
| 6 | `WM_SHOWWINDOW` | 1, 0 | 0 |
| 7–8 | `WM_WINDOWPOSCHANGING` ×2 | 0, `WINDOWPOS*` flags `0x43`, `0x03` | 0 |
| 9 | `WM_ACTIVATEAPP` | 1, 다른 앱의 thread ID / another app's thread ID | 0 |
| 10 | `WM_NCACTIVATE` | 1, 0 | 1 |
| 11 | `WM_ACTIVATE` | `WA_ACTIVE`, 0 — DefWindowProc 안에서 IME 2개, `WM_GETOBJECT` 2개, `WM_SETFOCUS(0, 0)` / inside DefWindowProc: two IME messages, two `WM_GETOBJECT`, `WM_SETFOCUS(0, 0)` | 0 |
| 12 | `WM_NCPAINT` | 1, 0 | 0 |
| 13 | `WM_ERASEBKGND` | HDC, 0 | 1 |
| 14 | `WM_WINDOWPOSCHANGED` | 0, `WINDOWPOS*` {0, 0, 640, 480}, flags `0x10001843` | 0 |

그 뒤 style은 `0x94000000`(`WS_CLIPSIBLINGS` 추가)이다. `UpdateWindow`는 갱신 영역이 있으면 `WM_PAINT`를 한 번 보낸다. DefWindowProc가 그 영역을 검증하므로 두 번째 호출은 아무것도 보내지 않는다. 둘 다 TRUE를 돌려준다.

*Afterwards the style is `0x94000000` (`WS_CLIPSIBLINGS` added). `UpdateWindow` sends one `WM_PAINT` while an update region exists; DefWindowProc validates it, so a second call sends nothing. Both return TRUE.*

## 결정 / Decisions

1. **게스트 호출 서비스.** `ImportCallServices::CallGuest(GuestCall*, result, error)`는 stdcall 게스트 함수를 끝까지 실행한다. `GuestCall`은 함수, 인자, 게스트 메모리에 둘 data(CREATESTRUCT 등)와 그 주소가 들어갈 인자 위치를 담는다. 게스트가 data를 고치면 그 내용이 돌려진다. 게스트가 그 사이에 부르는 import는 중첩 호출로 dispatch된다.
   ***Guest call service:** `ImportCallServices::CallGuest(GuestCall*, result, error)` runs a stdcall guest function to completion. `GuestCall` holds the function, the arguments, data to place in guest memory (a CREATESTRUCT, say) with the argument that receives its address, and the guest's changes to that data come back. Imports the guest makes meanwhile dispatch as nested calls.*
   - **x86**: handler는 이미 게스트 stack과 게스트 FS 위에서 돈다. 그래서 data를 현재 frame에 두고 naked asm helper가 인자를 16-byte 정렬로 복사해 부른다. callee가 얼마를 pop하든 register와 stack은 ebp에서 복원한다.
     ***x86:** the handler already runs on the guest stack with the guest's FS, so the data sits in the current frame and a naked asm helper copies the arguments 16-byte aligned and calls; registers and the stack come back from ebp whatever the callee pops.*
   - **x64**: SEH dispatch(작업 356)와 같은 중첩 compat 전환을 쓴다. runtime은 처리 중인 가장 안쪽 import의 guest esp를 기억하고, 그 아래 64 byte 간격을 둔 뒤 data, 인자, `exit32` 복귀 주소를 놓는다. 중첩 진입이 덮어쓰는 host rsp slot은 저장했다가 복원한다.
     ***x64:** the nested compatibility-mode transition of SEH dispatch (Task 356). The runtime remembers the guest esp of the innermost import being handled and places the data, arguments, and an `exit32` return address 64 bytes below it; the host rsp slot the nested entry overwrites is saved and restored.*
2. **재진입.** Linux 진단의 `Dispatch`는 기록과 오류를 지역 변수에 모은 뒤 끝날 때 넘긴다. 그래야 중첩 호출이 바깥 호출의 기록을 덮지 않는다. API log는 호출 머리(이름·인자)를 dispatch 전에, 결과를 dispatch 뒤에 쓴다. 중첩 호출은 바깥 호출의 머리와 결과 사이에 깊이만큼 들여써서 나온다. 게스트 호출은 `call  <함수>(<인자>, &data[<길이>]) -> <eax>`로 기록된다. 기록 인자 수 상한은 8에서 13으로 늘린다.
   ***Re-entry:** the Linux diagnostic's `Dispatch` collects its record and error locally and hands them over at the end, so nested calls do not overwrite the outer call's. The API log writes a call's head (name, arguments) before dispatch and its outcome after, so nested calls appear between them, indented by depth; guest calls are recorded as `call  <function>(<arguments>, &data[<length>]) -> <eax>`. The logged argument limit rises from 8 to 13.*
3. **USER 객체(`GuestUser`).** `GuestProcess`가 가진다. system icon·cursor(ID별 공유 handle), window class(atom `0xC000`부터, 이름은 대소문자 무시), window, 활성·focus window를 둔다. USER handle은 kernel handle과 다른 공간이다. `hInstance` NULL은 main image로 본다.
   ***USER objects (`GuestUser`):** owned by `GuestProcess`: system icons and cursors (one shared handle per ID), window classes (atoms from `0xC000`, names case-insensitive), windows, and the active and focus windows. USER handles are a space apart from kernel handles; a NULL `hInstance` means the main image.*
4. **user32.** `LoadIconA`, `LoadCursorA`(NULL `hInstance`만), `RegisterClassA`, `CreateWindowExA`, `DefWindowProcA`, `UpdateWindow`를 구현한다. `GetActiveWindow`는 활성 window를 돌려준다.
   ***user32:** `LoadIconA` and `LoadCursorA` (NULL `hInstance` only), `RegisterClassA`, `CreateWindowExA`, `DefWindowProcA`, and `UpdateWindow`; `GetActiveWindow` returns the active window.*
   - `CreateWindowExA`는 측정한 모양만 다룬다. 부모와 menu가 없는 top-level `WS_POPUP`이고, ex-style은 없거나 `WS_EX_APPWINDOW`다. caption·child·최소화·최대화·`CW_USEDEFAULT`와 등록되지 않은 class는 handler 실패로 멈춘다. 위 표의 메시지를 같은 인자로 보낸다. 다른 구성 요소가 보내는 IME·접근성 메시지와 shell의 `WM_GETICON`은 보내지 않는다. `WM_ACTIVATEAPP`의 lParam은 비활성화되는 다른 앱이 없으므로 0이고, `WINDOWPOS.hwndInsertAfter`도 0(`HWND_TOP`)이다. `WM_NCCREATE`가 0을 돌려주거나 `WM_CREATE`가 -1을 돌려주면 멈춘다.
     *`CreateWindowExA` models only the measured shape — a top-level `WS_POPUP` with no parent or menu and no ex-style beyond `WS_EX_APPWINDOW`; a caption, a child, minimized or maximized, `CW_USEDEFAULT`, or an unregistered class stops the handler. It sends the table's messages with the same arguments, leaving out the IME and accessibility messages other components send and the shell's `WM_GETICON`; `WM_ACTIVATEAPP`'s lParam is 0, as no other application loses activation, and `WINDOWPOS.hwndInsertAfter` is 0 (`HWND_TOP`). A `WM_NCCREATE` of 0 or a `WM_CREATE` of -1 stops.*
   - `DefWindowProcA`는 위 메시지와 `WM_PAINT`, `WM_KILLFOCUS`의 결과를 준다. `WM_ACTIVATE`(활성·비최소화)에서는 focus를 옮기고 `WM_KILLFOCUS`/`WM_SETFOCUS`를 보낸다. `WM_WINDOWPOSCHANGED`는 크기·위치가 그대로일 때만 다룬다. 그 밖의 메시지는 멈춘다. 그리기는 아직 하지 않는다.
     *`DefWindowProcA` answers those messages plus `WM_PAINT` and `WM_KILLFOCUS`; `WM_ACTIVATE` (active, not minimized) moves the focus with `WM_KILLFOCUS`/`WM_SETFOCUS`; `WM_WINDOWPOSCHANGED` is handled only with the size and position unchanged; any other message stops. Nothing is drawn yet.*
5. **gdi32 module.** `gdi32.dll`을 해석 전용 목록에서 분리해 `gdi32_module`로 둔다. `GetStockObject`는 측정한 handle 표를 쓴다.
   ***gdi32 module:** `gdi32.dll` leaves the resolve-only list for `gdi32_module`, with `GetStockObject` answering from the measured handle table.*

## 범위 밖 / Out of scope

- message queue와 `PeekMessageA`/`DispatchMessageA`, window timer, `ShowCursor`/`SetCursor`. 게임 loop에 닿으면 한다. / *The message queue with `PeekMessageA`/`DispatchMessageA`, window timers, and `ShowCursor`/`SetCursor`, when the game loop reaches them.*
- 실제 host window와 그리기. DirectDraw 작업에서 정한다. / *A real host window and drawing, settled with DirectDraw.*
- 게스트 호출 중 게스트 SEH가 host frame을 건너 unwind하는 경우. / *Guest SEH unwinding across host frames during a guest call.*
- DirectDraw(`DirectDrawEnumerateExA`부터)는 다음 작업이다. / *DirectDraw (from `DirectDrawEnumerateExA`) is the next task.*
