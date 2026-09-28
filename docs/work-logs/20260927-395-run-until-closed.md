# 작업 395 작업 로그 — 창을 닫을 때까지 실행 / Task 395 work log — running until the window is closed

설계: [20260927-395-run-until-closed.md](../design/20260927-395-run-until-closed.md)
작업 지시서: [20260927-395-run-until-closed.md](../work-orders/20260927-395-run-until-closed.md)

## 진행 / Progress

사용자가 직접 실행했을 때 게임이 로고 장면에서 끝났다. 원인은 continuation의 호출 한도(32,768번)였다.

한도를 없앤 x64 실행을 40초 두었더니 창에 EZ2DJ 4th OVER MIND 타이틀 화면(INSERT COIN(S), 크레딧 0/3)이 나왔다. 캡처는 원본 화면이라 저장소 밖 scratchpad에만 두었다.

그 뒤 Windows 쪽에서 WSLg 창에 `WM_CLOSE`를 보냈다. 제목 표시줄의 닫기 단추와 같은 효과다. 실행은 다음 호출에서 멈췄다.

- 호출 809,357번 뒤 `continuation : host window closed, after ddraw.dll!IDirectDrawSurface7::Flip`
- exit 0
- re2dj 프로세스가 남지 않았다.

API 기록은 7.7MB에서 멈췄고, 끝에 "calls after #32768 are not recorded here" 한 줄이 남았다.

*A user's own run ended at the logo scene because of the continuation's call limit (32,768 calls). With no limit, an x64 run left for 40 seconds showed the EZ2DJ 4th OVER MIND title screen (INSERT COIN(S), credits 0/3) in its window; the capture, of the original screen, is kept only outside the repository in the scratchpad. `WM_CLOSE` was then posted to the WSLg window from the Windows side, as the title bar's close button does. The run stopped at the next call:*

- *`continuation : host window closed, after ddraw.dll!IDirectDrawSurface7::Flip` after 809,357 calls*
- *exit 0*
- *no re2dj process left*

*The API log stopped at 7.7 MB with one line: "calls after #32768 are not recorded here".*

회귀 스크립트(`b367.sh`)는 `--call-limit 32768`을 넘기도록 바꿨다. 결과는 작업 394와 같은 경계다. 두 폭 모두 한도에서 멈추고, 시계와 무관한 수치는 같다.

*The regression script (`b367.sh`) now passes `--call-limit 32768` and gives the same boundary as Task 394: both widths stop at the limit, with the clock-independent figures equal.*

## 변경 / Changes

- **runner**:
  - `OriginalRunEnvironment::call_limit`(기본 0).
  - 경계 `kContinuationHostClosed`.
  - `kOriginalApiLogFullCalls`(32,768). `kOriginalContinuationCallLimit`는 없앴다.

  ***runner:***
  - *`OriginalRunEnvironment::call_limit` (default 0).*
  - *The boundary `kContinuationHostClosed`.*
  - *`kOriginalApiLogFullCalls` (32,768), with `kOriginalContinuationCallLimit` removed.*
- **continuation**: 호출마다 창 닫기 요청을 확인한다. 한도는 설정했을 때만 쓴다. API 기록은 처음 32,768번까지다. / ***continuation:** a close request is checked after every call, the limit applies only when set, and the API log covers the first 32,768 calls.*
- **backend**: `Sdl3OpenGlBackend::SetEventObserver`. / ***Backend:** `Sdl3OpenGlBackend::SetEventObserver`.*
- **host 표시 / host presentation**:
  - `HostPresentation::CloseRequested()`.
  - Linux는 SDL 종료·창 닫기 이벤트로 닫기 요청을 기억한다.
  - `HoldUntilClosed`는 이미 닫힌 창을 기다리지 않는다.

  ***Host presentation:***
  - *`HostPresentation::CloseRequested()`.*
  - *Linux notes a close request from SDL's quit and window-close events.*
  - *`HoldUntilClosed` does not wait on a window already closed.*
- **CLI**: `--call-limit <n>`, 새 경계의 출력. / ***CLI:** `--call-limit <n>` and the new boundary's report.*

## 검증 / Validation

| 항목 / Item | 결과 / Result |
| --- | --- |
| Windows x86 build, CTest | exit 0, 6/6 |
| Windows 실제 4th, 기준 `a5c1fba`와 30초씩 / real 4th vs base `a5c1fba`, 30 s each | 정규화한 ddraw 기록 1,410줄이 같다 / *the normalized ddraw logs match, 1,410 lines* |
| Linux x64·x86 build, CTest | 경고·오류 없음, 각각 3/3 / *no warnings or errors, 3/3 each* |
| Linux `--call-limit 32768`, 두 폭 / both widths | 호출 32,768번에서 멈춤, hardlock 83 / *stops at 32,768 calls, hardlock 83* |
| Linux x64, 한도 없음 / no limit | 40초 뒤 타이틀 화면, 창 닫기로 exit 0 / *title screen after 40 s; exit 0 on closing the window* |

## 다음 / Next

창 크기·배율 단축키·전체 화면을 Windows 제품과 같게 맞춘다. 사용자가 요청한 작업이다.

*Next, match the window size, scale shortcuts, and fullscreen to the Windows product, as the user asked.*
