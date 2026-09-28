# 작업 396 작업 로그 — Linux 창 크기와 단축키 / Task 396 work log — the Linux window's size and shortcuts

설계: [20260927-396-linux-window-policy.md](../design/20260927-396-linux-window-policy.md)
작업 지시서: [20260927-396-linux-window-policy.md](../work-orders/20260927-396-linux-window-policy.md)

## 진행 / Progress

처음 Windows 빌드는 `direct3d3_com_facade.cpp`에서 실패했다(`PresentSync`를 `bool`로 바꿀 수 없음). Windows facade는 `Sdl3OpenGlWindowConfig`를 필드 순서대로 나열해 초기화한다. 그런데 새 필드를 `present_sync` 앞에 넣었기 때문이다. 새 필드를 구조체 끝으로 옮겨 해결했다.

*The first Windows build failed in `direct3d3_com_facade.cpp` (`PresentSync` cannot convert to `bool`): the Windows facade initializes `Sdl3OpenGlWindowConfig` by listing its fields in order, and the new fields had gone in before `present_sync`. Moving them to the end of the struct fixed it.*

합성 SDL 이벤트를 넣는 프로브(scratchpad의 `winprobe396`, x64)로 창 동작을 확인했다.

*A probe feeding synthetic SDL events (`winprobe396` in the scratchpad, x64) checked the window's behaviour:*

| 입력 / Input | 창 / Window | 배율·전체 화면 / Scale, fullscreen |
| --- | --- | --- |
| 열림 / opened | 1280×960 | 2, 아님 / *off* |
| Alt+1 | 640×480 | 1 |
| 오른쪽 Alt+숫자패드 3 / right Alt+keypad 3 | 1920×1440 | 3 |
| Alt 없는 2 / 2 without Alt | 그대로 / *unchanged* | 3 |
| Alt+2 키 반복 / Alt+2 auto-repeat | 그대로 / *unchanged* | 3 |
| 한 번 클릭 / single click | 그대로 / *unchanged* | 3 |
| 더블클릭 / double click | 3840×2160 전체 화면 / *fullscreen* | 3, 켬 / *on* |
| 전체 화면 중 Alt+2 / Alt+2 in fullscreen | 3840×2160 | 2(저장만 / *stored*) |
| 더블클릭 / double click | 1280×960 | 2, 끔 / *off* |
| 창 닫기 이벤트 / close event | — | `CloseRequested()` = 1 |

프로브의 90프레임은 1초 안에 끝나서 제목 FPS를 확인하지 못했다. 그래서 실제 4th 실행(x64)으로 확인했다.

- 12초 뒤 제목이 `... SDL3 OpenGL - FPS : 57.7`이었다.
- 창의 클라이언트 영역은 1280×960이었다.
- 창을 닫자 exit 0으로 끝났다.

화면 캡처는 원본 화면이라 저장소 밖 scratchpad에만 두었다.

*The probe's 90 frames finished within a second, so the title FPS was checked with a real 4th run (x64): after 12 seconds the title read `... SDL3 OpenGL - FPS : 57.7`, the client area was 1280×960, and closing the window ended with exit 0. The capture, of the original screen, is kept only outside the repository in the scratchpad.*

## 변경 / Changes

- **공용 / shared**: `re2dj/graphics/window_policy.h`(배율 상수, `IsWindowScale`, `FrameRateMeter`). Windows `window_mode.cpp`가 기본 배율과 범위에 쓴다. / *`re2dj/graphics/window_policy.h` (scale constants, `IsWindowScale`, `FrameRateMeter`), used by Windows' `window_mode.cpp` for its default scale and range.*
- **backend**: `Sdl3OpenGlWindowConfig::resizable`/`centered`(끝에 둠), `ResizeWindow`, `SetFullscreen`, `SetTitle`. / ***Backend:** `Sdl3OpenGlWindowConfig::resizable`/`centered` (at the end), `ResizeWindow`, `SetFullscreen`, and `SetTitle`.*
- **Linux 표시 / Linux presentation**: 기본 2배, Alt+1/2/3, 더블클릭 전체 화면, 모드 되돌리기, 제목 FPS, `SetStartFullscreen`. / *The default 2x scale, Alt+1/2/3, double-click fullscreen, undoing a refused mode, the title FPS, and `SetStartFullscreen`.*
- **CLI**: `--fullscreen`/`--windowed`를 Linux에서도 받는다. 기본은 profile 값이다. / *`--fullscreen`/`--windowed` accepted on Linux, defaulting to the profile.*
- **단위 테스트**: `window_policy_test.cpp`(배율 범위, FPS 구간). / ***Unit tests:** `window_policy_test.cpp` (the scale range and FPS intervals).*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `2f74b06`와 30초씩 / real 4th vs base `2f74b06`, 30 s each | 정규화한 ddraw 기록 1,410줄이 같다 / *the normalized ddraw logs match, 1,410 lines* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3(unit checks 3,995) / *no warnings or errors, 3/3 each (3,995 unit checks)* |
| Linux `--call-limit 32768`, 두 폭 / both widths | 호출 32,768번에서 멈춤, hardlock 83 / *stops at 32,768 calls, hardlock 83* |
| 프로브, 실제 실행 / probe and real run | 위 표와 같다. FPS 57.7, 클라이언트 영역 1280×960 / *as tabulated above; FPS 57.7, a 1280×960 client area* |

## 다음 / Next

Linux host 입력이다. 키보드·마우스를 DirectInput `InputSnapshot`, IO 보드, 커서에 연결한다.

*Next is Linux host input: keyboard and mouse into DirectInput's `InputSnapshot`, the IO board, and the cursor.*
