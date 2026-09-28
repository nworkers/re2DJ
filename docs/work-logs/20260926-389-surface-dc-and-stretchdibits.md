# 작업 389 작업 로그 — 표면 DC와 StretchDIBits / Task 389 work log — surface DCs and StretchDIBits

설계: [20260926-389-surface-dc-and-stretchdibits.md](../design/20260926-389-surface-dc-and-stretchdibits.md)
작업 지시서: [20260926-389-surface-dc-and-stretchdibits.md](../work-orders/20260926-389-surface-dc-and-stretchdibits.md)

## 진행 / Progress

Linux 실행은 다음 순서로 진행했다.

1. `GetDC`가 DC `0x0a000014`를 돌려준다.
2. `StretchDIBits`가 `warning.abm`의 1024×512 5-5-5 이미지를 텍스처의 RGB565 픽셀에 그리고 512를 돌려준다.
3. `ReleaseDC`, `SetColorKey`, `GetSurfaceDesc`를 지난다.
4. 게임이 메인 루프에 들어간다. `timeGetTime`, keyboard와 mouse의 `GetDeviceState`를 부른 뒤 `user32!GetCursorPos`에서 멈춘다.

*The Linux run went through these steps:*

1. *`GetDC` returns DC `0x0a000014`.*
2. *`StretchDIBits` draws `warning.abm`'s 1024×512 5-5-5 image into the texture's RGB565 pixels and returns 512.*
3. *It passes `ReleaseDC`, `SetColorKey`, and `GetSurfaceDesc`.*
4. *The game enters its main loop: it calls `timeGetTime` and the keyboard's and mouse's `GetDeviceState`, then stops at `user32!GetCursorPos`.*

Windows 실행 비교에서 한 번 실수가 있었다. 비교 명령 끝의 정리 명령이 금지된 경로(`E:/r2base_*.txt`) 때문에 거부되자, 명령 전체가 실행되지 않았다. 그런데 나는 그 사이에 남아 있던 이전 작업의 로그 두 개를 비교했다. 파일 시각으로 이를 알아챘다. 변경 전 build를 다시 만들어 새로 실행하고, 그 결과를 아래에 적었다.

*I made one mistake in the Windows comparison. The cleanup at the end of the comparison command was refused for a protected path (`E:/r2base_*.txt`), so the whole command never ran, and I first compared two older logs left from the previous task. The file timestamps gave it away; the pre-change build was rebuilt and both runs made fresh, and those results are recorded below.*

## 변경 / Changes

- **core**: `CheckGetDc`, `CheckReleaseDc`, `CheckSetColorKey`, `kDdErrDcAlreadyCreated`, `kDdckeySrcBlt`.
  ***Core:** `CheckGetDc`, `CheckReleaseDc`, `CheckSetColorKey`, `kDdErrDcAlreadyCreated`, and `kDdckeySrcBlt`.*
- **Windows**: 표면 `GetDC`, `ReleaseDC`, `SetColorKey`가 core 규칙을 쓴다. SDK 검사 2개를 추가했다.
  ***Windows:** surface `GetDC`, `ReleaseDC`, and `SetColorKey` use the core rules, with two more SDK checks.*
- **HLE**:
  - `guest_gdi.h/.cpp`: `GuestGdi`, `GuestDc`, `GuestBitmap`.
  - `gdi_raster.h/.cpp`: 측정한 변환.
  - `GuestProcess::gdi()`.

  ***HLE:***
  - *`guest_gdi.h/.cpp`: `GuestGdi`, `GuestDc`, `GuestBitmap`.*
  - *`gdi_raster.h/.cpp`: the measured conversions.*
  - *`GuestProcess::gdi()`.*
- **Linux**:
  - `IDirectDrawSurface7::GetDC`, `ReleaseDC`, `SetColorKey`.
  - `gdi32!StretchDIBits`.

  ***Linux:***
  - *`IDirectDrawSurface7::GetDC`, `ReleaseDC`, and `SetColorKey`.*
  - *`gdi32!StretchDIBits`.*
- **단위 테스트**:
  - `gdi_raster_test.cpp`: 측정한 555·24bit 변환과 배치 도우미.
  - ddraw: 표면 DC의 두 번째 `GetDC` 거절, `StretchDIBits` 결과 픽셀, `ReleaseDC` 거절, `SetColorKey` 거절, 표면 해제 때 DC 제거.

  ***Unit tests:***
  - *`gdi_raster_test.cpp`: the measured 5-5-5 and 24-bit conversions and the layout helpers.*
  - *ddraw: a surface DC's second `GetDC` refused, `StretchDIBits`' resulting pixels, `ReleaseDC` refusals, `SetColorKey` refusals, and the DC removed with the surface.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th, 변경 전(`cb0a444`)·후 각 30초(22:16, 22:17 실행) / real 4th on Windows, pre-change (`cb0a444`) and post-change, 30 s each (runs at 22:16 and 22:17) | ddraw 기록 1,410줄이 같다(GetDC/ReleaseDC 214줄 포함). / *The 1,410 ddraw lines match, including 214 GetDC/ReleaseDC lines.* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 12,037번이며, 주소와 시계 값을 정규화하면 두 폭이 같다. `#12030 StretchDIBits` → 512를 지나 `#12037 user32.dll!GetCursorPos`에서 멈춘다. / *12,037 calls, identical on both widths after address and clock normalization. The run passes `#12030 StretchDIBits` → 512 and stops at `#12037 user32.dll!GetCursorPos`.* |

## 다음 / Next

메인 루프의 입력 조회(`GetCursorPos`부터)다. 그 뒤에는 그리기와 Linux 창 표시(5단계)가 온다.

*Next are the main loop's input queries (from `GetCursorPos`), then drawing and presenting in the Linux window (phase 5).*
