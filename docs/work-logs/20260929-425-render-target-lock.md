# 작업 425 작업 로그 — 렌더 타깃 Lock의 되읽기와 올리기 / Task 425 work log — reading back and writing the render target on Lock

설계: [20260929-425-render-target-lock.md](../design/20260929-425-render-target-lock.md) · 지시서: [20260929-425-render-target-lock.md](../work-orders/20260929-425-render-target-lock.md)

## 2026-09-29

- F1은 `ez2dj_keyboard_map`에서 IO 보드 TEST 버튼(`test`)이다. SERVICE 버튼은 F2다.
  *F1 is the IO board's TEST button (`test`) in `ez2dj_keyboard_map`; SERVICE is F2.*
- WSLg 창에 XTest(`xkey`)로 보낸 F1은 다섯 번 중 한 번만 들어갔다. Windows 쪽에서 WSLg 창을 앞으로 가져와 `keybd_event`로 보내면 매번 들어갔다(`winkey.ps1`, scratchpad).
  *F1 sent to the WSLg window through XTest (`xkey`) got in only once in five tries. Bringing the WSLg window to the front on the Windows side and sending it with `keybd_event` got in every time (`winkey.ps1`, scratchpad).*
- 테스트 결과, 실패 0:
  - Windows x86: CTest 6개 통과, 단위 5347 checks.
  - Linux x64·x86: CTest 4개 통과, 단위 5344 checks.

  *Test results, no failures:*
  - *Windows x86: all 6 CTest tests pass, 5347 unit checks.*
  - *Linux x64 and x86: all 4 CTest tests pass, 5344 unit checks.*
- F1 뒤 화면(캡처는 scratchpad에만 둔다):
  - Linux x64: 테스트 모드의 "게임 옵션" 메뉴. 한글 글자, 흐린 항목, 강조색이 바르게 보인다.
  - Windows x86: 테스트 모드 메뉴("Language", "화면 테스트" 등). 작업 424의 검은 화면이 사라졌다.

  두 캡처의 메뉴 위치가 다른 것은 키 입력 시점 차이다.

  *The screen after F1 (captures kept in the scratchpad only):*
  - *Linux x64: test mode's "게임 옵션" menu, with the Korean text, dimmed items, and highlight colour right.*
  - *Windows x86: the test-mode menu ("Language", "화면 테스트", and so on); Task 424's black screen is gone.*

  *The two captures show different menu positions because of key timing.*
