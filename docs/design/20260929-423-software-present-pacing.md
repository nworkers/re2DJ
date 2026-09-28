# 작업 423 설계 — vsync가 막지 않는 host의 소프트웨어 페이싱 / Task 423 design — software present pacing where vsync does not block

선행: [작업 422 설계](20260928-422-dx6-textures.md)

## 배경 / Background

WSLg의 Linux 실행에서 1st SE의 창 제목 FPS가 약 112였다. 같은 게임이 Windows에서는 60이다. 공용 SDL3/OpenGL backend는 두 host에서 똑같이 swap interval 1을 요청하고 받는다. 그런데 WSLg의 가상 GPU에서는 swap이 vblank를 기다리지 않고 바로 돌아온다.

1st SE는 매 프레임 `Flip`과 `GetAsyncKeyState`를 부르고, `timeGetTime`은 드물게 부른다(API 기록 첫 32,768호출 중 `Flip` 911, `GetAsyncKeyState` 912, `timeGetTime` 311). 그래서 프레임 수로 속도를 맞추는 것으로 보이며, 이런 host에서는 약 1.9배 빨리 돈다. 사용자는 소프트웨어 페이싱이 필요하다고 정했다.

*On a WSLg Linux run, 1st SE's window title read about 112 FPS, against 60 on Windows. The shared SDL3/OpenGL backend requests and is granted swap interval 1 on both hosts, but on WSLg's virtual GPU the swap returns at once instead of waiting for the vblank.*

*1st SE calls `Flip` and `GetAsyncKeyState` every frame and `timeGetTime` only rarely (in the first 32,768 recorded calls: 911 `Flip`, 912 `GetAsyncKeyState`, 311 `timeGetTime`). It therefore appears to keep time by counting frames, and on such a host runs about 1.9 times too fast. The user decided that software pacing is needed.*

## 결정 / Decisions

1. **판단 로직은 순수 클래스 `graphics::PresentPacer`에 둔다**(`present_pacer.h`). 공용 backend가 swap 직후 이를 불러, 돌려받은 시간만큼 `SDL_DelayPrecise`로 기다린다. 두 host가 같은 코드를 쓴다.
   *The logic lives in the pure class `graphics::PresentPacer` (`present_pacer.h`). The shared backend calls it right after each swap and waits the returned time with `SDL_DelayPrecise`; both hosts share the code.*
2. **켜는 조건.**
   - present 간격 60개(`kProbeFrames`)를 한 창으로 본다. 그 중앙값이 표시 주기의 3/4보다 짧으면 swap이 막지 않는다고 보고 pacing을 켠다. 한 번 켜면 실행이 끝날 때까지 유지한다.
   - 실제로 막는 swap에서는 간격이 주기에 머물러 켜지지 않는다. 주사율보다 느린 게스트도 켜지 않는다.
   - `PresentSync::kImmediate` 정책(막지 말라는 요청)에서는 켜지 않는다.

   *When it engages.*
   - *Present intervals are watched in windows of 60 (`kProbeFrames`). When a window's median is below three quarters of the display period, the swap is taken not to block and pacing engages, staying on for the rest of the run.*
   - *With a swap that does block, the intervals stay at the period and it never engages; a guest slower than the refresh rate never engages it either.*
   - *The `PresentSync::kImmediate` policy (asking presents not to block) never engages it.*
3. **켜진 뒤의 동작.** 각 present는 앞 present보다 한 주기 뒤에야 게스트로 돌아간다(마감 시각 방식이라 오차가 쌓이지 않는다). 늦은 present는 기다리지 않고, 그 시점부터 다시 센다. 그래서 느린 프레임 뒤에 몰아서 따라잡지 않는다.
   *Once engaged. Each present returns to the guest no earlier than one period after the previous one; a deadline schedule keeps error from accumulating. A late present waits for nothing and the schedule restarts from it, so a slow frame is never followed by a burst of catching up.*
4. **주기.** 주기는 창이 있는 디스플레이의 현재 주사율이다. vsync가 줄 값과 같다. 모르면 60 Hz로 한다.
   *The period. It is the refresh rate of the display holding the window, which is what vertical sync would give; 60 Hz when unknown.*
5. **관찰.** pacing이 켜지면 한 번 기록한다.
   - Linux: 제품 로그에 `presentation: the swap does not block, so presents are paced at the display rate`.
   - Windows DX6 facade: graphics trace에 `re2dj:hle:present-sync:software-pacing=1`.

   *Observation. Engagement is recorded once:*
   - *Linux: `presentation: the swap does not block, so presents are paced at the display rate` in the product log.*
   - *Windows DX6 facade: `re2dj:hle:present-sync:software-pacing=1` in the graphics trace.*

## 범위 밖 / Out of scope

- 원본 아케이드의 60 Hz를 host 주사율과 따로 강제하는 일. 지금 Windows도 host vsync를 따른다.
  *Forcing the original arcade's 60 Hz apart from the host refresh rate; Windows follows the host vsync today as well.*
