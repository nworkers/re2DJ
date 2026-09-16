# 작업 297 작업 지시 — Dear ImGui OSD와 autoplay 토글 / Task 297 work order — Dear ImGui OSD with an autoplay toggle

설계: [20260917-297-imgui-osd-autoplay.md](../design/20260917-297-imgui-osd-autoplay.md)
선행: [작업 296](20260917-296-autoplay-write-test.md)
상태: **완료.** [작업 로그](../work-logs/20260917-297-imgui-osd-autoplay.md)

## 한국어

### 구현 범위

1. **Dear ImGui 도입.** `v1.92.9`를 고정 commit으로 FetchContent 하고 코어와 `imgui_impl_opengl3`만 `re2dj_imgui` 정적 라이브러리로 묶는다. `LICENSE.txt`를 확인하고 `THIRD_PARTY_NOTICES.md`에 추가한다.
2. **backend overlay 지점.** `graphics::PresentOverlay` 인터페이스를 두고 `Sdl3OpenGlBackend::Present`가 합성 직후·swap 직전에 부른다. overlay가 없으면 동작이 바뀌지 않는다.
3. **공용 OSD.** `ui::Osd` — ImGui 컨텍스트, 표시 상태, lock으로 보호한 입력 큐, 토글 항목 목록. 꺼져 있으면 ImGui 프레임을 만들지 않는다. 플랫폼 API를 부르지 않는다.
4. **Windows 입력.** `GuestWindowProcedure`가 백틱(`VK_OEM_3`, 반복 제외)으로 OSD를 토글하고, OSD가 켜져 있을 때 마우스를 큐에 넣는다.
5. **게임 제어.** 프로파일 `game_controls`(`autoplay_flag_rva`, `build_timestamp`), 공용 판정 함수, 런처의 timestamp 검사와 `g_re2dj_autoplay_flag_address` 쓰기, 주입 런타임의 토글 항목 등록. 3rd 값은 `0x00629508` / `0x3bca98a3`.

### 검증

* Windows x86 Debug·Release 빌드, 단위 테스트.
* 단위 테스트: 3rd 프로파일 값, timestamp 일치·불일치 판정.
* 실행(사용자 조작): 시작 시 OSD 숨김, 백틱 토글, 곡 선택에서 체크 후 autoplay, 데모 중 체크박스 추종.
* OSD 꺼진 상태의 `present-interval` 비용이 이전과 같은지.

### 완료 조건

* 위 검증 결과와 작업 로그.
* `ARCHITECTURE.md`에 OSD 계층 반영.
* 분석 문서의 7절(강제 toggle)에 제품 경로를 연결.
* 사용 절차를 `docs/guides/windows-x86-runtime.md`에 추가.

### 범위에서 뺀 것

* 3rd 외 타깃, Linux·Web 입력 공급, 키보드로 항목 조작, 슬롯 `0x1b` 물리 바인딩.

## English

Design: [20260917-297-imgui-osd-autoplay.md](../design/20260917-297-imgui-osd-autoplay.md)
Prerequisite: [Task 296](20260917-296-autoplay-write-test.md)
Status: **complete.** [Work log](../work-logs/20260917-297-imgui-osd-autoplay.md)

### Scope

1. **Bring in Dear ImGui.** FetchContent `v1.92.9` pinned to its commit and build only the core and `imgui_impl_opengl3` into a `re2dj_imgui` static library. Check `LICENSE.txt` and add it to `THIRD_PARTY_NOTICES.md`.
2. **A backend overlay point.** A `graphics::PresentOverlay` interface that `Sdl3OpenGlBackend::Present` calls right after compositing and right before the swap; without an overlay, behavior is unchanged.
3. **The shared OSD.** `ui::Osd` holds the ImGui context, the visibility state, a lock-guarded input queue and the list of toggle items. It builds no ImGui frame while hidden and calls no platform API.
4. **Windows input.** `GuestWindowProcedure` toggles the OSD on backtick (`VK_OEM_3`, ignoring repeats) and queues mouse input while it is visible.
5. **Game control.** The profile's `game_controls` (`autoplay_flag_rva`, `build_timestamp`), a shared decision function, the launcher's timestamp check and write of `g_re2dj_autoplay_flag_address`, and the injected runtime registering the toggle item. The 3rd values are `0x00629508` / `0x3bca98a3`.

### Verification

* Windows x86 Debug and Release builds and unit tests.
* Unit tests for the 3rd profile values and for the timestamp match and mismatch decisions.
* A run driven by the user: hidden at start, backtick toggles it, ticking at song select gives autoplay, the checkbox follows during a demo.
* With the OSD hidden, `present-interval` cost matches the earlier baseline.

### Completion criteria

* The verification results and a work log.
* The OSD layer reflected in `ARCHITECTURE.md`.
* The product path linked from section 7 (forced toggle) of the analysis document.
* The usage procedure added to `docs/guides/windows-x86-runtime.md`.

### Out of scope

* Targets other than 3rd, Linux and Web input feeding, operating items from the keyboard, and slot `0x1b`'s physical binding.
