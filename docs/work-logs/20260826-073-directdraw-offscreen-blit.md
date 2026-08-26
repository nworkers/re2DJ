# DirectDraw 오프스크린 합성 복구 작업 로그

관련 설계: [DirectDraw 오프스크린 합성 복구](../design/20260826-073-directdraw-offscreen-blit.md)  
관련 작업 지시: [DirectDraw 오프스크린 합성 복구](../work-orders/20260826-073-directdraw-offscreen-blit.md)

## 사용자 크래시 분석

- 사용자 실행 로그 `20260826-195753-339.jsonl`은 정상 detach 뒤 종료 코드 1을 기록했다.
- Windows Application Error와 WER는 `ez2dj.exe+0x88d6`, 예외 `0xc0000005`를 기록했고 `%LOCALAPPDATA%\CrashDumps`에 dump를 남겼다. dump 파일 자체는 저장소에 포함하지 않았다.
- dump의 예외 context는 `ECX=0`, read address `0x00000008`, `EAX=0x016403a4`, `EBP=0x0019faec`, return `0x004085e5`였다. 게임 객체 `+0x2c8`은 0이었다.
- 정적 역추적에서 생성자 `0x0040a9c4`가 `1p_meter_back`을 조회해 `+0x2c8`을 채우며, 조회 함수 `0x0041ff10`은 `%s.bmp`와 `DDSCAPS_OFFSCREENPLAIN(0x40)` surface를 사용한다. dump의 동적 surface count는 0이었다.
- 이어지는 원본 호출은 source-key `BltFast`와 `Blt`다. 기존 facade는 offscreen surface를 거절하고 `BltFast` vtable을 연결하지 않았다.

## 구현

- 공용 graphics core에 pitch·bounds를 검증하는 RGB565 사각형 복사와 inclusive source color-key 처리를 추가했다.
- DirectDraw facade가 명시적 pixel format이 없는 관찰된 offscreen descriptor를 현재 RGB565 mode로 생성한다.
- `BltFast`와 source-copy `Blt`를 연결하고 동일 크기 rectangle, `DDBLTFAST_SRCCOLORKEY`, `DDBLT_KEYSRC`와 wait flag를 처리한다.
- destination CPU backing과 revision을 갱신하고 primary/back destination은 같은 source rectangle을 기존 OpenGL frame에 합성한다.
- 관찰되지 않은 stretch, ROP과 destination-key 조합은 계속 명시적으로 거절한다.

## 검증

- Windows x86 warnings-as-errors 전체 빌드 통과.
- Windows x86 CTest 2/2 통과.
- Windows x64 warnings-as-errors 전체 빌드 통과.
- Windows x64 CTest 1/1 통과.
- 단위 테스트는 일반 rectangle copy, inclusive key skip, invalid bounds를 검증한다.
- detached 로그 `20260826-201731-528.jsonl`에서 process는 기존 약 76초 crash 지점을 넘어 120초 동안 응답 상태를 유지했다. PID와 실행 파일 경로를 확인한 뒤 검증 종료를 위해 해당 process만 강제 종료했으므로 기록된 `0xffffffff`는 테스트 종료 코드다.
- 같은 실행 시간대에 새 Application Error/WER crash는 없었다.

## 남은 확인

누락됐던 그림, source color-key 테두리와 전체 합성 순서가 실제 화면에서 정확한지는 사용자가 다시 확인해야 한다. 이 항목은 작업 072의 활성 사용자 검증으로 유지한다.

## 다음 세션 시작점

브랜치 `render-correctness-performance`에서 작업 072의 사용자 재검증을 계속한다. `23f4598`까지 구현과 자동 검증은 완료됐으며 아직 `main`에 머지하지 않았다. 실행 command와 진단 절차, merge 조건은 [작업 072 인계](../work-orders/20260826-072-render-correctness-performance.md#다음-세션-인계)에 정리했다.

---

# DirectDraw Offscreen Composition Recovery Work Log

The user's detached log ended with code 1. Windows Error Reporting identified `0xc0000005` at `ez2dj.exe+0x88d6`; the dump confirmed `ECX=0`, a read from `0x00000008`, and null game-object member `+0x2c8`. Static flow maps that member to lazy `1p_meter_back` lookup through `%s.bmp`, an RGB565 `DDSCAPS_OFFSCREENPLAIN` surface, GDI copy, and source-key `BltFast`/`Blt`. The dump's dynamic-surface count was zero, matching the facade's unsupported offscreen request and unset BltFast slot.

The implementation adds a tested platform-neutral RGB565 rectangle/key copy, GDI-backed offscreen surfaces, source-copy `Blt` and `BltFast`, destination revision tracking, and OpenGL composition for visible destinations. x86 and x64 warnings-as-errors builds pass; x86 CTest passes 2/2 and x64 CTest passes 1/1. Detached log `20260826-201731-528.jsonl` remains responsive for 120 seconds beyond the former roughly 76-second crash point. The exact verified process was then forcibly stopped, explaining exit `0xffffffff`; no new Application Error or WER crash appeared. User-visible sprite, color-key, and composition-order accuracy remains active Task 072 validation.

Continue the next session on branch `render-correctness-performance` with Task 072 user revalidation. Implementation and automated verification through `23f4598` are complete and have not been merged into `main`. The Task 072 next-session handoff records the run command, crash-diagnostic path, remaining checks, and merge conditions.
