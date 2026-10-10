# #20 설계: 패드 조합 LT+RT+L3+R3 1초로 게임·런처 종료

이슈: [#20](https://github.com/reexec/re2DJ/issues/20) · 관련: [#12 런처](20261010-i012-launcher.md), [#17 런처 업데이트](20261010-i017-launcher-self-update.md)

## 배경

스팀덱 게임 모드에는 패드만 있고 창 닫기 버튼이나 Alt+F4가 없습니다. 그래서 게임 창을 닫아 런처로 돌아가거나 끝낼 방법이 없습니다. 게임 창을 닫으면 `SdlHostPresentation`이 `close_requested_`를 켜고, 러너가 `continuation: host window closed`로 끝나며, 런처에서 시작한 게임이면 런처가 다시 열립니다.

## 결정 (사용자, 2026-10-10: rePIU #52와 같게)

| 질문 | 결정 |
|---|---|
| 발동 | **한 패드에서** LT + RT + 왼쪽 스틱 클릭(L3) + 오른쪽 스틱 클릭(R3)을 **1초 동안** 함께 누르고 있으면 |
| 런처 | 런처에서도 같은 조합으로 런처를 닫아 re2DJ를 끝냄(Quit과 같음) |
| 게임 입력 | 조합 중에도 각 버튼은 평소처럼 게임에 그대로 전달됨(LT·RT는 기본 매핑에서 이펙터 3·4) |

* **판정(플랫폼 공용, `re2dj/input/pad_exit_chord.h`)**: `IsPadExitChordDown`은 한 패드가 두 트리거(기존 규칙대로 절반 이상)와 두 스틱 클릭을 모두 누르고 있으면 참입니다. 여러 패드에 나눠 누른 것은 세지 않습니다. `PadExitChordTimer::Update(down, now_ms)`는 이어서 1초가 되는 순간 한 번 참을 돌려주고, 떼기 전에는 다시 발동하지 않습니다. 1초 전에 떼면 아무 일도 없습니다.
* **패드별 상태**: `Sdl3GamepadReader::ReadEach()`가 열린 패드마다 상태를 돌려주고, `Read()`는 그것을 합칩니다.
* **게임**: `SdlHostPresentation::Present`가 매 프레임 패드를 읽은 뒤 타이머를 갱신합니다. 발동하면 `input: gamepad exit chord (LT+RT+L3+R3) held for 1 s`를 남기고, 창 닫기와 같은 `close_requested_`를 켭니다. 이후 경로(러너 종료, 런처 복귀)는 창 닫기와 같습니다.
* **런처**: 런처 창이 같은 리더를 열어 SDL 패드 이벤트를 넘기고, 프레임마다 타이머를 갱신합니다. 발동하면 `launcher: gamepad exit chord …`를 남기고 Quit과 같이 닫습니다. ImGui의 SDL3 backend도 같은 패드를 열지만 SDL이 여는 횟수를 셉니다.
* 키보드·마우스 입력과 `--io-config` 매핑은 바꾸지 않습니다.

## 검증

* 단위 테스트: 한 패드의 네 입력(하나 빠짐, 다른 버튼과 함께), 두 패드에 나눔, 타이머(1초 전·후, 한 번만, 떼면 다시 준비).
* 실제 실행(Linux, 패드): 게임 중 1초 유지 → 끝나고 런처 복귀, 1초 전에 떼면 계속, 런처에서 1초 유지 → 종료.

---

# #20 Design: ending the game and the launcher with LT+RT+L3+R3 held for a second

Issue: [#20](https://github.com/reexec/re2DJ/issues/20) · Related: [#12 launcher](20261010-i012-launcher.md), [#17 launcher update](20261010-i017-launcher-self-update.md)

## Background

Steam Deck game mode has only the pad, with no close button or Alt+F4, so a game window cannot be closed to return to the launcher or quit. Closing the game window makes `SdlHostPresentation` set `close_requested_`, the runner ends with `continuation: host window closed`, and a game started from the launcher brings the launcher back.

## Decisions (the user, 2026-10-10: as rePIU #52 has it)

| Question | Decision |
|---|---|
| Trigger | LT + RT + left stick click (L3) + right stick click (R3) held together **on one pad** for **one second** |
| Launcher | The same chord closes the launcher and ends re2DJ, as Quit does |
| Game input | While held, each control still reaches the game as usual (LT and RT are effectors 3 and 4 by default) |

* **Test (shared, `re2dj/input/pad_exit_chord.h`)**: `IsPadExitChordDown` is true when one pad holds both triggers (past half, as before) and both stick clicks; the four split across pads do not count. `PadExitChordTimer::Update(down, now_ms)` is true once, at the moment an unbroken hold reaches a second, and not again until released; letting go earlier does nothing.
* **Per-pad state**: `Sdl3GamepadReader::ReadEach()` gives each open pad's state, which `Read()` merges.
* **Game**: `SdlHostPresentation::Present` updates the timer after reading the pads each frame; firing logs `input: gamepad exit chord (LT+RT+L3+R3) held for 1 s` and sets `close_requested_`, as closing the window does, with the same path after (the runner ending, the launcher returning).
* **Launcher**: the launcher window opens the same reader, passes it SDL's pad events and updates the timer each frame; firing logs `launcher: gamepad exit chord …` and closes it as Quit does. ImGui's SDL3 backend opens the same pads, and SDL counts the opens.
* Keyboard and mouse input and `--io-config` mappings are unchanged.

## Verification

* Unit tests: the four on one pad (one missing, with other buttons), split across two pads, and the timer (before and after a second, once only, ready again after a release).
* Real runs (Linux, a pad): held a second in game, the game ends and the launcher returns; let go earlier, play goes on; held a second in the launcher, it quits.
