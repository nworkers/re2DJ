# 작업 388 작업 로그 — 키 상태 조회와 mixer 식별 / Task 388 work log — key state queries and mixer identification

설계: [20260926-388-input-queries.md](../design/20260926-388-input-queries.md)
작업 지시서: [20260926-388-input-queries.md](../work-orders/20260926-388-input-queries.md)

## 진행 / Progress

`GetAsyncKeyState`를 더하자 게임은 mixer를 조회했다. mixer ID 자리에 열린 handle을 넘겼기 때문에 처음에는 세 번 모두 `BADDEVICEID`를 받았다. Windows 11에서 그 조합을 측정해 규칙을 고치자, 게임은 다음 순서로 진행했다.

1. 구성 요소 유형으로 line을 찾는다(3번).
2. 유형으로 control을 찾는다(3번).
3. control 값 하나를 읽고 쓴다.
4. `timeGetTime`과 Hardlock 요청을 한 번 부른다.
5. `System\warning.abm`(1 MB)을 읽고 텍스처 표면을 만든다.
6. `IDirectDrawSurface7::GetDC`에서 멈춘다.

*With `GetAsyncKeyState` in place the game queried the mixer. Because it passed its open handle where a mixer ID is expected, it first got `BADDEVICEID` all three times. With that combination measured on Windows 11 and the rule fixed, the game went through these steps:*

1. *It finds lines by component type (three times).*
2. *It finds controls by type (three times).*
3. *It reads one control's value and writes it.*
4. *It calls `timeGetTime` and makes one Hardlock request.*
5. *It reads `System\warning.abm` (1 MB) and creates a texture surface.*
6. *It stops at `IDirectDrawSurface7::GetDC`.*

## 변경 / Changes

- **user32**: `GetAsyncKeyState`를 더했다(구현 14개, 해석 전용 23개).
  ***user32:** `GetAsyncKeyState` is added (14 implemented, 23 resolve-only).*
- **winmm**: `MIXER_OBJECTF_MIXER`에서 열린 handle도 받는다.
  ***winmm:** under `MIXER_OBJECTF_MIXER`, open handles are accepted too.*
- **단위 테스트**: `GetAsyncKeyState`(범위 안·밖과 last error), mixer ID 자리의 handle(열린 것·닫힌 것).
  ***Unit tests:** `GetAsyncKeyState` in and out of range with the last error, and handles in the mixer ID position (open and closed).*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 우리 코드 경고·오류 없음, 6/6 / exit 0, no warnings or errors from this project, 6/6 |
| Windows 실제 4th | 생략했다. 이번 변경은 Linux HLE(user32·winmm facade)만 건드렸다. / *Skipped; this change touches only the Linux HLE (the user32 and winmm facades).* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / no warnings or errors, 3/3 each |
| Linux probe, 기존 진단 네 개 / probes and the four diagnostics | 이전과 같음 / as before |
| 실제 4th, Linux 두 폭 / real 4th, both Linux widths | 호출 12,029번이며, 주소와 시계 값을 정규화하면 두 폭이 같다. `#12029 IDirectDrawSurface7::GetDC`에서 멈춘다. / *12,029 calls, identical on both widths after address and clock normalization, stopping at `#12029 IDirectDrawSurface7::GetDC`.* |

## 다음 / Next

텍스처 표면의 `GetDC`다. Windows facade는 GDI DIB로 DC를 준다. Linux에서는 guest 메모리의 픽셀 위에 GDI DC를 모델링해야 한다. 게임이 그 DC에서 부르는 GDI 호출을 보면서 진행한다.

*Next is `GetDC` on a texture surface. The Windows facade answers with a GDI DIB's DC; Linux needs a GDI DC modelled over the surface's pixels in guest memory, guided by the GDI calls the game makes on it.*
