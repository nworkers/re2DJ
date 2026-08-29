# Win32 창 모드와 메시지 pump 작업 로그

관련 설계: [Win32 창 모드와 메시지 pump](../design/20260828-084-window-mode-message-pump.md)

관련 작업 지시: [Win32 창 모드와 메시지 pump 작업 지시](../work-orders/20260828-084-window-mode-message-pump.md)

## 한국어

### 결과

원본 HWND의 생명주기는 유지하면서 `IDirectDraw4::SetCooperativeLevel` 경계에서 host 표시 정책을 적용했다. 기본값은 `re2DJ` 제목, 제목 표시줄·시스템 메뉴·최소화 버튼, resize/maximize가 없는 정확한 640×480 client-area 창이다. 현재 monitor work area 중앙에 배치한다. 제품 `--fullscreen`은 `OriginalProcessOptions`, x86 launcher와 injected runtime export를 거쳐 같은 HWND를 monitor bounds의 `WS_POPUP`으로 전환한다. 원본 EXE·INI와 host display mode는 수정하지 않는다.

공용 SDL3/OpenGL backend는 매 Present 전에 `SDL_PollEvent`를 queue가 빌 때까지 호출한다. 이 경계가 external HWND의 Win32 message queue를 rendering thread에서 처리하므로 창 클릭·이동 뒤 message starvation으로 무응답 판정되는 원인을 제거한다. 이 작업에서는 창 종료 정책을 재구현하지 않았고, 후속 사용자 관찰에 따른 close 전달은 [작업 090](20260829-090-window-close-process-exit.md)에서 구현했다.

### 검증

- `windows-x86-debug` 전체 build 통과. 새 코드도 warnings-as-errors를 통과했다.
- CTest 3/3 통과: runtime probe, product loader probe, unit tests.
- runtime probe는 전용 `CS_OWNDC` HWND에서 `re2DJ` 제목, windowed style, 640×480 client 크기, fullscreen popup 전환과 windowed 복귀를 확인했다.
- 실제 기본 실행 로그 `20260828-164044-334`는 `fullscreen:false` 주입 뒤 Flip/DrawPrimitive 루프가 계속 진행됨을 확인했다.
- 실제 fullscreen 실행은 launcher 진단에 `fullscreen:true`를 기록하고 DrawPrimitive sequence 786 이상까지 진행했다.
- 도구 실행 데스크톱에서는 실제 원본 HWND를 외부 PowerShell process가 열거하지 못해 자동 click/SendMessageTimeout 측정은 수행하지 못했다. 실제 renderer의 반복 Flip이 Present event pump를 실행했고, 사용자 반복 검증 절차는 [Windows x86 원본 실행 가이드](../guides/windows-x86-runtime.md)에 남겼다.

### 회고

창 정책을 Direct3D facade 거대 파일에 누적하지 않고 전용 Windows module로 분리해 원본 HWND adapter와 renderer 책임을 구분했다. fullscreen은 guest 설정 파일을 해석하거나 덮어쓰지 않고 host 실행 옵션으로만 전달하므로 다른 플랫폼에서도 같은 공용 설정 경계를 확장할 수 있다. 참고 영상은 현재 in-app browser session을 사용할 수 없어 정확한 typography나 icon은 확정하지 않았고, 간결한 프로젝트명 `re2DJ`만 반영했다.

## English

### Result

Host presentation policy now applies at `IDirectDraw4::SetCooperativeLevel` while preserving the original HWND lifetime. The default is an exact 640x480 client-area window titled `re2DJ`, with a title bar, system menu, and minimize button but no resize or maximize affordance, centered in the current monitor work area. Product `--fullscreen` travels through `OriginalProcessOptions`, the x86 launcher, and an injected-runtime export to switch the same HWND to a monitor-bounds `WS_POPUP`. It changes neither the original EXE/INI nor the host display mode.

Before every Present, the shared SDL3/OpenGL backend drains `SDL_PollEvent`. This processes the external HWND's Win32 message queue on the rendering thread and removes the message-starvation cause of Windows reporting the window as unresponsive after click or movement. This task did not reimplement gameplay input or window-close lifetime policy; later user observation led to the separate [Task 090](20260829-090-window-close-process-exit.md) close forwarding.

### Verification

- The complete `windows-x86-debug` build passed with warnings as errors.
- All three CTest cases passed: runtime probe, product loader probe, and unit tests.
- A dedicated `CS_OWNDC` HWND in the runtime probe verified the `re2DJ` title, windowed style, 640x480 client size, fullscreen popup transition, and restoration to windowed mode.
- Real default run log `20260828-164044-334` recorded `fullscreen:false` and continued through the Flip/DrawPrimitive loop.
- A real fullscreen run recorded `fullscreen:true` in launcher diagnostics and advanced beyond DrawPrimitive sequence 786.
- The tool desktop did not allow the external PowerShell process to enumerate the original HWND, so an automated click/SendMessageTimeout measurement was unavailable. Repeated renderer Flip calls exercised the Present event pump, and the repeatable user check remains in the [Windows x86 original runtime guide](../guides/windows-x86-runtime.md).

### Retrospective

Window policy is isolated in a dedicated Windows module instead of accumulating inside the large Direct3D facade, keeping the original-HWND adapter separate from renderer responsibilities. Fullscreen is transported only as a host execution option, leaving guest configuration untouched and retaining a configuration boundary that other platforms can extend. The in-app browser session was unavailable for exact typography or icon inspection of the reference video, so this task adopts only the concise project title `re2DJ`.
