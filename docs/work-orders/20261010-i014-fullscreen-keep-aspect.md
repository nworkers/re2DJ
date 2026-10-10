# #14 작업 지시서 — 전체 화면·비율 유지 옵션과 저장, 런처 게임패드 동작 / #14 work order — fullscreen and keep-aspect options, kept in cfg, and launcher pad behaviour

이슈: [#14](https://github.com/reexec/re2DJ/issues/14) · 설계: [20261010-i014-fullscreen-keep-aspect.md](../design/20261010-i014-fullscreen-keep-aspect.md)

## 절차 / Steps

1. `window_policy.h`에 `ComputePresentRect`. 백엔드 `SetKeepAspect`와 present, `host_presentation`의 마우스 변환이 이를 쓴다.
   *`ComputePresentRect` in `window_policy.h`, used by the backend's `SetKeepAspect` and present and by `host_presentation`'s mouse mapping.*
2. `launcher_settings`: `keep_aspect` 키, `--keep-aspect`/`--stretch` 자식 인자, `SaveDisplayPreferences`(두 키만 바꿔 저장).
   *`launcher_settings`: the `keep_aspect` key, the `--keep-aspect`/`--stretch` child arguments, and `SaveDisplayPreferences` (saving with just the two keys changed).*
3. `SdlHostPresentation`: 시작 비율 유지 값, Alt+Enter(Enter는 게임에 넘기지 않음), OSD 두 토글(맨 앞, 전체 화면은 present 뒤 적용), 사용자 조작 때만 부르는 observer.
   *`SdlHostPresentation`: the start-up keep-aspect value, Alt+Enter (Enter withheld from the game), the two OSD toggles (first; fullscreen applied after the present), and an observer called only for user actions.*
4. CLI: `--keep-aspect`/`--stretch`, `cfg/re2dj.ini`의 시작 값(명령줄 > 파일 > 기본값), observer에서 저장. 사용법 문구.
   *CLI: `--keep-aspect`/`--stretch`, start-up values from `cfg/re2dj.ini` (command line, then file, then default), saving from the observer, and the usage text.*
5. 런처: Options 맨 위의 두 체크박스, 창의 전체 화면·Alt+Enter·비율 유지 배치, Space 시작 제거.
   *Launcher: the two checkboxes at the top of Options, the window's fullscreen, Alt+Enter and keep-aspect layout, and Space no longer starting.*
6. 단위 테스트와 문서(README, ARCHITECTURE, IMPLEMENTED).
   *Unit tests and documents (README, ARCHITECTURE, IMPLEMENTED).*
7. 검증: Linux x64·x86 Debug(경고를 오류로)·Release·clang 빌드와 CTest, 실제 실행, push 뒤 CI.
   *Verification: Linux x64 and x86 Debug (warnings as errors), Release and clang builds with CTest, real runs, and CI after the push.*

## 완료 조건 / Done when

게임과 런처에서 전체 화면과 비율 유지를 바꿀 수 있고, 게임 중 바꾼 값이 `cfg/re2dj.ini`에 남아 다음 실행에 적용되며, 런처 표에서 Space가 시작하지 않고, 모든 타깃 빌드와 테스트가 통과한다.

*Fullscreen and keep-aspect can be changed in the game and the launcher, values changed in game stay in `cfg/re2dj.ini` for the next run, Space does not start a row on the launcher's table, and every target builds and passes its tests.*
