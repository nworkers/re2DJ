# #14 설계: 전체 화면·비율 유지 옵션과 `cfg/re2dj.ini` 저장, 런처 게임패드 동작

이슈: [#14](https://github.com/reexec/re2DJ/issues/14) · 참고: rePIU v0.0.214(#45), v0.0.212(#34) · 선행: [#12 런처](20261010-i012-launcher.md)

## 배경

* 게임 창은 늘 원본 비율을 지킵니다. `FitPresentation`이 창 안에서 가장 큰 4:3 사각형을 고르고 나머지는 검은 띠입니다(백엔드의 그리기와 `host_presentation`의 마우스 좌표 변환이 같은 함수를 씀).
* 전체 화면은 `--fullscreen`/`--windowed`(없으면 프로필 기본값)로 시작하고, 게임 중에는 왼쪽 더블클릭으로만 바꿉니다. 바꾼 상태는 저장되지 않고 OSD에도 항목이 없습니다. Alt+1..3은 창 배율입니다.
* 런처(#12)는 늘 창 모드로 열리고 창 전체에 그립니다. 표에서는 Enter·Space·패드 A·더블클릭이 시작합니다.

rePIU는 v0.0.214(#45)에서 이 부분을 정리했고, 사용자가 re2DJ에도 같게 적용하기로 했습니다(2026-10-10).

## 목표

1. **비율 유지**(Keep aspect ratio, 기본 켬): 켜면 지금처럼 4:3과 검은 띠, 끄면 그림이 창(전체 화면이면 화면) 전체를 채웁니다. 창 모드에도 적용합니다.
2. 게임 중 OSD에서 **전체 화면**과 **비율 유지**를 바꿉니다. 두 체크박스를 OSD 항목의 맨 위에 둡니다.
3. 게임 창에서 **Alt+Enter**로 전체 화면을 바꿉니다(더블클릭과 같음).
4. OSD·Alt+Enter·더블클릭으로 **사용자가 바꾼** 값은 `cfg/re2dj.ini`의 `[Video] fullscreen`·`keep_aspect`에 저장하고, 다음 실행이 그 값으로 시작합니다.
5. 런처: 두 체크박스를 Options 맨 위에 두고, 런처 창도 두 값을 따릅니다(전체 화면으로 열기, Alt+Enter, 비율 유지 시 기본 창 비율 영역을 가운데에 두고 검은 띠). 표에서는 Enter·패드 A·더블클릭이 시작하고 Space는 선택만 합니다.

## 결정

### 1. 시작 값의 우선순위

| 값 | 1순위 | 2순위 | 3순위 |
|---|---|---|---|
| 전체 화면 | `--fullscreen` / `--windowed` | `cfg/re2dj.ini` `[Video] fullscreen` | 프로필 기본값 |
| 비율 유지 | `--keep-aspect` / `--stretch` (새 옵션) | `cfg/re2dj.ini` `[Video] keep_aspect` | 켬 |

rePIU처럼 인자를 준 실행도 설정 파일을 읽습니다. 명령줄이 가장 직접적인 지시이므로 이깁니다. 파일은 현재 디렉터리의 `cfg/re2dj.ini`이고, 런처와 같은 파일·같은 파서(`launcher_settings`)를 씁니다. 파일이 없거나 값이 잘못됐으면 다음 순위로 갑니다(잘못된 값은 경고).

`--keep-aspect`/`--stretch`를 새로 두는 이유는, 런처가 고른 값을 자식에게 다른 옵션처럼 명령줄로 넘기기 위해서입니다. 저장이 실패한 경우(읽기 전용 디렉터리 등)에도 고른 값이 그 실행에 적용됩니다.

### 2. 그림 사각형

`window_policy.h`에 `ComputePresentRect(window_w, window_h, logical_w, logical_h, keep_aspect)`를 둡니다. 켜면 `FitPresentation`, 끄면 창 전체입니다. 백엔드의 그리기와 마우스 좌표 변환이 모두 이 함수를 쓰므로 그림과 포인터가 어긋나지 않습니다. 백엔드는 `SetKeepAspect(bool)`로 값을 받고 다음 present부터 적용합니다. 표시 필터(`SelectPresentationFilter`)는 지금처럼 그림 사각형의 크기로 고릅니다.

### 3. 게임 창 입력과 OSD

* **Alt+Enter**(본 키·키패드, 반복 제외): 전체 화면을 바꿉니다. 이때 Enter는 게임에 넘기지 않습니다. EZ2DJ 기본 매핑에서 Enter는 2P 턴테이블(`p2_negative`)입니다. Alt+1..3은 지금처럼 키도 게임에 보입니다.
* **OSD**: "Fullscreen"과 "Keep aspect ratio" 토글을 맨 앞에 등록합니다(게임 컨트롤, 32-bit color보다 앞). OSD는 present 도중 그려지므로 전체 화면 토글은 요청만 남기고, `SdlHostPresentation::Present`가 백엔드 present를 마친 뒤 적용합니다. 비율 유지는 백엔드 값만 바꾸므로 바로 적용합니다. 전체 화면 체크박스는 현재 창 상태를 보여 줍니다.
* **저장 통지**: `SdlHostPresentation`에 `SetDisplayPreferencesObserver(std::function<void(bool fullscreen, bool keep_aspect)>)`를 둡니다. **사용자 조작**(OSD, Alt+Enter, 더블클릭)으로 값이 실제로 바뀌었을 때만 부릅니다. 시작 시 적용, 창이 거절해 되돌린 변경, Alt+1..3 배율은 부르지 않습니다.
* CLI는 이 observer에서 `cfg/re2dj.ini`를 다시 읽어 두 키만 바꿔 저장합니다(`SaveDisplayPreferences`). 다른 키는 그대로이고, 실패하면 경고만 남깁니다. 런처를 거친 실행에서는 런처가 게임이 끝난 뒤 파일을 다시 읽으므로, 게임 중 바꾼 값이 런처 화면과 다음 실행으로 이어집니다. 런처는 게임 중에는 파일을 쓰지 않습니다.

### 4. 런처

* `LauncherSettings`에 `keep_aspect`(`[Video] keep_aspect`)를 더하고, 고른 값이면 자식에 `--keep-aspect`/`--stretch`를 넘깁니다.
* Options 맨 위에 "Fullscreen"(기존)과 "Keep aspect ratio"를 둡니다.
* 런처 창: 열 때 `fullscreen`이 켜져 있으면 전체 화면으로 열고, 체크박스나 Alt+Enter로 바꾸면 그 자리에서 적용합니다. Alt+Enter는 ImGui로 넘기지 않습니다(Enter가 행을 시작하지 않도록). 런처 표에서는 더블클릭이 시작이므로 더블클릭 전환은 두지 않습니다.
* 비율 유지가 켜져 있으면 기본 창(960×640, 3:2) 비율의 가장 큰 영역을 가운데에 두고 나머지는 검은 띠로 칠하며, 글자 배율은 그 영역의 높이를 따릅니다. 끄면 창 전체를 씁니다. 같은 `ComputePresentRect`를 씁니다.
* 표에서 Space는 더 이상 시작하지 않습니다(Enter·키패드 Enter·패드 A·더블클릭만).

## 바꾸지 않는 것

* 전체 화면 방식(모니터 크기의 테두리 없는 창, 디스플레이 모드 변경 없음), Alt+1..3 배율, 더블클릭 전환.
* OSD의 다른 항목(32-bit color, 셰이더, 게임 컨트롤)은 지금처럼 그 실행 안에서만 바뀝니다.

## 검증

* 단위 테스트: `ComputePresentRect`(켬은 `FitPresentation`과 같음, 끔은 창 전체), 설정의 `keep_aspect` 왕복·잘못된 값·자식 인자, `SaveDisplayPreferences`가 두 키만 바꾸고 다른 키를 지키는지, 시작 값 우선순위.
* 실제 실행(Linux x64·x86): 게임에서 OSD 두 체크박스와 Alt+Enter·더블클릭, 비율 유지를 끈 그림과 마우스 위치, `cfg/re2dj.ini` 갱신과 다음 실행 반영, 인자의 우선. 런처에서 체크박스·Alt+Enter·전체 화면으로 열기·비율 유지 배치, Space가 시작하지 않음, 패드 A로 시작.
* Windows x86: CI 빌드·테스트.

---

# #14 Design: fullscreen and keep-aspect options kept in `cfg/re2dj.ini`, and the launcher's pad behaviour

Issue: [#14](https://github.com/reexec/re2DJ/issues/14) · Reference: rePIU v0.0.214 (#45) and v0.0.212 (#34) · Builds on: [#12 launcher](20261010-i012-launcher.md)

## Background

* The game window always keeps the original shape: `FitPresentation` picks the largest 4:3 rectangle in the window and the rest is black bars (the backend's drawing and `host_presentation`'s mouse mapping share the function).
* Fullscreen starts from `--fullscreen`/`--windowed` (else the profile default) and changes in game only through a left double click; the state is not kept and the OSD has no item for it. Alt+1..3 set the window scale.
* The launcher (#12) always opens windowed and draws over the whole window; on its table Enter, Space, the pad's A and a double click start a row.

rePIU settled this in v0.0.214 (#45), and the user chose to bring the same to re2DJ (2026-10-10).

## Goals

1. **Keep aspect ratio** (on by default): on, 4:3 with black bars as now; off, the picture fills the window (or the screen in fullscreen), windowed mode included.
2. The OSD switches **Fullscreen** and **Keep aspect ratio** in game, both at the top of its items.
3. **Alt+Enter** in the game window switches fullscreen, as a double click does.
4. Values the **user changes** through the OSD, Alt+Enter or a double click are saved to `[Video] fullscreen` and `keep_aspect` in `cfg/re2dj.ini`, and the next run starts with them.
5. The launcher puts both checkboxes at the top of Options, and its window follows both (opening fullscreen, Alt+Enter, and with keep-aspect the default window's shape centred with black bars). On its table Enter, the pad's A and a double click start a row, and Space only selects.

## Decisions

### 1. Start-up precedence

| Value | First | Second | Third |
|---|---|---|---|
| Fullscreen | `--fullscreen` / `--windowed` | `cfg/re2dj.ini` `[Video] fullscreen` | the profile default |
| Keep aspect | `--keep-aspect` / `--stretch` (new) | `cfg/re2dj.ini` `[Video] keep_aspect` | on |

As in rePIU, runs with arguments read the settings file too; the command line, the most direct instruction, wins. The file is `cfg/re2dj.ini` in the current directory, the launcher's file read by the launcher's parser (`launcher_settings`). A missing file or a bad value falls through to the next rank (a bad value with a warning).

`--keep-aspect`/`--stretch` exist so the launcher hands the chosen value to its child on the command line like every other option; it then applies to that run even when saving failed (a read-only directory, say).

### 2. The picture rectangle

`window_policy.h` gains `ComputePresentRect(window_w, window_h, logical_w, logical_h, keep_aspect)`: `FitPresentation` when on, the whole window when off. The backend's drawing and the mouse mapping both use it, so the picture and the pointer agree. The backend takes the value through `SetKeepAspect(bool)` and applies it from the next present; the presentation filter (`SelectPresentationFilter`) is still chosen from the picture rectangle's size.

### 3. Game-window input and the OSD

* **Alt+Enter** (main or keypad, not repeated) switches fullscreen, and the Enter is not passed to the game: in EZ2DJ's default mapping Enter is the 2P turntable (`p2_negative`). Alt+1..3 keep showing their keys to the game.
* **OSD**: "Fullscreen" and "Keep aspect ratio" toggles are registered first, before the game controls and 32-bit color. The OSD draws during the present, so the fullscreen toggle only leaves a request, applied by `SdlHostPresentation::Present` after the backend's present; keep-aspect only changes a backend value and applies at once. The fullscreen checkbox shows the window's current state.
* **Save notice**: `SdlHostPresentation` gains `SetDisplayPreferencesObserver(std::function<void(bool fullscreen, bool keep_aspect)>)`, called only when a **user action** (the OSD, Alt+Enter, a double click) actually changed a value; not for the start-up state, a change the window refused and that was undone, or Alt+1..3.
* The CLI's observer re-reads `cfg/re2dj.ini` and saves it with just the two keys changed (`SaveDisplayPreferences`), other keys untouched and a failure only warned about. On a run through the launcher, the launcher re-reads the file after the game, so values changed in game carry on to its screen and the next run; the launcher writes nothing while the game runs.

### 4. The launcher

* `LauncherSettings` gains `keep_aspect` (`[Video] keep_aspect`), passed to the child as `--keep-aspect`/`--stretch` when chosen.
* Options start with "Fullscreen" (existing) and "Keep aspect ratio".
* The launcher window opens fullscreen when `fullscreen` is on, and a change through the checkbox or Alt+Enter applies on the spot. Alt+Enter is not passed to ImGui, so Enter does not start a row; a double click starts a row on the table, so it is no toggle here.
* With keep-aspect on, the largest area of the default window's shape (960×640, 3:2) is centred and the rest painted black, the text scale following that area's height; off, the whole window is used. Both use `ComputePresentRect`.
* Space no longer starts a row on the table (Enter, keypad Enter, the pad's A and a double click do).

## Unchanged

* The fullscreen kind (a monitor-sized borderless window, no display-mode change), Alt+1..3 scales and the double-click switch.
* The OSD's other items (32-bit color, shaders, game controls) still change only within the run.

## Verification

* Unit tests: `ComputePresentRect` (on equals `FitPresentation`, off is the whole window); the settings' `keep_aspect` round trip, bad values and child arguments; `SaveDisplayPreferences` changing only the two keys; the start-up precedence.
* Real runs (Linux x64 and x86): in game, the two OSD checkboxes, Alt+Enter and the double click, the stretched picture and the mouse position, `cfg/re2dj.ini` updated and the next run following it, and the command line winning; in the launcher, the checkboxes, Alt+Enter, opening fullscreen, the keep-aspect layout, Space not starting, and the pad's A starting.
* Windows x86: CI build and tests.
