# 작업 390 설계 — 메인 루프의 입력과 메시지 / Task 390 design — main-loop input and messages

선행: [작업 388 설계](20260926-388-input-queries.md), [작업 389 설계](20260926-389-surface-dc-and-stretchdibits.md)

## 배경 / Background

작업 389 뒤 Linux 실행은 메인 루프에 들어가 `user32!GetCursorPos`에서 멈췄다. 루프는 매 프레임 다음을 부른다.

1. `GetCursorPos`, `ScreenToClient`
2. `GetAsyncKeyState`(ESC, F3)
3. `PeekMessageA(msg, NULL, 0, 0, PM_REMOVE)`

메시지가 있으면 `TranslateMessage`와 `DispatchMessageA`를 부르고, 없으면 프레임을 그린다. 시작할 때 보호 envelope가 thread timer 하나를 걸어 둔다(`SetTimer(NULL, 0, 32768, 0x00aeaddb)`). 그래서 메시지 큐는 이 timer의 `WM_TIMER`를 돌려주고 그 proc을 불러야 한다.

*After Task 389 a Linux run entered the main loop and stopped at `user32!GetCursorPos`. Every frame the loop calls:*

1. *`GetCursorPos` and `ScreenToClient`*
2. *`GetAsyncKeyState` (ESC, F3)*
3. *`PeekMessageA(msg, NULL, 0, 0, PM_REMOVE)`*

*With a message it calls `TranslateMessage` and `DispatchMessageA`; without one it draws a frame. At start the protection envelope sets one thread timer (`SetTimer(NULL, 0, 32768, 0x00aeaddb)`), so the message queue must deliver that timer's `WM_TIMER` and call its procedure.*

32비트 PowerShell로 측정한 Windows 11 동작은 다음과 같다.

- **`GetCursorPos`**: 점 포인터가 null이면 FALSE와 `ERROR_NOACCESS`(998)다. 성공하면 last error를 바꾸지 않는다.
- **`ScreenToClient`**: 점에서 client 원점의 화면 좌표를 뺀다. 창 (100,50)의 client 원점이 (108,81)이면 (500,300)은 (392,219)가 된다. 모르는 창이나 null 점은 FALSE와 1400이고, 점은 그대로다.
- **`PeekMessageA`**:
  - 메시지가 없으면 FALSE이고, MSG와 last error는 그대로다.
  - thread timer의 `WM_TIMER`는 hwnd 0, wParam id, lParam proc, time은 꺼낸 시각, pt는 커서다.
  - `PM_REMOVE`로 꺼내면 다음 간격이 그 시각부터 다시 시작한다. 늦으면 하나로 합쳐진다.
  - `PM_NOREMOVE`는 같은 메시지를 다시 보여 준다.
- **`TranslateMessage(WM_TIMER)`**: FALSE이고, last error는 그대로다.
- **`DispatchMessageA`**:
  - 살아 있는 timer의 proc이면 `(hwnd, WM_TIMER, id, dispatch 시각)`으로 부르고, 그 반환값을 돌려준다.
  - 등록되지 않은 proc이나 창 없는 메시지는 0이고, 아무것도 부르지 않는다.

*Windows 11, measured from 32-bit PowerShell:*

- ***`GetCursorPos`:** a null point is FALSE with `ERROR_NOACCESS` (998); success leaves the last error alone.*
- ***`ScreenToClient`:** subtracts the client origin's screen position. With a window at (100,50) whose client origin is (108,81), (500,300) becomes (392,219). An unknown window or a null point is FALSE with 1400, and the point is left alone.*
- ***`PeekMessageA`:***
  - *With no message it is FALSE, leaving the MSG and the last error alone.*
  - *A thread timer's `WM_TIMER` has hwnd 0, wParam the id, lParam the procedure, time when it was taken, and pt the cursor.*
  - *Taking it with `PM_REMOVE` restarts the interval from then; late timers coalesce into one.*
  - *`PM_NOREMOVE` shows the same message again.*
- ***`TranslateMessage(WM_TIMER)`:** FALSE, with the last error untouched.*
- ***`DispatchMessageA`:***
  - *A live timer's procedure is called as `(hwnd, WM_TIMER, id, the dispatch time)`, and its result is returned.*
  - *An unregistered procedure, or a message for no window, is 0 and calls nothing.*

## 결정 / Decisions

1. **커서.** `GuestUser`가 커서 위치를 갖는다. host 포인터가 연결될 때까지는 화면 원점 (0,0)이다. 모델로 정한 값이고, 측정한 값이 아니다.
   ***The cursor:** `GuestUser` holds the cursor's position, the screen origin (0,0) until a host pointer is connected. This value is a modelling choice, not a measurement.*
2. **timer.** `GuestTimer`에 `base_tick`을 더한다. 설정할 때나 `WM_TIMER`를 꺼낼 때 그 시각이 들어간다. `SetTimer`는 host 시계를 읽는다.
   ***Timers:** `GuestTimer` gains `base_tick`, the time it was set or its `WM_TIMER` last left the queue. `SetTimer` reads the host clock.*
3. **메시지 큐(`PeekMessageA`).** 스레드 전체 조회만 모델링한다. 창·범위 필터와 다른 flag는 불리면 멈춘다. 순서는 Windows와 같다.
   1. posted 메시지. 아직 post하는 export가 없어 비어 있다.
   2. update region이 있는 창의 `WM_PAINT`. 꺼내도 큐에 남는다.
   3. 간격이 지난 thread timer의 `WM_TIMER`.

   ***Message queue (`PeekMessageA`):** only whole-thread peeks are modelled; window and range filters and other flags stop. The order follows Windows:*
   1. *Posted messages, empty since no export posts yet.*
   2. *`WM_PAINT` for a window with an update region, left in place.*
   3. *`WM_TIMER` for a thread timer whose interval has passed.*
4. **`TranslateMessage`와 `DispatchMessageA`.**
   - `TranslateMessage`는 키 메시지가 아니면 FALSE다. 키 메시지는 문자를 post해야 하므로 멈춘다.
   - `DispatchMessageA`는 측정한 timer 규칙을 따르고, 창 메시지는 그 창의 window procedure로 보낸다.

   ***`TranslateMessage` and `DispatchMessageA`:***
   - *`TranslateMessage` is FALSE for anything but key messages; those would post characters and stop.*
   - *`DispatchMessageA` follows the measured timer rules and sends window messages to the window procedure.*

## 범위 밖 / Out of scope

- `PostMessage`, `PostQuitMessage`, 입력 메시지(host 입력 단계). / *`PostMessage`, `PostQuitMessage`, and input messages (the host-input phase).*
- DirectX 5단계(`Clear`, 그리기, `Flip`, Linux 창 표시): 다음 작업. / *DirectX phase 5 (`Clear`, drawing, `Flip`, presentation in the Linux window): the next task.*
