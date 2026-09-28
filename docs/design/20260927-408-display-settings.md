# 작업 408 설계 — EnumDisplaySettingsA와 ChangeDisplaySettingsExA / Task 408 design — EnumDisplaySettingsA and ChangeDisplaySettingsExA

선행: [작업 407 설계](20260927-407-show-window.md)

## 배경 / Background

작업 407 뒤 Linux의 EZ2DJ 1st는 메시지 루프를 돌다가 `user32!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &devmode)`에서 멈췄다. 1st는 이어서 같은 DEVMODE로 `ChangeDisplaySettingsExA(..., CDS_FULLSCREEN)`를 부른다.

Windows 제품은 두 함수를 다르게 다룬다. `EnumDisplaySettingsA`는 Windows 자신이 답해서 host 모니터의 실제 모드가 나온다. `ChangeDisplaySettingsExA`는 흡수해서 성공으로 답하고 실제로는 모드를 바꾸지 않는다(`display_mode_boundary.cpp`). Windows 실행 기록에서 1st는 640×480×16을 요청했다.

*After Task 407, EZ2DJ 1st on Linux stopped in its message loop at `user32!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &devmode)`, followed by `ChangeDisplaySettingsExA(..., CDS_FULLSCREEN)` with the same DEVMODE. The Windows product lets Windows answer the first, so the guest reads the host monitor's real mode, and absorbs the second, answering success without changing the display (`display_mode_boundary.cpp`); on Windows, 1st asks for 640x480x16.*

### 측정 / Measurements

Windows 11(32비트)의 `EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS)`를 측정했다. 측정 host는 3840×2160×32, 60Hz였다.

*Measured on Windows 11 (32-bit) on a 3840x2160x32 60 Hz host.*

| 오프셋 / Offset | 쓰는 값 / Written |
| --- | --- |
| 0–3 (`dmDeviceName`) | "CDD\0". 나머지 28바이트는 그대로 / *the other 28 bytes untouched* |
| 32–39 | `dmSpecVersion`·`dmDriverVersion` `0x0401`, `dmSize` 124, `dmDriverExtra` 0 |
| 40 (`dmFields`) | `0x207C00A0` |
| 44–70 | 0 (`dmPosition`부터 `dmFormName` 첫 바이트까지) / *0, from `dmPosition` through `dmFormName`'s first byte* |
| 104–123 | 색 깊이, 너비, 높이, `dmDisplayFlags` 0, 주사율 / *bits per pixel, width, height, flags 0, frequency* |
| 나머지 / the rest | 그대로 / *untouched* |

TRUE를 돌려주고 last error는 그대로다. `ENUM_REGISTRY_SETTINGS`도 같다. 인덱스 열거(0부터)는 장치 이름 "cdd"와 `dmFields` `0x007C0080`로 다르다.

*TRUE, the last error unchanged; `ENUM_REGISTRY_SETTINGS` is the same. Enumeration by index differs (device name "cdd", `dmFields` `0x007C0080`).*

## 결정 / Decisions

1. **현재 모드는 host 데스크톱 모드.** Windows 제품의 guest가 실제 모니터 모드를 읽듯, Linux도 host의 데스크톱 모드로 답한다. `HostPresentation::DesktopDisplayMode`를 더한다. Linux host는 창 없이 SDL3 비디오 하위 시스템만 잠깐 열어 `SDL_GetDesktopDisplayMode`로 읽는다. 색 깊이는 픽셀당 바이트×8이다(32비트 데스크톱).
   ***The current mode is the host desktop's.** As the Windows product's guest reads the real monitor's mode, Linux answers with the host's desktop mode through a new `HostPresentation::DesktopDisplayMode`; the Linux host reads it with `SDL_GetDesktopDisplayMode`, opening only the SDL3 video subsystem, since no window exists yet. Bits per pixel are bytes per pixel times eight.*
2. **DEVMODEA는 측정한 바이트만 쓴다.** host가 모드를 알려 주지 않으면 멈춘다. 이름 있는 장치와 인덱스 열거도 모델 밖이라 멈춘다.
   ***The DEVMODEA gets exactly the measured bytes.** A host that cannot tell stops, as do a named device and enumeration by index.*
3. **`ChangeDisplaySettingsExA`는 Windows 제품처럼 흡수한다.** 인자와 상관없이 `DISP_CHANGE_SUCCESSFUL`, last error 0으로 답하고 host 화면은 바꾸지 않는다.
   ***`ChangeDisplaySettingsExA` is absorbed as on the Windows product:** `DISP_CHANGE_SUCCESSFUL` and last error 0 whatever the request; the host display never changes.*
