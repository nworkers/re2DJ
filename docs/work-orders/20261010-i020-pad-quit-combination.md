# #20 작업 지시서 — 패드 조합으로 게임 종료 / #20 work order — ending the game with a pad combination

이슈: [#20](https://github.com/reexec/re2DJ/issues/20) · 설계: [20261010-i020-pad-quit-combination.md](../design/20261010-i020-pad-quit-combination.md)

## 절차 / Steps

1. `input/pad_exit_chord.h`(`IsPadExitChordDown`, `PadExitChordTimer`), 리더의 `ReadEach()`, 단위 테스트.
   *`input/pad_exit_chord.h` (`IsPadExitChordDown`, `PadExitChordTimer`), the reader's `ReadEach()`, and unit tests.*
2. 게임(`SdlHostPresentation::Present`)과 런처 창에서 1초 유지 시 종료와 로그. 게임 입력은 막지 않음.
   *In the game (`SdlHostPresentation::Present`) and the launcher window, quit with a log after a second's hold, the game's input left alone.*
3. 문서: README 게임패드 절, 런처 업데이트 가이드의 스팀덱 절, IMPLEMENTED.
   *Documents: the README's gamepad section, the launcher update guide's Steam Deck section, IMPLEMENTED.*
4. 검증: 빌드·테스트, 패드로 실제 실행.
   *Verification: builds and tests, and a real run with a pad.*

## 완료 조건 / Done when

한 패드에서 LT+RT+L3+R3을 1초 누르면 게임은 끝나 런처로 돌아오고 런처는 종료되며, 모든 타깃 빌드와 테스트가 통과한다.

*LT+RT+L3+R3 held on one pad for a second ends the game back to the launcher and quits the launcher, and every target builds and passes its tests.*
