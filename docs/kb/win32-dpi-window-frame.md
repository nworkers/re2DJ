# Win32 DPI-aware window frame 계산

## 한국어

Win32 top-level 창에서 원하는 client pixel 크기를 유지하려면 현재 DPI에 맞는 non-client frame을 outer rectangle에 더해야 한다. `AdjustWindowRectExForDpi`는 지정한 DPI와 style/exstyle로 필요한 window rectangle을 계산한다. Windows 10 version 1607 이상에서 제공되므로 runtime lookup 뒤 사용할 수 있고, 없으면 `AdjustWindowRectEx`로 fallback할 수 있다.

DPI awareness가 초기화 중 바뀌는 프로그램은 창을 만든 직후의 DPI 값과 renderer 초기화 뒤 DPI 값이 다를 수 있다. DPI context가 확정된 뒤 `GetDpiForWindow`로 다시 읽어 frame을 재계산해야 한다. 서로 다른 DPI monitor 사이 이동은 `WM_DPICHANGED`와 suggested rectangle을 별도로 처리해야 한다.

공식 자료:

- [AdjustWindowRectExForDpi](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi)
- [High DPI desktop application development](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
- [SetThreadDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext)

## English

To preserve a desired client-pixel size in a Win32 top-level window, add non-client frame dimensions calculated for the current DPI to the outer rectangle. `AdjustWindowRectExForDpi` computes the required window rectangle for an explicit DPI and style/exstyle. It is available from Windows 10 version 1607, so code can resolve it at runtime and fall back to `AdjustWindowRectEx` when unavailable.

Programs that change DPI awareness during initialization can observe different DPI values immediately after window creation and after renderer initialization. Read `GetDpiForWindow` and recalculate the frame after the DPI context is established. Moving between monitors with different DPIs additionally requires `WM_DPICHANGED` and its suggested rectangle.

Official references:

- [AdjustWindowRectExForDpi](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi)
- [High DPI desktop application development](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
- [SetThreadDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext)
