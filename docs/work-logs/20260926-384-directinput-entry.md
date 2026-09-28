# 작업 384 작업 로그 — DirectInput 진입 / Task 384 work log — DirectInput entry

설계: [20260926-384-directinput-entry.md](../design/20260926-384-directinput-entry.md)
작업 지시서: [20260926-384-directinput-entry.md](../work-orders/20260926-384-directinput-entry.md)

## 진행 / Progress

DirectInput 1단계를 더하자, Linux 실행은 Windows 기록과 같은 순서로 입력 장치를 준비했다.

1. `DirectInputCreateA`로 DirectInput 객체를 만든다.
2. keyboard를 만들고 `SetDataFormat`, `SetCooperativeLevel(hwnd, 6)`을 부른다.
3. mouse를 만들고 같은 호출을 한다.
4. 두 장치에 `Acquire`를 부른다.

그 직후 게임 코드 `0x004c3817`에서 `in al, dx`(port `0x0103`)가 실행된다. Linux user mode에서는 이 명령이 SIGSEGV(SI_KERNEL, 일반 보호 예외)로 멈춘다. 게임이 IO 보드를 포트 입출력으로 직접 읽는 것이다. Windows는 이 명령을 privileged-instruction 예외 handler에서 공용 `LegacyIoPortBus`로 처리한다. Linux의 같은 처리는 다음 작업이다.

*With DirectInput phase 1 in place, the Linux run prepared the input devices in the same order as the Windows record:*

1. *`DirectInputCreateA` makes the DirectInput object.*
2. *It makes the keyboard and calls `SetDataFormat` and `SetCooperativeLevel(hwnd, 6)`.*
3. *It makes the mouse and makes the same calls.*
4. *It calls `Acquire` on both devices.*

*Right after that, the game's code at `0x004c3817` runs `in al, dx` (port `0x0103`). In Linux user mode that instruction stops with SIGSEGV (SI_KERNEL, a general protection fault): the game reads its IO board through port I/O. Windows handles the instruction in its privileged-instruction exception handler through the shared `LegacyIoPortBus`; doing the same on Linux is the next task.*

## 변경 / Changes

- **core**:
  - `directinput.h/.cpp`: ABI(HRESULT, flag, `DIDEVCAPS`, `DIMOUSESTATE`, IID, 장치 GUID).
  - `IsDirectInputInterface`, `IsDirectInputDeviceInterface`, `DeviceKindOf`.
  - `InputSnapshot`, `ComposeDeviceState`.

  ***Core:***
  - *`directinput.h/.cpp`: the ABI (HRESULTs, flags, `DIDEVCAPS`, `DIMOUSESTATE`, IIDs, and device GUIDs).*
  - *`IsDirectInputInterface`, `IsDirectInputDeviceInterface`, and `DeviceKindOf`.*
  - *`InputSnapshot` and `ComposeDeviceState`.*
- **Windows**:
  - DirectInput facade의 `QueryInterface`, 장치 종류, `GetDeviceState` 배치가 core를 쓴다.
  - 7A IID를 SDK 값으로 바로잡았다.
  - SDK 검사를 추가했다.

  ***Windows:***
  - *The DirectInput facade's `QueryInterface`, device kind, and `GetDeviceState` layout use the core.*
  - *The 7A IIDs are corrected to their SDK values.*
  - *SDK checks are added.*
- **Linux**:
  - `dinput.dll` module을 추가했다. 구현한 메서드는 `IDirectInputA` 4개와 `IDirectInputDeviceA` 9개(`AddRef`/`Release` 포함)다.
  - resolve-only 목록에는 `avifil32`와 `ws2_32`만 남는다.

  ***Linux:***
  - *The `dinput.dll` module is added, implementing 4 `IDirectInputA` methods and 9 `IDirectInputDeviceA` methods (including `AddRef`/`Release`).*
  - *Only `avifil32` and `ws2_32` remain on the resolve-only list.*
- **단위 테스트**:
  - `directx_input_test.cpp`: 장치·인터페이스 판정, keyboard·mouse 상태 배치.
  - `dinput_module_test.cpp`: 생성, 장치 설정, 빈 상태, 인터페이스, 모르는 장치에서 멈추기, 참조 수명.

  ***Unit tests:***
  - *`directx_input_test.cpp`: device and interface checks, and the keyboard and mouse state layouts.*
  - *`dinput_module_test.cpp`: creation, device setup, the empty state, interfaces, the stop on an unknown device, and reference lifetimes.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`86c8d3e`)·후 각 30초 / real 4th on Windows, pre-change (`86c8d3e`) and post-change, 30 s each | ddraw 기록 1,410줄과 DirectInput 기록 10줄이 같다. / *The 1,410 ddraw lines and 10 DirectInput lines match.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,880번이며, 주소를 정규화하면 두 폭이 같다. `#1872 DirectInputCreateA`부터 `#1880 Acquire`까지 모두 DI_OK다. 그 뒤 guest `0x004c3817`(`in al, dx`, port `0x0103`)에서 SIGSEGV로 멈춘다. / *1,880 calls, identical on both widths after address normalization. Every call from `#1872 DirectInputCreateA` to `#1880 Acquire` returns DI_OK. The run then stops with SIGSEGV at guest `0x004c3817` (`in al, dx`, port `0x0103`).* |

## 다음 / Next

Linux에서 IO 보드 포트 입출력을 처리한다. signal handler가 guest의 `in`/`out` 명령을 해석하고, Windows와 같은 `LegacyIoPortBus`로 답한 뒤 다음 명령으로 넘어간다.

*Next is handling the IO board's port I/O on Linux: the signal handler decodes the guest's `in`/`out` instructions, answers them through the same `LegacyIoPortBus` Windows uses, and moves on to the next instruction.*
