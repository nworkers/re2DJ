# src/hle

Win32 / DirectX HLE의 플랫폼 공용 구현이 들어 있습니다. `import_dispatcher.cpp`는 binding lookup, x86 ABI marshalling과 import completion을 담당합니다. 실제 API handler는 아직 등록하지 않습니다.

*This directory holds platform-neutral Win32 / DirectX HLE implementations. `import_dispatcher.cpp` performs binding lookup, x86 ABI marshalling, and import completion. No actual API handler is registered yet.*

우선순위는 `kernel32`·`user32`, 그다음 `gdi32`·`ddraw`·`dsound`, 마지막이 `dinput`·`winmm`·`advapi32`입니다. 실제로 호출되는 API만 구현합니다. 우선순위표는 [ARCHITECTURE.md](../../ARCHITECTURE.md) 8절입니다.

*Priority runs `kernel32` and `user32` first, then `gdi32`, `ddraw`, and `dsound`, then `dinput`, `winmm`, and `advapi32`. Only APIs the game actually calls get implemented. The table is in section 8 of [ARCHITECTURE.md](../../ARCHITECTURE.md).*
