# src/hle

Win32 / DirectX HLE 구현이 들어갈 자리입니다. 아직 비어 있습니다.

*Implementations of the Win32 and DirectX HLE. Empty for now.*

우선순위는 `kernel32`·`user32`, 그다음 `gdi32`·`ddraw`·`dsound`, 마지막이 `dinput`·`winmm`·`advapi32`입니다. 실제로 호출되는 API만 구현합니다. 우선순위표는 [ARCHITECTURE.md](../../ARCHITECTURE.md) 8절입니다.

*Priority runs `kernel32` and `user32` first, then `gdi32`, `ddraw`, and `dsound`, then `dinput`, `winmm`, and `advapi32`. Only APIs the game actually calls get implemented. The table is in section 8 of [ARCHITECTURE.md](../../ARCHITECTURE.md).*
