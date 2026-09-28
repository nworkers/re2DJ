# 작업 384 설계 — DirectInput 진입 / Task 384 design — DirectInput entry

선행: [작업 383 설계](20260926-383-directsound-entry.md)

## 배경 / Background

작업 383 뒤 Linux 실행은 `dinput.dll!DirectInputCreateA(hinst, 0x700, ...)`에서 멈췄다. Windows 실행 기록을 보면 게임은 다음 순서로 입력 장치를 준비한다.

1. DirectInput 객체 하나와 system keyboard·mouse 장치를 만든다.
2. 두 장치에 `SetDataFormat`, `SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)`, `Acquire`를 부른다.
3. 매 프레임 keyboard의 `GetDeviceState`를 부른다.

*After Task 383 a Linux run stopped at `dinput.dll!DirectInputCreateA(hinst, 0x700, ...)`. The Windows run's record shows the game preparing its input devices in this order:*

1. *It makes one DirectInput object and the system keyboard and mouse devices.*
2. *It calls `SetDataFormat`, `SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)`, and `Acquire` on both devices.*
3. *It calls the keyboard's `GetDeviceState` every frame.*

Windows facade(`directinput7_com_facade.cpp`)는 대부분의 요청을 그대로 받는다.

- **장치 생성**: system mouse GUID면 mouse를, 그 밖의 GUID면 keyboard를 만든다.
- **`GetDeviceState`**: 버퍼를 0으로 채운 뒤, host에서 눌린 키의 DirectInput scan code 자리에 0x80을 쓴다. mouse는 버퍼가 `DIMOUSESTATE`를 담을 만큼 클 때만 눌린 버튼을 표시한다.

*The Windows facade (`directinput7_com_facade.cpp`) accepts most requests as they come.*

- ***Device creation:** the system mouse GUID makes a mouse, and any other GUID makes a keyboard.*
- ***`GetDeviceState`:** zeroes the buffer, then writes 0x80 at the DirectInput scan code of each key the host holds. A mouse marks held buttons only when the buffer holds a `DIMOUSESTATE`.*

SDK를 보면 이 facade의 `IID_IDirectInput7A`와 `IID_IDirectInputDevice7A` 값은 틀려 있다. 게임은 두 IID로 `QueryInterface`를 부르지 않으므로 지금까지 드러나지 않았다.

*Checked against the SDK, the facade's `IID_IDirectInput7A` and `IID_IDirectInputDevice7A` values are wrong. The game never calls `QueryInterface` with either IID, so the error had not shown.*

## 결정 / Decisions

1. **core (`re2dj/directx/directinput.h`).**
   - ABI: HRESULT, flag, `DIDEVCAPS`, `DIMOUSESTATE`, SDK 값의 IID·장치 GUID.
   - 인터페이스 판정: `IsDirectInputInterface`, `IsDirectInputDeviceInterface`.
   - 장치 종류: `DeviceKindOf`는 system keyboard·mouse를 돌려주고, 그 밖의 GUID는 `nullopt`다.
   - 장치 상태: `InputSnapshot`(DirectInput scan code 기준 눌린 키, 마우스 버튼 세 개)과 `ComposeDeviceState`(guest 버퍼 배치).

   ***Core (`re2dj/directx/directinput.h`):***
   - *ABI: HRESULTs, flags, `DIDEVCAPS`, `DIMOUSESTATE`, and the IIDs and device GUIDs at their SDK values.*
   - *Interface checks: `IsDirectInputInterface` and `IsDirectInputDeviceInterface`.*
   - *Device kinds: `DeviceKindOf` returns the system keyboard or mouse, and `nullopt` for any other GUID.*
   - *Device state: `InputSnapshot` (held keys by DirectInput scan code, and three mouse buttons) and `ComposeDeviceState` (the guest buffer's layout).*
2. **Windows.**
   - `QueryInterface`, 장치 종류, `GetDeviceState`의 버퍼 배치가 core를 쓴다.
   - host 키를 읽는 부분(`GetAsyncKeyState`, `MapVirtualKeyA`, 확장 키 표)은 facade에 남긴다. 결과는 `InputSnapshot`으로 넘긴다.
   - core가 모르는 GUID는 facade가 지금처럼 keyboard로 만든다.
   - 7A IID는 SDK 값으로 바로잡힌다.

   ***Windows:***
   - *`QueryInterface`, the device kind, and `GetDeviceState`'s buffer layout use the core.*
   - *Reading the host keys (`GetAsyncKeyState`, `MapVirtualKeyA`, and the extended-key table) stays in the facade and hands over an `InputSnapshot`.*
   - *For a GUID the core does not know, the facade still makes a keyboard, as before.*
   - *The 7A IIDs are corrected to their SDK values.*
3. **Linux `dinput.dll`(`dinput_module.cpp`).** resolve-only 목록에서 실제 module로 옮긴다.
   - `DirectInputCreateA`, `IDirectInputA::QueryInterface`/`CreateDevice`를 구현한다. system keyboard·mouse가 아닌 장치는 불리면 멈춘다.
   - 장치에는 `QueryInterface`, `GetCapabilities`, `Acquire`, `Unacquire`, `GetDeviceState`, `SetDataFormat`, `SetCooperativeLevel`을 구현한다.
   - Linux host 입력은 아직 연결하지 않는다. 그래서 `GetDeviceState`는 "눌린 것 없음"을 core 배치로 돌려준다. 키 입력은 Linux 창의 이벤트 처리 단계에서 `InputSnapshot`으로 연결한다.

   ***Linux `dinput.dll` (`dinput_module.cpp`)** moves from the resolve-only list to a real module.*
   - *`DirectInputCreateA` and `IDirectInputA::QueryInterface`/`CreateDevice`; a device other than the system keyboard or mouse stops when called.*
   - *Devices implement `QueryInterface`, `GetCapabilities`, `Acquire`, `Unacquire`, `GetDeviceState`, `SetDataFormat`, and `SetCooperativeLevel`.*
   - *Linux host input is not connected yet, so `GetDeviceState` returns "nothing held" in the core's layout. Key input connects through an `InputSnapshot` when the Linux window's event handling arrives.*

## 범위 밖 / Out of scope

- Linux host 키보드·마우스 입력. / *Linux host keyboard and mouse input.*
- IO 보드 포트 입출력(`in`/`out`)의 Linux 처리: 다음 작업. / *Handling the IO board's port I/O (`in`/`out`) on Linux: the next task.*
