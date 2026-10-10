# #14 작업 로그 — 전체 화면·비율 유지 옵션과 저장, 런처 게임패드 동작 / #14 work log — fullscreen and keep-aspect options, kept in cfg, and launcher pad behaviour

설계: [20261010-i014-fullscreen-keep-aspect.md](../design/20261010-i014-fullscreen-keep-aspect.md) · 지시서: [20261010-i014-fullscreen-keep-aspect.md](../work-orders/20261010-i014-fullscreen-keep-aspect.md)

## 2026-10-10 — 조사 / Investigation

- rePIU `f21ac2e62`(v0.0.214, #45)와 `6e9cba349`(v0.0.212, #34)를 읽었다. #45: OSD·런처의 "Fullscreen"·"Keep aspect ratio", 비율 유지 끔이면 그림이 drawable 전체, 사용자 조작으로 바뀐 값만 세션 sink로 `cfg/repiu.ini`에 저장, 런처 창의 전체 화면·Alt+Enter·기본 창 비율 배치. #34의 런처 부분: `SDL_INIT_GAMEPAD`, South(A)로 행 시작, Space는 선택만.
- re2DJ는 `FitPresentation`(백엔드 그리기와 마우스 변환)으로 늘 비율을 지키고, 더블클릭만 전체 화면을 바꾸며 저장하지 않았다. 런처(#12)는 이미 `SDL_INIT_GAMEPAD`와 패드 A 시작이 있었고, Space도 시작했다. EZ2DJ 기본 매핑에서 Enter는 2P 턴테이블(`p2_negative`)이라 Alt+Enter의 Enter는 게임에 넘기지 않기로 했다.

  *Read rePIU `f21ac2e62` (v0.0.214, #45) and `6e9cba349` (v0.0.212, #34). #45: "Fullscreen" and "Keep aspect ratio" in the OSD and the launcher, the picture over the whole drawable with keep-aspect off, only values changed by user actions saved to `cfg/repiu.ini` through a session sink, and the launcher window's fullscreen, Alt+Enter and default-shape layout; #34's launcher part: `SDL_INIT_GAMEPAD`, South (A) starting a row, Space only selecting. re2DJ always kept the shape through `FitPresentation` (the backend's drawing and the mouse mapping), switched fullscreen only by a double click and kept nothing; the launcher (#12) already had `SDL_INIT_GAMEPAD` and the pad's A starting, and Space started too. In EZ2DJ's default mapping Enter is the 2P turntable (`p2_negative`), so Alt+Enter's Enter is withheld from the game.*

## 2026-10-10 — 구현 / Implementation

- `graphics/window_policy.h`: `ComputePresentRect`(켬은 `FitPresentation`, 끔은 창 전체). 백엔드 `SetKeepAspect`/`keep_aspect`와 present, `host_presentation`의 마우스 변환이 쓴다.
- `launcher_settings`: `[Video] keep_aspect`, 자식 인자 `--keep-aspect`/`--stretch`, `ResolveDisplayPreferences`(명령줄 → 파일 → 기본값), `SaveDisplayPreferences`(파일을 다시 읽어 두 키만 바꿔 저장).
- `SdlHostPresentation`: `SetStartKeepAspect`, `SetDisplayPreferencesObserver`, OSD 맨 앞의 "Fullscreen"·"Keep aspect ratio" 토글(전체 화면은 present 뒤 적용), Alt+Enter(반복 제외, Enter는 게임에 안 넘김), 더블클릭. 사용자 조작으로 실제로 바뀌었을 때만 observer를 부른다(`ChangeWindowMode`가 되돌린 경우 제외).
- CLI: `--keep-aspect`/`--stretch`, 시작 때 `cfg/re2dj.ini`를 읽어 시작 값을 정하고 `display : …` 로그, observer에서 저장하고 `(kept in cfg/re2dj.ini)` 로그, 사용법 문구.
- 런처: Options 맨 위에 "Fullscreen"·"Keep aspect ratio"와 설명, 창이 설정의 전체 화면을 따름(시작 시·체크박스·Alt+Enter, Alt+Enter는 ImGui로 안 넘김), 비율 유지 시 960×640 비율 영역을 가운데에 두고 바깥은 검게, 글자 배율은 영역 높이 기준. 표에서 Space 시작 제거.
- 단위 테스트: `window_policy_test`(`ComputePresentRect`), `launcher_test`(`keep_aspect` 파싱·잘못된 값·왕복, 자식 인자, 시작 값 우선순위, `SaveDisplayPreferences`가 다른 키를 지킴, 파일이 없을 때 생성).
- 문서: README(OSD, 런처, 명령행), `docs/IMPLEMENTED.md`.

  *`graphics/window_policy.h` gains `ComputePresentRect` (`FitPresentation` when on, the whole window when off), used by the backend's `SetKeepAspect`/`keep_aspect` and present and by `host_presentation`'s mouse mapping. `launcher_settings` gains `[Video] keep_aspect`, the child arguments `--keep-aspect`/`--stretch`, `ResolveDisplayPreferences` (command line, then file, then default) and `SaveDisplayPreferences` (re-reading the file and saving it with just the two keys changed). `SdlHostPresentation` gains `SetStartKeepAspect`, `SetDisplayPreferencesObserver`, "Fullscreen" and "Keep aspect ratio" OSD toggles first in the list (fullscreen applied after the present), Alt+Enter (not repeated, its Enter withheld from the game) and the double click, calling the observer only when a user action actually changed a value (not when `ChangeWindowMode` undid it). The CLI gains `--keep-aspect`/`--stretch`, reads `cfg/re2dj.ini` at start for the start-up values with a `display : …` log line, saves from the observer with `(kept in cfg/re2dj.ini)`, and updates the usage text. The launcher puts "Fullscreen" and "Keep aspect ratio" with explanations at the top of Options; its window follows the stored fullscreen (at start, the checkbox and Alt+Enter, which never reaches ImGui), with keep-aspect lays out a centred 960×640-shaped area with black outside and the text scale following its height, and Space no longer starts a row. Unit tests: `window_policy_test` (`ComputePresentRect`) and `launcher_test` (`keep_aspect` parsing, bad values and round trip, child arguments, start-up precedence, `SaveDisplayPreferences` keeping other keys and creating a missing file). Documents: the README (OSD, launcher, command line) and `docs/IMPLEMENTED.md`.*

## 검증 / Verification

- 빌드·테스트: `linux-x64-debug`·`linux-x86-debug`·clang(경고를 오류로), `linux-x64-release`·`linux-x86-release` 빌드 성공, CTest 각 5개 통과, `re2dj_unit_tests` checks 6,295, failures 0.
- 시작 값(6th, 조작 없음): 설정에 두 키가 없을 때 `windowed, keep aspect`, `--stretch --fullscreen`이면 `fullscreen, stretched`, 파일에 `fullscreen=1`·`keep_aspect=0`이면 `fullscreen, stretched`, 거기에 `--windowed`면 `windowed, stretched`. 조작이 없으면 파일은 바뀌지 않았다.
- 런처 + 6th(x64 Debug, 사용자 조작): 런처가 고른 값을 자식에 넘겼다(`--windowed --color-depth 32 --post-shader=scanline --audio-gain-db -6`). 게임 중 전체 화면·창, 비율 유지·늘림을 오갈 때마다 `display : … (kept in cfg/re2dj.ini)`가 남았고(8번), 파일에는 마지막 값과 런처의 다른 키(색 깊이, 셰이더, 소리, 마지막 프로필)가 함께 남았다. 게임 뒤 런처가 다시 열리고 Quit할 때 런처에서 바꾼 전체 화면 값이 저장됐다.
- 3rd는 시작 직후 `user32.dll!GetClientRect`(resolve-only)에서 멈췄다. 변경 전 v0.0.68 Release에서도 같아 이 작업과 무관하다.
- 남은 것: 런처의 Alt+Enter·비율 유지 배치·Space·패드 A, 게임의 늘린 화면과 Alt+Enter 때 턴테이블이 눌리지 않는지의 화면 확인은 사용자 답을 받지 못했다. 사용자가 커밋을 먼저 요청했고, 게임패드 매핑은 이어서 추가로 고치기로 했다. Windows x86은 push 뒤 CI로 확인한다.

  *Builds and tests: `linux-x64-debug`, `linux-x86-debug` and clang (warnings as errors) and `linux-x64-release` and `linux-x86-release` build, pass 5 CTest tests each, and report 6,295 checks with 0 failures. Start-up values (6th, no input): `windowed, keep aspect` with neither key stored, `fullscreen, stretched` with `--stretch --fullscreen`, `fullscreen, stretched` with `fullscreen=1` and `keep_aspect=0` in the file, and `windowed, stretched` adding `--windowed`; with no input the file stayed unchanged. Launcher with 6th (x64 Debug, the user operating): the launcher passed its choices to the child (`--windowed --color-depth 32 --post-shader=scanline --audio-gain-db -6`); every switch in game between fullscreen and windowed and between keeping and stretching logged `display : … (kept in cfg/re2dj.ini)` (eight times), and the file held the last values with the launcher's other keys (colour depth, shader, gain, last profile); the launcher came back after the game and saved its own fullscreen change on Quit. 3rd stopped right after start at `user32.dll!GetClientRect` (resolve-only), as it does on the unchanged v0.0.68 Release, so that is unrelated. Left: the user has not yet answered on the visual checks (the launcher's Alt+Enter, keep-aspect layout, Space and pad A; the stretched game picture and no turntable press on Alt+Enter); the user asked to commit first and to change the gamepad mapping further next; Windows x86 is checked by CI after the push.*

## 2026-10-10 — EZ2DJ 게임패드 기본값 / EZ2DJ gamepad defaults

- 사용자가 `config/ez2dj-io.example.ini`의 `[gamepad]`를 고쳐 두고 그 값을 기본값으로 올리라고 했다. `ez2dj_keyboard_map.cpp`의 표를 같게 바꿨다: 1P 1~5번 키 `DPAD_LEFT` `DPAD_UP` `A` `Y` `B`, 페달 `X`, 이펙터 1~4 `LB` `RB` `LT` `RT`. 시작·코인·턴테이블은 그대로다. 두 예시 파일의 "Linux host" 문구를 "both hosts"로, README의 패드 설명을 고쳤다.
- `gamepad_bindings_test`의 기본값 확인을 새 값으로 바꾸고 페달·이펙터 확인을 더했다. 예시 파일과 기본값이 같은지는 기존 테스트가 확인한다. x64·x86 Debug와 clang: 빌드, CTest 5개 통과, checks 6,297, failures 0.

  *The user edited `[gamepad]` in `config/ez2dj-io.example.ini` and asked for those values to become the defaults. The table in `ez2dj_keyboard_map.cpp` now matches: player 1's keys 1 to 5 `DPAD_LEFT` `DPAD_UP` `A` `Y` `B`, the pedal `X`, effectors 1 to 4 `LB` `RB` `LT` `RT`; start, coin and the turntable are unchanged. Both example files' "Linux host" note became "both hosts", and the README's pad description was updated. `gamepad_bindings_test` checks the new defaults and adds the pedal and an effector; the existing test checks that the example file equals the defaults. x64 and x86 Debug and clang build, pass 5 CTest tests, and report 6,297 checks with 0 failures.*

## 2026-10-10 — 사용자 화면 확인 / User's visual check

- 사용자가 화면으로 확인할 항목이 모두 잘 동작한다고 알려 주었다: 런처의 Alt+Enter 전체 화면 전환, 비율 유지 시 가운데 3:2 영역과 검은 띠, Space가 시작하지 않음, 패드 A로 시작, 게임에서 비율 유지를 끈 늘린 화면, Alt+Enter 때 2P 턴테이블이 눌리지 않음.

  *The user reported that every visual check works: the launcher's Alt+Enter fullscreen switch, the centred 3:2 area with black bars when keeping aspect, Space not starting, the pad's A starting, the stretched game picture with keep-aspect off, and no 2P turntable press on Alt+Enter.*

