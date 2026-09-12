# 작업 로그: 창 크기 단축키와 더블클릭 fullscreen

## 결과

Windows host shell에 다음 동작을 추가했습니다.

- `Alt+1`: 640x480 windowed client
- `Alt+2`: 1280x960 windowed client
- `Alt+3`: 1920x1440 windowed client
- client 영역 double-click: fullscreen 진입/해제

원본 guest logical render target과 DirectDraw display mode는 계속 640x480이며, host desktop display mode는 변경하지 않습니다.

## 원인과 수정

1. 원본 HWND가 host `WS_CHILD`이므로 guest child WndProc를 subclass하여 단축키와 mouse double-click을 처리했습니다. 처리하지 않는 메시지는 원본 WndProc로 전달하고, 창 제거 시 원복합니다.
2. SDL3 external HWND wrapping이 기존 subclass를 저장할 수 있어, wrapping 직전에 subclass를 잠시 원복하고 wrapping 후 SDL3 WndProc를 delegate로 다시 감쌌습니다. 이 절차가 없으면 SDL3와 subclass가 서로를 재호출하는 예외가 발생했습니다.
3. window mode 계층에 현재 scale과 logical client size를 저장하고, scale 변경·fullscreen 전환 실패 시 이전 상태로 복구하도록 했습니다.
4. Windows 기본 `WM_GETMINMAXINFO`가 검증 데스크톱에서 1920x1440 client를 1920x1421로 잘랐습니다. host shell이 최대 추적 크기를 확장하여 명시된 preset 크기를 유지하도록 했습니다.

## 검증

- Windows x86 Debug build: 성공
- CTest 4개 선택 실행: 4/4 통과
  - `re2dj_ez2dj_keyboard_input_test`
  - `re2dj_ez2dancer_keyboard_input_test`
  - `re2dj_windows_product_loader_probe`
  - `re2dj_unit_tests` — 1679 checks, 0 failures
- `re2dj_windows_vfs_runtime_probe --vfs-enumeration-only`: 완료
- runtime window probe의 Alt+1/2/3, double-click fullscreen 왕복 검증: 단계 검증 통과
- 전체 runtime probe는 새 window 검증 이후 기존 비활성 `\\.\LPTDI7` 장치 경로에서 정체했습니다. 입력 검증 블록을 제외한 대조 실행에서도 같은 지점에 정체했으므로 이번 창 변경의 실패로 판단하지 않았습니다.

## English

# Work Log: Window Scale Shortcuts and Double-Click Fullscreen

## Result

The Windows host shell now supports:

- `Alt+1`: 640x480 windowed client
- `Alt+2`: 1280x960 windowed client
- `Alt+3`: 1920x1440 windowed client
- Double-clicking the client area: enter/leave fullscreen

The original guest logical render target and DirectDraw display mode remain 640x480, and the host desktop display mode is unchanged.

## Cause and changes

1. Because the original HWND is a `WS_CHILD` of the host, the guest WndProc is subclassed for shortcuts and mouse double-clicks. Unhandled messages continue to the original WndProc, which is restored when the window is removed.
2. SDL3 can save the existing subclass while wrapping an external HWND. The subclass is suspended before wrapping and reinstalled afterward with SDL3's WndProc as its delegate. Without this ordering, SDL3 and the subclass recursively called each other and raised an exception.
3. The window-mode layer stores the current scale and logical client size and restores the previous state if a scale change or fullscreen transition fails.
4. Windows' default `WM_GETMINMAXINFO` clipped the requested 1920x1440 client to 1920x1421 on the validation desktop. The host shell now raises the maximum tracking size so the explicit preset remains exact.

## Verification

- Windows x86 Debug build: passed
- Four selected CTest tests: 4/4 passed
  - `re2dj_ez2dj_keyboard_input_test`
  - `re2dj_ez2dancer_keyboard_input_test`
  - `re2dj_windows_product_loader_probe`
  - `re2dj_unit_tests` — 1679 checks, 0 failures
- `re2dj_windows_vfs_runtime_probe --vfs-enumeration-only`: completed
- Runtime window-probe checks for Alt+1/2/3 and double-click fullscreen round trips: passed before the later unrelated probe section
- The full runtime probe stops after the new window checks at the existing disabled `\\.\LPTDI7` device path. A control run with the new input-check block excluded stopped at the same point, so this was not attributed to the window change.
