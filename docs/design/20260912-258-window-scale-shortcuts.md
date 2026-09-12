# 창 크기 단축키와 더블클릭 전체화면 전환 설계

## 목적

Windows host shell에서 다음의 창 표시 조작을 제공한다.

- `Alt+1`: 640×480 client area
- `Alt+2`: 1280×960 client area
- `Alt+3`: 1920×1440 client area
- 마우스 더블클릭: borderless monitor-sized fullscreen 전환 및 해제

논리 guest render target은 기존처럼 640×480으로 유지하고, 변경하는 것은 host client area와 host window style이다.

## 현재 구조와 제약

**확인됨:** `src/platform/windows/host_window_shell.cpp`는 원본 guest HWND를 host HWND의 `WS_CHILD`로 붙이고, host `WM_SIZE`에서 guest child를 client area 전체로 맞춘다. 따라서 실제 caption, outer size, fullscreen style은 host shell이 소유한다.

**확인됨:** `ApplyRe2djWindowMode`는 guest logical size를 받아 windowed client area를 현재 고정 배율 2배로 계산하며, `g_re2dj_fullscreen`이 true이면 monitor bounds와 `WS_POPUP`을 사용한다.

**설계 판단:** 입력은 guest child에 먼저 전달될 수 있으므로 host procedure만 수정하면 client area에서의 키·마우스 조작을 놓칠 수 있다. host shell이 guest WndProc를 안전하게 subclass하고, 기존 WndProc는 저장하여 모든 미처리 메시지를 그대로 위임한다.

## 설계

1. window mode에 현재 logical client size와 window scale을 저장한다. 기본 scale은 2이며 scale 1, 2, 3만 허용한다.
2. `ApplyRe2djWindowMode`는 기존 호출 호환성을 유지하면서 현재 scale을 사용해 windowed client area를 계산한다. fullscreen에서는 scale과 무관하게 monitor bounds를 유지한다.
3. `SetRe2djWindowScale`을 추가해 scale을 변경하고 현재 mode를 재적용한다. fullscreen 중에는 선택한 scale을 저장하고, fullscreen 해제 시 해당 windowed size로 돌아간다.
4. `ToggleRe2djFullscreen`을 추가해 `g_re2dj_fullscreen`을 반전하고 현재 logical size와 scale로 mode를 재적용한다. 실패하면 이전 상태로 복구한다.
5. host shell 생성 시 guest WndProc를 subclass한다. subclass state에는 원래 WndProc와 마지막 mouse-down 시각·좌표를 저장한다.
6. `WM_SYSKEYDOWN`/Alt 상태의 `1`, `2`, `3`을 scale 단축키로 소비한다. 숫자 키패드 1, 2, 3도 같은 scale로 지원한다. 자동 반복 keydown은 무시한다.
7. `WM_LBUTTONDBLCLK`를 처리하고, 원본 window class가 double-click style을 제공하지 않는 경우에도 동작하도록 두 번의 `WM_LBUTTONDOWN` 시각·거리로 double-click을 보완 감지한다.
8. subclass 해제 시 원래 WndProc를 복원하고 state property를 제거한다. 미처리 메시지와 일반 click은 원래 WndProc로 전달한다.
9. logical render target, DirectDraw display mode, host desktop display mode는 변경하지 않는다. host window의 client size와 style만 변경한다.

```mermaid
sequenceDiagram
    participant U as User
    participant G as Guest HWND subclass
    participant M as Window mode policy
    participant H as Host shell
    participant R as SDL/OpenGL logical target

    U->>G: Alt+1 / Alt+2 / Alt+3
    G->>M: SetRe2djWindowScale(1 / 2 / 3)
    M->>H: Reconfigure client area
    H-->>R: Keep logical 640x480 target; resize presentation child
    U->>G: Mouse double-click
    G->>M: ToggleRe2djFullscreen()
    M->>H: Switch WS_OVERLAPPEDWINDOW <-> WS_POPUP
    H-->>R: Keep logical target; change only host presentation bounds
```

## 검증 계획

- 기존 Windows x86 build와 unit test를 실행한다.
- runtime probe에서 windowed 기본 client size 1280×960, scale 1/2/3의 client size, fullscreen monitor bounds와 style을 확인한다.
- scale을 바꾼 뒤 fullscreen 전환·해제를 수행해 마지막 windowed scale이 복원되는지 확인한다.
- subclass가 원래 WndProc를 보존하고 미처리 메시지를 위임하는지 compile/runtime probe로 확인한다.
- 원본 logical render target이 640×480으로 유지되는지 기존 DirectDraw display-mode 검사로 확인한다.

Windows는 windowed host에 `WM_GETMINMAXINFO` 기본 최대 추적 크기를 적용합니다. 검증 데스크톱에서는 요청한 1920x1440 client가 1920x1421로 잘렸으므로, host WndProc가 최대 추적 크기를 확장하여 guest logical target과 desktop display mode를 바꾸지 않고 명시적 배율을 그대로 적용하도록 합니다.

SDL3가 external guest HWND를 wrapping할 때 기존 subclass를 저장할 수 있으므로, wrapping 직전에는 guest WndProc를 원복하고 wrapping 직후에는 SDL3가 설치한 현재 WndProc를 delegate로 삼아 입력 subclass를 다시 설치합니다. 이를 생략하면 SDL3의 저장된 이전 WndProc와 입력 subclass가 서로를 재호출할 수 있습니다.

## English

### Purpose

Add the following presentation controls to the Windows host shell:

- `Alt+1`: 640×480 client area
- `Alt+2`: 1280×960 client area
- `Alt+3`: 1920×1440 client area
- Mouse double-click: toggle borderless monitor-sized fullscreen

The logical guest render target remains 640×480. Only the host client area and host window style change.

### Current structure and constraints

**Confirmed:** `src/platform/windows/host_window_shell.cpp` reparents the original guest HWND as a `WS_CHILD` of the host HWND and resizes that child to the host client area on `WM_SIZE`. The host shell therefore owns the visible caption, outer size, and fullscreen style.

**Confirmed:** `ApplyRe2djWindowMode` receives the guest logical size, computes a windowed client area using a fixed scale of two, and uses monitor bounds with `WS_POPUP` when `g_re2dj_fullscreen` is true.

**Design decision:** Keyboard and mouse messages can arrive at the guest child first, so changing only the host procedure could miss input over the client area. The host shell will safely subclass the guest WndProc, preserve the original procedure, and delegate every unhandled message unchanged.

### Design

1. Store the current logical client size and window scale in the window-mode layer. The default scale is 2, and only scales 1, 2, and 3 are accepted.
2. Keep `ApplyRe2djWindowMode` source-compatible while making its windowed bounds use the current scale. Fullscreen continues to use monitor bounds regardless of scale.
3. Add `SetRe2djWindowScale` to change the scale and reapply the current mode. While fullscreen, the selected scale is remembered and used when returning to windowed mode.
4. Add `ToggleRe2djFullscreen` to invert `g_re2dj_fullscreen` and reapply the current logical size and scale. Restore the previous state if reconfiguration fails.
5. Subclass the guest WndProc when creating the host shell. The subclass state stores the original WndProc and the last mouse-down time and position.
6. Consume `WM_SYSKEYDOWN`/Alt-state top-row `1`, `2`, and `3` as scale shortcuts. Numeric keypad 1, 2, and 3 use the same scale. Ignore auto-repeat keydown messages.
7. Handle `WM_LBUTTONDBLCLK`, and also detect a double-click from two `WM_LBUTTONDOWN` messages using time and distance so the feature works even if the original window class lacks the double-click style.
8. Restore the original WndProc and remove the state property when the subclass is removed. Delegate unhandled messages and ordinary clicks to the original procedure.
9. Do not change the logical render target, DirectDraw display mode, or host desktop display mode. Change only the host window client size and style.

Windows applies a default `WM_GETMINMAXINFO` maximum track size to windowed hosts. On the validation desktop it clipped the requested 1920x1440 client to 1920x1421. The host WndProc therefore raises the maximum track dimensions so the explicit scale presets are applied without changing the guest logical target or desktop display mode.

When SDL3 wraps the external guest HWND, it can save the existing subclass. Suspend the guest WndProc subclass before wrapping and install it again afterward with SDL3's current WndProc as the delegate. Otherwise SDL3's saved previous procedure and the input subclass can call each other recursively.

### Verification plan

- Run the existing Windows x86 build and unit tests.
- Use the runtime probe to verify the default 1280×960 windowed client, scale 1/2/3 client sizes, fullscreen monitor bounds, and styles.
- Change scale, enter fullscreen, and leave fullscreen to verify that the selected windowed scale is restored.
- Verify through compile/runtime probing that the original WndProc is preserved and unhandled messages are delegated.
- Verify through the existing DirectDraw display-mode checks that the logical render target remains 640×480.
