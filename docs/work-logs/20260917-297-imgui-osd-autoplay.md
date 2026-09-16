# 작업 297 작업 로그 — Dear ImGui OSD와 autoplay 토글 / Task 297 work log — Dear ImGui OSD with an autoplay toggle

설계: [20260917-297-imgui-osd-autoplay.md](../design/20260917-297-imgui-osd-autoplay.md)
작업 지시: [20260917-297-imgui-osd-autoplay.md](../work-orders/20260917-297-imgui-osd-autoplay.md)
선행: [작업 296](20260917-296-autoplay-write-test.md)

## 한국어

### 구현

| 계층 | 변경 |
| --- | --- |
| 서드파티 | Dear ImGui `v1.92.9`(`01380c57`)를 FetchContent, 코어와 `imgui_impl_opengl3`만 `re2dj_imgui`로. MIT 라이선스를 받은 소스에서 확인하고 `THIRD_PARTY_NOTICES.md`에 추가 |
| 그래픽 | `graphics::PresentOverlay` 인터페이스, `Sdl3OpenGlBackend::SetPresentOverlay`, Present의 합성 뒤·swap 전 호출 |
| 공용 UI | `ui::Osd` — ImGui 컨텍스트, 표시 상태, 입력 큐, 정보 줄, 토글. 숨김 상태에서는 프레임을 만들지 않음 |
| 타깃 프로파일 | `GameControls`(`autoplay_flag_rva`, `build_timestamp`), `ArmedAutoplayFlagRva`, 3rd 값 `0x00629508`/`0x3bca98a3` |
| 런처 | 대상 id·실행 파일 이름과, timestamp가 맞을 때만 autoplay 주소를 런타임 전역에 기록. `osd_controls` 진단 한 줄 |
| Windows | `osd_host.*`(프로세스 OSD, backend 설치, 창 메시지), `game_controls.*`(정보 줄과 토글), 호스트·게스트 창 프로시저 연결, 커서 표시 |

### 사용자 확인으로 바뀐 것

설계 문서의 "구현 중 바뀐 것" 절에 근거와 함께 남겼다.

1. **키가 도착하지 않았다.** 첫 빌드는 백틱을 게스트 창 프로시저에서만 받았다. 진단을 넣어 보니 백틱뿐 아니라 `Z`, `F5`도 한 건도 기록되지 않았다. 게스트 창은 호스트 창의 `WS_CHILD`이고 키보드 포커스는 호스트에 있으므로, 호스트 창 프로시저에서 받도록 고쳤고 동작을 확인했다. 원인 추적용 키 기록은 사용자의 키 입력을 로그에 남기므로 해결 뒤 제거했다.
2. **OSD 내용을 정보 세 줄로 바꿨다.** `re2DJ v{버전} {빌드 날짜}` / `Target Profile : {id}` / `Executable : {파일 이름}`. "다음 곡부터 적용" 문구는 삭제했다.
3. **폭을 창 전체로, 글자를 0.75배로** 줄였다.
4. **마우스 커서가 창 위에서 사라졌다.** 3rd가 `ShowCursor`·`SetCursor`를 import 하는 것을 덤프에서 확인했다. 클라이언트 영역의 `WM_SETCURSOR`에서 화살표를 설정하고 표시 카운트를 복구한다.

### 검증

* Windows x86 Debug·Release 빌드: 오류·경고 0건.
* 단위 테스트: `checks: 1804, failures: 0`. 추가된 21개는 3rd `game_controls` 값, 같은 제품의 다른 timestamp에서 무장하지 않음, 다른 프로파일에 선언이 없음, 선언이 반쪽이면 무장하지 않음을 확인한다.
* CTest: `re2dj_windows_vfs_runtime_probe`를 제외한 5개 통과(그 probe는 기존 hang).
* 실행: `osd_controls` 진단에서 `build_timestamp` = `executable_timestamp` = `0x3bca98a3`, `autoplay_armed: true`.
* 사용자 확인: 시작 시 OSD 숨김, 백틱 토글, 정보 세 줄, 가로 전체 폭, 커서 표시, autoplay 토글 동작.

### present 비용 관찰 — 이번 변경과 무관함을 확인

작업 지시의 "OSD를 끈 상태의 present 비용이 이전과 같은지"를 확인하다가 이전과 다른 값이 나왔다. 오늘 01:25 실행까지는 present 비용 평균 0.17~0.23 ms, 간격 17.6 ms였는데, 01:49 실행부터 **present가 약 15 ms 블록되고 간격이 16.67 ms(60 fps)** 가 됐다. 01:49는 OSD 코드를 처음 넣은 빌드와 시점이 겹친다.

그래서 같은 환경에서 A/B를 했다. 커밋 전 OSD 변경을 stash하고 이전 코드를 빌드해 실행하자 **이전 코드도 present 비용 14.5~15.5 ms, 간격 16.67 ms** 였다. 따라서 이 변화는 OSD 코드가 아니라 01:25~01:49 사이에 바뀐 실행 환경(디스플레이·드라이버의 present 방식 등)에서 온 것이다. 원인은 이번 범위 밖이므로 **미확정**으로 남긴다. 작업 289~291의 프레임 측정을 인용할 때는 측정 시점의 환경이 같은지 먼저 확인해야 한다.

### 남은 것

* 3rd 외 타깃의 게임 제어.
* Linux·Web의 OSD 입력 공급.
* 위 present 비용 변화의 원인.

## English

Design: [20260917-297-imgui-osd-autoplay.md](../design/20260917-297-imgui-osd-autoplay.md)
Work order: [20260917-297-imgui-osd-autoplay.md](../work-orders/20260917-297-imgui-osd-autoplay.md)
Prerequisite: [Task 296](20260917-296-autoplay-write-test.md)

### Implementation

| Layer | Change |
| --- | --- |
| Third party | Dear ImGui `v1.92.9` (`01380c57`) through FetchContent, only the core and `imgui_impl_opengl3` in `re2dj_imgui`; MIT license confirmed in the fetched source and added to `THIRD_PARTY_NOTICES.md` |
| Graphics | The `graphics::PresentOverlay` interface, `Sdl3OpenGlBackend::SetPresentOverlay`, and the call after compositing and before the swap in Present |
| Shared UI | `ui::Osd` — ImGui context, visibility, input queue, information lines and toggles; no frame is built while hidden |
| Target profile | `GameControls` (`autoplay_flag_rva`, `build_timestamp`), `ArmedAutoplayFlagRva`, and the 3rd values `0x00629508` / `0x3bca98a3` |
| Launcher | Writes the target id, executable name and — only on a timestamp match — the autoplay address into runtime globals, with one `osd_controls` diagnostic line |
| Windows | `osd_host.*` (process OSD, backend install, window messages), `game_controls.*` (information lines and toggle), host and guest window procedure hooks, cursor visibility |

### Changed through the user's checks

Recorded with rationale in the design's "Changed during implementation" section.

1. **No key arrived.** The first build took backtick only in the guest window procedure. A diagnostic showed not a single key recorded — not backtick, `Z` or `F5`. The guest window is a `WS_CHILD` of the host window, where keyboard focus stays, so keys are now taken in the host window procedure, confirmed working. The key recording used to find this logged the user's keystrokes and was removed once solved.
2. **The OSD shows three information lines**: `re2DJ v{version} {build date}` / `Target Profile : {id}` / `Executable : {file name}`; the "applies from the next song" note was removed.
3. **Full window width, and a 0.75 font scale.**
4. **The mouse cursor vanished over the window.** The dump shows 3rd importing `ShowCursor` and `SetCursor`; `WM_SETCURSOR` over the client area now sets the arrow and restores the display count.

### Verification

* Windows x86 Debug and Release builds: no errors or warnings.
* Unit tests: `checks: 1804, failures: 0`. The 21 new checks cover the 3rd `game_controls` values, not arming on another timestamp of the same product, no declaration on other profiles, and not arming a half declaration.
* CTest: 5 pass excluding `re2dj_windows_vfs_runtime_probe`, the existing hang.
* Run: the `osd_controls` diagnostic shows `build_timestamp` = `executable_timestamp` = `0x3bca98a3` and `autoplay_armed: true`.
* User checks: hidden at start, backtick toggle, the three information lines, full width, visible cursor, and a working autoplay toggle.

### Present cost — confirmed unrelated to this change

Checking the work order's "present cost with the OSD hidden matches the earlier baseline" produced a different value. Through today's 01:25 run, present cost averaged 0.17-0.23 ms at a 17.6 ms interval; from the 01:49 run on, **present blocked about 15 ms and the interval became 16.67 ms (60 fps)**. 01:49 coincides with the first build carrying the OSD code.

So an A/B was run in the same environment: with the uncommitted OSD changes stashed, the previous code built and run showed **the same 14.5-15.5 ms present cost at 16.67 ms**. The change therefore comes not from the OSD code but from something in the execution environment that changed between 01:25 and 01:49 — the display or driver present mode, for example. Its cause is outside this task and left **unresolved**. Frame measurements from tasks 289-291 should be cited only after confirming the environment matches.

### Remaining

* Game controls for targets other than 3rd.
* OSD input feeding on Linux and Web.
* The cause of the present-cost change above.
