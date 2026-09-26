# 작업 383 작업 로그 — DirectSound 진입과 창 조회 / Task 383 work log — DirectSound entry and window queries

설계: [20260926-383-directsound-entry.md](../design/20260926-383-directsound-entry.md)
작업 지시서: [20260926-383-directsound-entry.md](../work-orders/20260926-383-directsound-entry.md)

## 진행 / Progress

Linux 실행은 다음 순서로 진행했다.

1. `GetForegroundWindow`가 guest 창(`0x00010014`)을 돌려주자, 게임은 `DirectSoundCreate`에서 멈췄다.
2. DirectSound 1단계를 더하자 `SetCooperativeLevel(hwnd, DSSCL_EXCLUSIVE)`와 primary 버퍼 생성이 통과했고, 게임은 `GetWindowLongA(hwnd, GWL_HINSTANCE)`에서 멈췄다.
3. `GetWindowLongA`를 더하자 `0x00400000`을 돌려받은 게임은 `dinput.dll!DirectInputCreateA`에서 멈췄다.

Windows 실행 기록으로 보면, 게임은 DirectInput을 초기화한 뒤에 secondary 버퍼를 만들고 복제한다.

*The Linux run went through these steps:*

1. *With `GetForegroundWindow` returning the guest window (`0x00010014`), the game stopped at `DirectSoundCreate`.*
2. *With DirectSound phase 1 added, `SetCooperativeLevel(hwnd, DSSCL_EXCLUSIVE)` and the primary buffer's creation passed, and the game stopped at `GetWindowLongA(hwnd, GWL_HINSTANCE)`.*
3. *With `GetWindowLongA` added, the game got `0x00400000` back and stopped at `dinput.dll!DirectInputCreateA`.*

*The Windows run's record shows that the game makes and duplicates its secondary buffers after it initializes DirectInput.*

`GetWindowLongA`는 32비트 PowerShell에서 STATIC 창으로 측정했다.

- 성공하면 last error가 그대로다.
- `-7`, `-24`, 그리고 cbWndExtra를 넘는 index는 0을 돌려주고 1413을 설정한다.
- 없는 창은 0을 돌려주고 1400을 설정한다.

*`GetWindowLongA` was measured on a STATIC window from 32-bit PowerShell:*

- *Success leaves the last error unchanged.*
- *`-7`, `-24`, and indices past cbWndExtra return 0 and set 1413.*
- *An unknown window returns 0 and sets 1400.*

처음 Windows CTest에서 단위 테스트가 "ESP was not properly saved"로 중단되었다. 원인은 시간 초과로 강제 종료한 빌드였다. 이 빌드가 MSBuild의 의존성 기록을 깨뜨려, `guest_user.h`에 `user_data`를 더한 뒤에도 `guest_user.obj`가 다시 컴파일되지 않았다. 그 결과 `GuestUser::AddWindow(GuestWindow)`의 호출 쪽과 받는 쪽이 서로 다른 구조체 크기로 인자를 정리했다. `re2dj_core`의 중간 산출물을 지우고 다시 빌드하자 6/6이 통과했다. 교훈은 두 가지다. 빌드를 강제로 끝냈다면 해당 target의 중간 산출물을 지운다. Windows 빌드는 동시에 두 개를 돌리지 않는다.

*The first Windows CTest aborted the unit tests with "ESP was not properly saved". The cause was a build that was killed after timing out. It broke MSBuild's dependency records, so `guest_user.obj` was not recompiled after `user_data` was added to `guest_user.h`. The caller and callee of `GuestUser::AddWindow(GuestWindow)` then cleaned up the argument with different structure sizes. With `re2dj_core`'s intermediate files removed and rebuilt, the tests passed 6/6. Two lessons follow: after killing a build, remove that target's intermediate files, and never run two Windows builds at once.*

## 변경 / Changes

- **core (`re2dj::audio`)**:
  - `directsound_abi.h`: WAVEFORMATEX(packed 18바이트), DSBUFFERDESC, DSCAPS, DSBCAPS, HRESULT, flag, IID.
  - `directsound_device.h/.cpp`: `PlanSoundBuffer`, `DeviceCaps`, `BufferCaps`, `CheckDuplicate`, `PlanLock`.
  - `LegacyAudioBuffer::Lock`이 `PlanLock`을 쓴다.

  ***Core (`re2dj::audio`):***
  - *`directsound_abi.h`: WAVEFORMATEX (packed, 18 bytes), DSBUFFERDESC, DSCAPS, DSBCAPS, HRESULTs, flags, and IIDs.*
  - *`directsound_device.h/.cpp`: `PlanSoundBuffer`, `DeviceCaps`, `BufferCaps`, `CheckDuplicate`, and `PlanLock`.*
  - *`LegacyAudioBuffer::Lock` uses `PlanLock`.*
- **Windows**:
  - DirectSound facade의 버퍼 생성, caps, 복제 거절, speaker 설정이 core를 쓴다.
  - SDK와 비교하는 `static_assert`를 추가했다.

  ***Windows:***
  - *The DirectSound facade's buffer creation, caps, duplicate refusal, and speaker configuration use the core.*
  - *`static_assert`s against the SDK are added.*
- **Linux**:
  - `dsound.dll` module을 추가했다. `IDirectSound` 11개 메서드를 모두 구현했고, `IDirectSoundBuffer`는 9개를 구현했다. resolve-only 목록에서 뺐다.
  - `user32!GetForegroundWindow`, `GetWindowLongA`를 더했다(구현 13개, 해석 전용 24개).
  - `GuestWindow::user_data`를 더했다.
  - `kWin32ErrorInvalidWindowHandle`, `kWin32ErrorInvalidIndex`를 더했다.

  ***Linux:***
  - *The `dsound.dll` module is added, with all 11 `IDirectSound` methods and 9 of `IDirectSoundBuffer`'s, and removed from the resolve-only list.*
  - *`user32!GetForegroundWindow` and `GetWindowLongA` are added (13 implemented, 24 resolve-only).*
  - *`GuestWindow::user_data` is added.*
  - *`kWin32ErrorInvalidWindowHandle` and `kWin32ErrorInvalidIndex` are added.*
- **단위 테스트**:
  - `directsound_device_test.cpp`: 계획, lock, caps.
  - `dsound_module_test.cpp`: 생성, primary, secondary, 복제, lock·unlock, 공유 sample의 수명.
  - user32: foreground 창, `GetWindowLongA`.
  - resolve-only 목록: module 3개.

  ***Unit tests:***
  - *`directsound_device_test.cpp`: plans, locks, and caps.*
  - *`dsound_module_test.cpp`: creation, the primary, a secondary, duplication, lock and unlock, and the shared samples' lifetime.*
  - *user32: the foreground window and `GetWindowLongA`.*
  - *The resolve-only list: three modules.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`ea799d1`)·후 각 30초 / real 4th on Windows, pre-change (`ea799d1`) and post-change, 30 s each | ddraw 기록 1,410줄이 같다(작업 381과 같은 방식으로 가림). 오디오 생성·복제 기록 1,039줄도 같다(host 포인터만 가림). / *The 1,410 ddraw lines match (masked as in Task 381), and so do the 1,039 audio creation and duplication lines (host pointers masked).* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 1,872번이며, 주소를 정규화하면 두 폭이 같다. 차례로 `#1867 GetForegroundWindow` → hwnd, `#1868 DirectSoundCreate`·`#1869 SetCooperativeLevel`·`#1870 CreateSoundBuffer` → DS_OK, `#1871 GetWindowLongA` → `0x00400000`이다. 그 뒤 `#1872 dinput.dll!DirectInputCreateA`에서 멈춘다. / *1,872 calls, identical on both widths after address normalization. In order: `#1867 GetForegroundWindow` → hwnd; `#1868 DirectSoundCreate`, `#1869 SetCooperativeLevel`, and `#1870 CreateSoundBuffer` → DS_OK; and `#1871 GetWindowLongA` → `0x00400000`. The run then stops at `#1872 dinput.dll!DirectInputCreateA`.* |

## 다음 / Next

DirectInput(`DirectInputCreateA`)이다. Windows facade `directinput7_com_facade.cpp`의 규칙을 공용 core로 옮기고 Linux module을 만든다.

*Next is DirectInput (`DirectInputCreateA`): moving the Windows facade `directinput7_com_facade.cpp`'s rules into a shared core, and adding the Linux module.*
