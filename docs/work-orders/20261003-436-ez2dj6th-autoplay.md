# 작업 436 작업 지시서 — 6th와 Remember 1st의 autoplay / Task 436 work order — autoplay for 6th and Remember 1st

설계: [20261003-436-ez2dj6th-autoplay.md](../design/20261003-436-ez2dj6th-autoplay.md)

## 절차 / Steps

1. `game-state-hunt` 절차로 `EZ2DJ6th.EXE`와 `EZ2DJ1ST/Ez2DJ.exe`를 분석한다(정적 분석, Linux 읽기 전용 폴링). `guest_memory.py`에 Linux 지원을 더한다.
   *Analyse `EZ2DJ6th.EXE` and `EZ2DJ1ST/Ez2DJ.exe` with the `game-state-hunt` procedure (static analysis, read-only polling on Linux), adding Linux support to `guest_memory.py`.*
2. 공용 `target`: `game_controls`를 빌드별 목록으로 바꾸고 6th를 선언한다. 단위 테스트를 고친다.
   *Shared `target`: make `game_controls` a per-build list and declare 6th; update the unit tests.*
3. Linux: `platform/linux/game_controls`, `OriginalRunEnvironment::autoplay_flag_rva`, 실행기의 무장, OSD 토글, CLI의 선택과 로그.
   *Linux: `platform/linux/game_controls`, `OriginalRunEnvironment::autoplay_flag_rva`, arming in the runner, the OSD toggle, and the CLI's pick and log line.*
4. Windows: 주 debuggee의 목록 선택, bootstrap 자식의 OSD 정보와 autoplay 주소.
   *Windows: the main debuggee's pick from the list, and the OSD information and autoplay address for bootstrap children.*
5. 문서: 분석 문서와 색인, `EXE_DESIGN`, 스킬, `ARCHITECTURE.md`, `IMPLEMENTED.md`, 작업 로그.
   *Documents: the analysis and its index, `EXE_DESIGN`, the skill, `ARCHITECTURE.md`, `IMPLEMENTED.md`, and the work log.*

## 완료 조건 / Done when

- Linux x64 build와 CTest가 통과한다.
  *The Linux x64 build and CTest pass.*
- Linux 실행에서 6th 자식만 autoplay가 무장되고, 사용자가 OSD로 autoplay를 확인한다.
  *In a Linux run only the 6th child arms autoplay, and the user confirms autoplay through the OSD.*
- Windows build와 실행을 하지 못한 이유를 작업 로그에 남긴다.
  *The work log records why the Windows build and run could not be done here.*
