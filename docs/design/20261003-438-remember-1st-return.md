# 작업 438 설계 — Remember 1st에서 6th로 돌아가기 / Task 438 design — returning from Remember 1st to 6th

선행: [작업 434 설계](20261001-434-remember-1st.md), [작업 437 설계](20261003-437-remember-1st-handoff-files.md)

## 배경 / Background

Linux에서 Remember 1st로 게임을 마치자 6th로 돌아가지 않고 re2dj가 끝났다. 1st 자식은 445,027번째 호출 `ddraw!IDirectDraw4::RestoreDisplayMode`(`0x0041473c`)에서 멈췄고, launcher는 `ExitProcess(0)`으로 끝났다(실행 `20261003-111451-799`, `20261003-111521-062`).

원본에서 확인한 1st의 정상 종료는 다음과 같다(자세한 내용은 [분석](../analysis/ez2dj5th-6th-chd-filesystem.md)).

1. 게임 한 판 뒤 `[0x01b0c2c4] = 1`을 쓴다.
2. 정리 루틴 `0x0041ebb0`이 사운드를 해제하고, Direct3D/DirectDraw 해제 중에 `RestoreDisplayMode`를 부른다.
3. bookkeeping을 저장한다.
4. `0x004219c0`이 종료 코드를 0x105로 정한다.
5. launcher는 0x105를 받으면 6th를 다시 실행한다.

작업 434에서 1st의 끝을 `ExitProcess(0)`으로 본 것은 오류 종료 경로였다.

*On Linux, finishing a game in Remember 1st ended re2dj instead of returning to 6th: the 1st child stopped at call 445,027, `ddraw!IDirectDraw4::RestoreDisplayMode` (`0x0041473c`), and the launcher ended with `ExitProcess(0)` (runs `20261003-111451-799`, `20261003-111521-062`). In the original, after one game 1st writes `[0x01b0c2c4] = 1`; its clean-up `0x0041ebb0` releases sound and, while releasing Direct3D and DirectDraw, calls `RestoreDisplayMode`; it saves bookkeeping; `0x004219c0` sets the exit code to 0x105; and the launcher runs 6th again on 0x105 (details in the [analysis](../analysis/ez2dj5th-6th-chd-filesystem.md)). Task 434's `ExitProcess(0)` ending was the error path.*

## 결정 / Decisions

- Linux facade의 `IDirectDraw4::RestoreDisplayMode`와 `IDirectDraw7::RestoreDisplayMode`는 `DD_OK`를 돌려준다. Windows DX6 facade(`RootRestoreDisplayMode`)와 같다. re2DJ는 host 디스플레이 모드를 바꾸지 않으므로(`SetDisplayMode`는 값만 기록한다) 되돌릴 것이 없다.
  *The Linux facade's `IDirectDraw4::RestoreDisplayMode` and `IDirectDraw7::RestoreDisplayMode` return `DD_OK`, as the Windows DX6 facade's `RootRestoreDisplayMode` does: re2DJ never changes the host's display mode (`SetDisplayMode` only records its values), so there is nothing to restore.*
- launcher의 0x105 처리와 자식 실행은 작업 431·434의 것을 그대로 쓴다.
  *The launcher's handling of 0x105 and the child start are task 431's and 434's, unchanged.*

## 검증 / Verification

- 단위 테스트(`ddraw_module_test.cpp`): 두 interface의 `RestoreDisplayMode`가 `DD_OK`.
  *Unit tests (`ddraw_module_test.cpp`): `RestoreDisplayMode` gives `DD_OK` on both interfaces.*
- Linux x64 build와 CTest(경고를 오류로).
  *The Linux x64 build and CTest with warnings as errors.*
- 사용자 확인: 6th → Remember 1st → 게임 한 판 → 6th 타이틀. 키 입력을 자동화하지 못해 사용자에게 맡긴다. 정리 경로의 이후 import에서 또 멈추면 같은 방식으로 이어서 다룬다.
  *User check: 6th → Remember 1st → one game → 6th's title, left to the user since key input cannot be automated here; any later import on the clean-up path that stops is handled the same way.*
