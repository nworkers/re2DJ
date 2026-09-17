# Win32 커서 표시와 자식 창 입력 / Win32 cursor visibility and child-window input

## 한국어

게스트 창을 우리 창 안에 넣어 실행하면서 겪은 Win32 입력·커서 동작을 정리한다. 프로젝트 고유 사실은 [작업 297 로그](../work-logs/20260917-297-imgui-osd-autoplay.md)와 [3rd 데모 플레이 분석](../analysis/ez2dj3rd-demo-play.md)에 있다.

### 키보드 메시지는 포커스를 가진 창으로 간다

키 입력 메시지(`WM_KEYDOWN` 등)는 **키보드 포커스를 가진 창**의 메시지 큐로 전달된다. 자식 창(`WS_CHILD`)은 `SetFocus`로 포커스를 받지 않는 한 키 메시지를 받지 않으며, 사용자가 활성화한 최상위 창이 받는다.

* [About Keyboard Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input) — Keyboard Focus and Activation

**이 프로젝트에서 확인됨.** re2DJ는 게스트 창을 호스트 창의 `WS_CHILD`로 넣는다. 게스트 창 프로시저에만 단축키를 연결했을 때 키 메시지가 한 건도 오지 않았고, 호스트 창 프로시저로 옮기자 동작했다. 마우스 메시지는 포커스가 아니라 커서 아래 창으로 가므로 게스트 영역의 마우스는 게스트 창이 받는다.

### 메시지를 삼켜도 `GetAsyncKeyState`는 막을 수 없다

`GetAsyncKeyState`는 호출 시점의 물리 키 상태를 돌려주며 메시지 큐를 거치지 않는다. 따라서 창 프로시저에서 `WM_KEYDOWN`을 처리하고 게스트에 넘기지 않아도, 키 상태를 직접 읽는 프로그램은 그 키를 본다. 게스트와 겹치지 않는 단축키를 고르는 것 외에는 방법이 없다.

* [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)

### 커서는 `WM_SETCURSOR`와 표시 카운트가 함께 정한다

* **`WM_SETCURSOR`:** 커서가 창 안에서 움직일 때 커서를 포함한 창에 보내진다. `lParam`의 하위 워드가 hit-test 코드이며 클라이언트 영역은 `HTCLIENT`다. 처리한 창이 `TRUE`를 돌려주면 이후 처리가 멈춘다. 처리하지 않으면 기본 처리가 창 클래스의 커서를 쓴다. 클래스 커서가 `NULL`이면 커서가 보이지 않는다.
* **표시 카운트:** `ShowCursor`는 내부 표시 카운트를 1 늘리거나 줄이고 새 값을 돌려준다. 카운트가 0 이상일 때만 커서가 보인다. `ShowCursor(FALSE)`를 한 프로그램은 `SetCursor`로 커서를 바꿔도 보이지 않는다.
* **현재 상태 확인:** `GetCursorInfo`의 `CURSOR_SHOWING` 플래그로 커서가 보이는지 알 수 있다.

**이 프로젝트에서 확인됨.** 3rd는 `ShowCursor`·`SetCursor`를 import 하고 커서를 숨긴다. re2DJ는 클라이언트 영역의 `WM_SETCURSOR`를 창의 스레드에서 처리해 화살표 커서를 설정하고, 커서가 보이지 않으면 `ShowCursor(TRUE)`로 카운트를 0 이상으로 되돌린다. 이 방식으로 커서가 계속 보임을 실행으로 확인했다. 창 테두리의 hit-test 코드는 기본 처리에 맡겨 크기 조절 커서를 유지한다.

* [WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor)
* [ShowCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showcursor)
* [GetCursorInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorinfo)

### Dear ImGui를 플랫폼 backend 없이 쓰기

Dear ImGui의 backend는 렌더러와 플랫폼으로 나뉜다. 플랫폼 backend는 창 이벤트를 `ImGuiIO`로 옮길 뿐이므로, 창을 직접 소유하지 못하는 환경에서는 렌더러 backend만 쓰고 `ImGuiIO::AddMousePosEvent`·`AddMouseButtonEvent`로 입력을 직접 넣을 수 있다. 이때 `io.DisplaySize`와 `io.DeltaTime`도 매 프레임 직접 채운다.

* [Dear ImGui backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md)

## English

Win32 input and cursor behavior met while running a guest window inside our own window. Project-specific facts live in [the task 297 log](../work-logs/20260917-297-imgui-osd-autoplay.md) and [the 3rd demo-play analysis](../analysis/ez2dj3rd-demo-play.md).

### Keyboard messages go to the window with focus

Keystroke messages such as `WM_KEYDOWN` are delivered to the **window that has keyboard focus**. A child window (`WS_CHILD`) receives none unless given focus with `SetFocus`; the top-level window the user activated receives them. See [About Keyboard Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input), Keyboard Focus and Activation.

**Confirmed in this project.** re2DJ places the guest window as a `WS_CHILD` of its host window. With a shortcut hooked only to the guest window procedure no key message arrived at all; moved to the host window procedure, it worked. Mouse messages follow the window under the cursor rather than focus, so the guest window receives mouse input over its area.

### Swallowing a message does not hide a key from `GetAsyncKeyState`

`GetAsyncKeyState` returns the physical key state at the time of the call without going through the message queue. Handling `WM_KEYDOWN` in a window procedure and not passing it on therefore does not hide the key from a program that reads key state directly; the only remedy is a shortcut the guest does not use. See [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate).

### The cursor is decided by `WM_SETCURSOR` and the display count together

* **`WM_SETCURSOR`** is sent to the window containing the cursor as it moves. The low-order word of `lParam` is the hit-test code, `HTCLIENT` over the client area. A window that handles it and returns `TRUE` halts further processing; otherwise default processing uses the window class cursor, and a `NULL` class cursor shows nothing.
* **The display count:** `ShowCursor` increments or decrements an internal display count and returns the new value; the cursor shows only while the count is zero or more. A program that called `ShowCursor(FALSE)` keeps the cursor hidden even after `SetCursor`.
* **Checking the current state:** the `CURSOR_SHOWING` flag from `GetCursorInfo` tells whether the cursor is visible.

**Confirmed in this project.** 3rd imports `ShowCursor` and `SetCursor` and hides the cursor. re2DJ handles `WM_SETCURSOR` over the client area on the window's thread, sets the arrow cursor, and when the cursor is not showing raises the count back to zero or more with `ShowCursor(TRUE)`; runs confirmed the cursor stays visible. Hit-test codes on the frame are left to default processing, keeping the resize cursors. See [WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor), [ShowCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showcursor) and [GetCursorInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorinfo).

### Using Dear ImGui without a platform backend

Dear ImGui backends split into renderer and platform. A platform backend only moves window events into `ImGuiIO`, so where the window cannot be owned, the renderer backend alone can be used with input fed directly through `ImGuiIO::AddMousePosEvent` and `AddMouseButtonEvent`, filling `io.DisplaySize` and `io.DeltaTime` every frame as well. See [Dear ImGui backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md).
