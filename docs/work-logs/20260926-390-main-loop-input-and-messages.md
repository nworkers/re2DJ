# 작업 390 작업 로그 — 메인 루프의 입력과 메시지 / Task 390 work log — main-loop input and messages

설계: [20260926-390-main-loop-input-and-messages.md](../design/20260926-390-main-loop-input-and-messages.md)
작업 지시서: [20260926-390-main-loop-input-and-messages.md](../work-orders/20260926-390-main-loop-input-and-messages.md)

## 진행 / Progress

`GetCursorPos`와 `ScreenToClient`를 차례로 더하자 게임은 ESC·F3 키 상태를 보고 `PeekMessageA`에 닿았다. 메시지 큐를 더하자 `PeekMessageA`는 FALSE를 돌려준다. 보호 timer(32,768ms)가 아직 만기되지 않았기 때문이다. 게임은 이어서 프레임 그리기에 들어가 `BeginScene`, render state 세 개를 지나 `IDirect3DDevice7::Clear`에서 멈췄다.

*With `GetCursorPos` and `ScreenToClient` added in turn, the game checked the ESC and F3 keys and reached `PeekMessageA`. With the message queue in place, `PeekMessageA` returns FALSE, since the protection timer (32,768 ms) is not yet due. The game then went on to draw a frame, passing `BeginScene` and three render states, and stopped at `IDirect3DDevice7::Clear`.*

측정 도구에서 두 가지를 바로잡았다.

- PowerShell의 `@(...)`는 object 배열이라 P/Invoke에 복사본으로 넘어간다. 그래서 `POINT` 결과가 돌아오지 않았다. `[int[]]::new(2)`로 다시 측정했다.
- 성공한 `DispatchMessageA` 뒤의 last error 1400은 C# delegate thunk의 영향일 수 있다. 그래서 모델에 넣지 않았다.

*Two things in the measuring tool were corrected:*

- *PowerShell's `@(...)` is an object array, which P/Invoke passes as a copy, so `POINT` results never came back. It was re-measured with `[int[]]::new(2)`.*
- *The last error 1400 after a successful `DispatchMessageA` may come from the C# delegate thunk, so it was left out of the model.*

## 변경 / Changes

- **HLE**:
  - `GuestUser`의 커서 위치와 `windows()`.
  - `GuestTimer::base_tick`, `GuestProcess::DueTimer`/`FindTimer`.
  - `SetThreadTimer`가 시각을 받는다.

  ***HLE:***
  - *`GuestUser`'s cursor position and `windows()`.*
  - *`GuestTimer::base_tick` and `GuestProcess::DueTimer`/`FindTimer`.*
  - *`SetThreadTimer` takes the time.*
- **user32**:
  - `GetCursorPos`, `ScreenToClient`, `PeekMessageA`, `TranslateMessage`, `DispatchMessageA`를 더했다(구현 19개, 해석 전용 18개).
  - `SetTimer`가 host 시계를 읽는다.

  ***user32:***
  - *`GetCursorPos`, `ScreenToClient`, `PeekMessageA`, `TranslateMessage`, and `DispatchMessageA` are added (19 implemented, 18 resolve-only).*
  - *`SetTimer` reads the host clock.*
- **단위 테스트**:
  - 커서와 `ScreenToClient`(성공, null, 모르는 창).
  - 메시지 루프: 빈 큐, `WM_TIMER`와 간격 재시작, `PM_NOREMOVE`, `TranslateMessage`, timer proc dispatch, 등록되지 않은 proc.

  ***Unit tests:***
  - *The cursor and `ScreenToClient` (success, null, unknown window).*
  - *The message loop: the empty queue, `WM_TIMER` with the interval restart, `PM_NOREMOVE`, `TranslateMessage`, dispatch to a timer procedure, and an unregistered procedure.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th | 생략했다. `src/platform/windows`에 바뀐 것이 없다. / *Skipped; nothing under `src/platform/windows` changed.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 12,047번이며, 주소와 시계 값을 정규화하면 두 폭이 같다. `#12042 PeekMessageA` → 0, `#12043 BeginScene`을 지나 `#12047 IDirect3DDevice7::Clear`에서 멈춘다. / *12,047 calls, identical on both widths after address and clock normalization. The run passes `#12042 PeekMessageA` → 0 and `#12043 BeginScene`, then stops at `#12047 IDirect3DDevice7::Clear`.* |

## 다음 / Next

DirectX 5단계, 그리기와 Linux 창 표시다(`Clear`, `SetTexture`, `DrawPrimitive`, `EndScene`, `Flip`).

*Next is DirectX phase 5, drawing and presenting in the Linux window (`Clear`, `SetTexture`, `DrawPrimitive`, `EndScene`, `Flip`).*
