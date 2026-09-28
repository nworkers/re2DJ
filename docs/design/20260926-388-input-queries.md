# 작업 388 설계 — 키 상태 조회와 mixer 식별 / Task 388 design — key state queries and mixer identification

선행: [작업 386 설계](20260926-386-winmm-mixer.md), [작업 387 설계](20260926-387-directsound-controls.md)

## 배경 / Background

작업 387 뒤 Linux 실행은 `user32!GetAsyncKeyState(VK_TAB)`에서 멈췄다. 32비트 PowerShell로 측정한 Windows 11 동작은 다음과 같다.

- 키 코드 0–255는 키 상태를 돌려주고, last error를 바꾸지 않는다.
- 256 이상이거나 음수이면 0을 돌려주고 `ERROR_INVALID_PARAMETER`(87)를 설정한다. 상위 bit를 잘라내지 않으므로 `0x10009`도 87이다.

`GetAsyncKeyState`를 더하자, 게임은 `mixerGetLineInfoA(hmx, ..., MIXER_GETLINEINFOF_COMPONENTTYPE)`를 부르고 `MMSYSERR_BADDEVICEID`를 받았다. 게임은 열린 handle을 넘기면서 객체 유형으로는 `MIXER_OBJECTF_MIXER`(0)를 준다.

Windows 11에서 이 조합을 측정했다.

- 열린 handle은 mixer ID 자리에서도 그 mixer를 가리킨다.
- 닫힌 handle 값은 `MMSYSERR_BADDEVICEID`(2)다.

*After Task 387 a Linux run stopped at `user32!GetAsyncKeyState(VK_TAB)`. Windows 11, measured from 32-bit PowerShell, behaves like this:*

- *Key codes 0–255 return the key state and leave the last error alone.*
- *256 and above, or negative, return 0 and set `ERROR_INVALID_PARAMETER` (87). The high bits are not masked, so `0x10009` is 87 too.*

*With `GetAsyncKeyState` in place, the game called `mixerGetLineInfoA(hmx, ..., MIXER_GETLINEINFOF_COMPONENTTYPE)` and got `MMSYSERR_BADDEVICEID`. The game passes its open handle but gives `MIXER_OBJECTF_MIXER` (0) as the object type.*

*This combination was measured on Windows 11:*

- *An open handle names its mixer even where a mixer ID is expected.*
- *A closed handle's value is `MMSYSERR_BADDEVICEID` (2).*

## 결정 / Decisions

1. **`GetAsyncKeyState`.** 측정한 last error 규칙을 따른다. Linux host 입력은 아직 연결되지 않았으므로, 범위 안의 키는 "안 눌림, 마지막 조회 뒤 눌린 적 없음"(0)으로 답한다. host 입력 단계에서 DirectInput과 같은 `InputSnapshot`으로 연결한다.
   ***`GetAsyncKeyState`** follows the measured last-error rules. Linux host input is not connected yet, so keys in range answer "up, not pressed since the last query" (0). The host-input phase will connect them through the same `InputSnapshot` DirectInput uses.*
2. **mixer 식별.** `MIXER_OBJECTF_MIXER`에서 mixer ID 0뿐 아니라 열린 handle도 그 mixer로 받는다. 그 밖의 값은 `MMSYSERR_BADDEVICEID`다.
   ***Mixer identification:** under `MIXER_OBJECTF_MIXER`, an open handle names the mixer as well as mixer ID 0; any other value is `MMSYSERR_BADDEVICEID`.*

## 범위 밖 / Out of scope

- Linux host 키보드·마우스 입력. / *Linux host keyboard and mouse input.*
- 텍스처 표면의 `IDirectDrawSurface7::GetDC`와 GDI 그리기: 다음 작업. / *`IDirectDrawSurface7::GetDC` on a texture surface, and GDI drawing: the next task.*
