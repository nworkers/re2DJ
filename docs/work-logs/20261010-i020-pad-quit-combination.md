# #20 작업 로그 — 패드 조합 LT+RT+L3+R3 1초로 게임·런처 종료 / #20 work log — ending the game and the launcher with LT+RT+L3+R3 held for a second

설계: [20261010-i020-pad-quit-combination.md](../design/20261010-i020-pad-quit-combination.md) · 지시서: [20261010-i020-pad-quit-combination.md](../work-orders/20261010-i020-pad-quit-combination.md)

## 2026-10-10

- **1차**: 게임 창에서 합친 패드 상태에 네 입력이 있으면 바로 `close_requested_`를 켜도록 만들었다(`IsQuitCombination`).
- **사용자 요청으로 rePIU #52와 같게 고침**: rePIU의 로컬 작업(설계 `20261010-i052-pad-exit-chord.md`, `pad_exit_chord.{h,cpp}`)을 읽고 조건을 맞췄다. 한 패드에서 네 입력, 1초 연속 유지로 한 번 발동하고 떼야 다시 준비, 런처에서도 같은 조합으로 종료, 입력은 게임에 그대로 전달한다.
  - `re2dj/input/pad_exit_chord.h`: `IsPadExitChordDown`(한 패드·패드 목록), `PadExitChordTimer`(1000ms). 헤더만이고 SDL을 쓰지 않는다.
  - `Sdl3GamepadReader::ReadEach()`. `Read()`는 그것을 합친다.
  - 게임: `SdlHostPresentation::Present`가 패드를 읽어 합친 상태를 게스트에 주고(입력은 그대로), `ReadEach()`로 타이머를 갱신해 발동하면 `close_requested_`와 로그를 남긴다.
  - 런처: `launcher_window.cpp`가 리더를 열고 SDL 패드 이벤트를 넘기며, 프레임마다 타이머를 갱신해 발동하면 `kQuit`와 로그를 남긴다. `re2dj_launcher_ui`가 `re2dj_sdl3_gamepad`·`re2dj_logging`을 링크한다.
- 문서: README 게임패드 절, 런처 업데이트 가이드의 스팀덱 절, IMPLEMENTED.

  *First pass: the game window set `close_requested_` at once when the merged pad state held the four (`IsQuitCombination`). At the user's request it now matches rePIU #52, after reading rePIU's local work (design `20261010-i052-pad-exit-chord.md`, `pad_exit_chord.{h,cpp}`): the four on one pad, an unbroken second's hold firing once and re-armed only by a release, the same chord in the launcher, and the input left flowing to the game. `re2dj/input/pad_exit_chord.h` holds `IsPadExitChordDown` (one pad, or a list of pads) and `PadExitChordTimer` (1000 ms), header-only without SDL; `Sdl3GamepadReader::ReadEach()` is new and `Read()` merges it. In game, `SdlHostPresentation::Present` hands the merged state to the guest as before and updates the timer from `ReadEach()`, setting `close_requested_` with a log line when it fires; the launcher's `launcher_window.cpp` opens a reader, passes it SDL's pad events and updates the timer each frame, returning `kQuit` with a log line when it fires, `re2dj_launcher_ui` now linking `re2dj_sdl3_gamepad` and `re2dj_logging`. Documents: the README's gamepad section, the launcher update guide's Steam Deck section, IMPLEMENTED.*

## 검증 / Verification

- 단위 테스트 `CheckExitChord`: 한 패드의 네 입력(하나씩 빠짐, 다른 버튼과 함께), 두 패드에 나눔은 아님, 빈 목록, 타이머(1초 직전·정각, 한 번만, 떼면 다시 준비, 1초 전에 떼면 발동 없음). x64·x86 Debug(경고를 오류로)와 clang: 빌드, CTest 각 5개 통과, checks 6,386, failures 0.
- 실제 실행(Linux x64 Debug, Xbox Series X Controller, 사용자 조작): 런처에서 6th 시작 → 게임 중 조합 유지 → `input: gamepad exit chord (LT+RT+L3+R3) held for 1 s` → `continuation: host window closed` → `launcher: ez2dj6th ended with exit code 0`. 다시 열린 런처에서 조합 유지 → `launcher: gamepad exit chord (LT+RT+L3+R3) held for 1 s` → `launcher: closed`, 프로세스 종료. 1초 전에 뗀 경우는 로그가 남지 않아 단위 테스트로만 확인했다.
- 스팀덱 실제 확인은 사용자 환경에서 한다. Windows는 push 뒤 CI로 확인한다.

  *Unit test `CheckExitChord`: the four on one pad (each one missing, with other buttons), not when split across two pads, an empty list, and the timer (just before and at a second, once only, re-armed by a release, nothing when let go earlier); x64 and x86 Debug (warnings as errors) and clang build, pass 5 CTest tests each and report 6,386 checks with 0 failures. Real run (Linux x64 Debug, an Xbox Series X Controller, the user operating): 6th started from the launcher, the chord held in game gave `input: gamepad exit chord (LT+RT+L3+R3) held for 1 s`, `continuation: host window closed` and `launcher: ez2dj6th ended with exit code 0`; held in the reopened launcher, `launcher: gamepad exit chord (LT+RT+L3+R3) held for 1 s`, `launcher: closed` and the process ended. Letting go before a second leaves no log line, so it was checked by the unit test only. A check on a real Steam Deck is the user's; Windows is checked by CI after the push.*
