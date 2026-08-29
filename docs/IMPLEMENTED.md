# 구현 완료 항목 / Implemented

완료된 구현·검증 항목을 누적 기록합니다. 상세 근거는 각 작업 로그와 설계 문서를 참조합니다.

*This document records completed implementation and verification items. Detailed evidence remains in the corresponding design and work-log documents.*

## 최신 실행 이정표 / Latest runtime milestones

- **작업 092 — DWM 캡션과 DPI-aware frame 완료.** DWM을 끄는 우회를 제거하고 표준 top-level host shell 안에 원본 HWND를 1280×960 child로 유지했다. 실제 결손 원인은 SDL DPI 초기화 전 96-DPI frame 크기가 이후 144-DPI DWM caption을 clip한 것이었다. SDL 초기화 뒤 `GetDpiForWindow`와 `AdjustWindowRectExForDpi`로 재적용해 outer 1302×1016, host/guest client 1280×960, icon·전체 제목·FPS와 Windows 11 frame을 확인했다. host close는 guest teardown 없이 current-process 종료 경계를 사용해 original process와 loader가 정상 종료된다. Debug/Release CTest 3/3이 통과했다. 근거: [설계](design/20260829-092-dwm-caption-preservation.md), [분석](analysis/win32-caption-dpi.md), [작업 로그](work-logs/20260829-092-dwm-caption-preservation.md).

  *Task 092 — DWM caption and DPI-aware frame complete. The DWM-disable workaround is removed, and the original HWND remains a 1280x960 child inside a standard top-level host shell. The actual defect was a pre-SDL 96-DPI frame size clipping the later 144-DPI DWM caption. Reapplying with `GetDpiForWindow` and `AdjustWindowRectExForDpi` after SDL initialization confirms a 1302x1016 outer, 1280x960 host/guest clients, icon, full title/FPS, and the Windows 11 frame. Host close uses the current-process termination boundary without guest teardown, allowing both the original process and loader to exit normally. Debug/Release CTest passes 3/3. Evidence: [design](design/20260829-092-dwm-caption-preservation.md), [analysis](analysis/win32-caption-dpi.md), [work log](work-logs/20260829-092-dwm-caption-preservation.md).*

- **작업 091 — Win32 상태 제목과 기본 2배 확대 완료.** 창 제목은 저장소 version, runtime build date, 실제 `SDL3 OpenGL` renderer와 성공한 `Flip` 기준 FPS를 표시한다. 원본 640×480 논리 표시와 surface는 유지하면서 기본 client 영역만 1280×960으로 확대했다. 실제 제품 trace에서 제목·icon·style 저장과 overwrite 차단이 정상인데도 DWM이 caption content를 소거하는 것을 확인했다. windowed는 `DWMNCRP_DISABLED`로 USER32 기본 caption을 표시하고 fullscreen은 `DWMNCRP_ENABLED`로 복원한다. 실제 제품에서 단일 제목·icon, `FPS : 60.0`, 1280×960 client를 확인했으며 hostile-WndProc runtime probe와 Debug/Release CTest 3/3이 통과했다. 근거: [설계](design/20260829-091-window-title-default-scale.md), [작업 로그](work-logs/20260829-091-window-title-default-scale.md).

  *Task 091 — Win32 status title and default 2x scale complete. The title shows the repository version, runtime build date, actual `SDL3 OpenGL` renderer, and FPS measured from successful `Flip` calls. The original 640x480 logical display and surfaces remain unchanged while only the default client area grows to 1280x960. Product traces confirmed valid title/icon/style storage and overwrite blocking while DWM still removed the caption content. Windowed mode now uses `DWMNCRP_DISABLED` to expose the USER32 default caption, while fullscreen restores `DWMNCRP_ENABLED`. A real product run confirmed one title/icon, `FPS : 60.0`, and the 1280x960 client; the hostile-WndProc runtime probe and Debug/Release CTest pass 3/3. Evidence: [design](design/20260829-091-window-title-default-scale.md), [work log](work-logs/20260829-091-window-title-default-scale.md).*

- **작업 090 — Win32 host close process 종료 완료.** 실제 trace `20260829-112237-831`은 close와 watcher exit 뒤 `ExitProcess(0)` termination이 완료되지 않는 문제를 확정했다. 원본 WndProc 정리 뒤 current-process `TerminateProcess(..., 0)`을 사용하는 보정 실행 `20260829-112906-743`은 exit code 0과 성공 outcome을 기록했다. Debug/Release CTest 3/3이 통과했고 사용자가 창 닫기 시 process 종료를 확인했다. 근거: [설계](design/20260829-090-window-close-process-exit.md), [작업 로그](work-logs/20260829-090-window-close-process-exit.md).

  *Task 090 — Win32 host-close process termination complete. Actual trace `20260829-112237-831` established that `ExitProcess(0)` termination did not complete after close and watcher exit. Corrected run `20260829-112906-743`, using current-process `TerminateProcess(..., 0)` after original-WndProc cleanup, records exit code zero and a successful outcome. Debug/Release CTest passes 3/3, and the user confirmed process termination on window close. Evidence: [design](design/20260829-090-window-close-process-exit.md), [work log](work-logs/20260829-090-window-close-process-exit.md).*

- **작업 089 — 변환 전 Direct3D 3 정점 draw 지원 완료.** 작업 088 수정 뒤에도 중앙 그림이 없었고 새 실행 로그의 `TextureLoad` 호출은 0회였으므로 이전 직접 원인 추정을 기각했다. 실제 실패 경계였던 32바이트 `D3DVERTEX(0x112)`와 `D3DLVERTEX(0x1e2)`를 플랫폼 중립 world/view/projection 및 viewport 변환에 연결했다. Windows build와 CTest 3/3이 통과했고, 사용자가 Music Select 중앙 그림 복구를 확인했다. 근거: [설계](design/20260829-089-untransformed-direct3d-draw.md), [작업 로그](work-logs/20260829-089-untransformed-direct3d-draw.md).

  *Task 089 — untransformed Direct3D 3 vertex draws complete. The center remained absent after Task 088 and the new runtime log contained zero `TextureLoad` calls, rejecting that direct-cause hypothesis. The actual failing boundary—32-byte `D3DVERTEX(0x112)` and `D3DLVERTEX(0x1e2)` layouts—now feeds a platform-neutral world/view/projection and viewport transform. Windows builds and CTest 3/3 pass, and the user confirmed restoration of the Music Select center artwork. Evidence: [design](design/20260829-089-untransformed-direct3d-draw.md), [work log](work-logs/20260829-089-untransformed-direct3d-draw.md).*

- **작업 088 — Direct3D 3 texture Load 복사 구현 완료.** 최신 VFS 로그에서 `_3week.bmp`가 정상 로드된 반면 당시 `IDirect3DTexture2::Load`는 무조건 `DDERR_UNSUPPORTED`였으므로, 같은 root·크기의 RGB565 pixel row, source color key와 destination revision 복사를 구현했다. Windows Release build와 CTest 3/3이 통과했다. 이후 사용자 재검증에서도 중앙 그림은 나타나지 않았고 실제 trace에는 `TextureLoad` 호출이 0회였으므로 이 경계를 해당 장면의 직접 원인으로 보았던 추정은 기각됐다. 근거: [설계](design/20260829-088-direct3d-texture-load.md), [작업 로그](work-logs/20260829-088-direct3d-texture-load.md).

  *Task 088 — Direct3D 3 texture Load copy implementation complete. Despite the user's observation of a black Music Select center, the latest VFS log shows `_3week.bmp` loading successfully. The formerly unconditional `DDERR_UNSUPPORTED` implementation of `IDirect3DTexture2::Load` now copies equal-sized, same-root RGB565 pixel rows, the source color key, and destination revision. The warnings-as-errors Windows Release build and all three CTest tests, including the facade probe, pass. Later user revalidation and a trace with zero `TextureLoad` calls reject this boundary as the direct cause of that scene defect. Evidence: [design](design/20260829-088-direct3d-texture-load.md), [work log](work-logs/20260829-088-direct3d-texture-load.md).*

- **작업 087 — 키보드 코인 누적 카운터 수정 완료.** F3 한 번으로 credit이 99가 되는 사용자 관찰을 원본 port `0x105`의 `current - previous` modulo-256 계산에 귀속했다. 잘못된 `0xfe → 0xff` one-read pulse를 false→true마다 1 증가하고 read 뒤 유지되는 8비트 counter로 교체했다. 초기·hold·release·다음 press·wrap 회귀 테스트와 표준 Win32 전체 build, CTest 3/3이 통과했다. 실제 press당 credit 1 증가는 사용자 재검증 대상으로 남긴다. 근거: [설계](design/20260829-087-coin-counter-keyboard-input.md), [작업 로그](work-logs/20260829-087-coin-counter-keyboard-input.md).

  *Task 087 — keyboard coin cumulative counter correction complete. The user's observation that one F3 press produces 99 credits is attributed to the original port `0x105` current-minus-previous modulo-256 calculation. The incorrect `0xfe` then `0xff` one-read pulse is replaced by an eight-bit counter incremented on false-to-true transitions and retained after reads. Initial, hold, release, next-press, and wrap regression coverage passes with the standard complete Win32 build and CTest 3/3. User verification of exactly one credit per press remains pending. Evidence: [design](design/20260829-087-coin-counter-keyboard-input.md), [work log](work-logs/20260829-087-coin-counter-keyboard-input.md).*

- **작업 086 — DirectSound 데모 음량 설정 HLE 완료.** 실제 trace와 대응 unprotected binary에서 `GAMEASSIGNMENTS/DemoVolume=0`이 원본 table `[-10000, -2222, -1111, 0]`의 음소거 profile을 선택하는 직접 원인임을 확인했다. 제품은 원본 EXE·HDD INI를 수정하지 않고 이 key만 기본 profile 3으로 재정의하며 다른 INI 요청은 pass-through한다. master gain 기본값은 0 dB로 복원했다. 최종 실제 trace는 `SetVolume(0)`, track/master gain 1.0, 계속되는 45,056바이트 streaming refresh를 확인했고 Win32 전체 build와 CTest 3/3이 통과했다. 근거: [설계](design/20260829-086-directsound-volume-transition.md), [작업 로그](work-logs/20260829-086-directsound-volume-transition.md).

  *Task 086 — DirectSound demo-volume setting HLE complete. Runtime tracing and the corresponding unprotected binary confirm that `GAMEASSIGNMENTS/DemoVolume=0` directly selects the mute entry in the original table `[-10000, -2222, -1111, 0]`. Without modifying the original EXE or HDD INI, the product overrides only this key to profile 3 by default and passes other INI reads through. Master gain defaults to 0 dB again. The final real trace confirms `SetVolume(0)`, track/master gain 1.0, and continuing 45,056-byte streaming refreshes; the complete Win32 build and CTest 3/3 pass. Evidence: [design](design/20260829-086-directsound-volume-transition.md), [work log](work-logs/20260829-086-directsound-volume-transition.md).*

- **작업 085 — 의미 기반 EZ2DJ I/O board 완료.** 원본에서 확인한 byte `IN`/`OUT` 경계와 공개 독립 구현에서 교차 확인한 protocol 의미를 분리해 문서화하고, button·turntable·coin·light 상태를 플랫폼 중립 `Ez2DjIoBoard`로 구현했다. 외부 `--io-config` INI가 Windows keyboard 입력을 제품 실행에 공급한다. 초기 coin pulse 해석은 작업 087에서 원본 counter 계산에 맞게 정정됐다. GPL 코드는 포함하지 않았다. 근거: [설계](design/20260828-085-ez2dj-io-board-emulation.md), [작업 로그](work-logs/20260828-085-ez2dj-io-board-emulation.md).

  *Task 085 — semantic EZ2DJ I/O board complete. The byte `IN`/`OUT` boundary confirmed in the original executable is documented separately from protocol meanings cross-checked against an independent public implementation. Platform-neutral `Ez2DjIoBoard` owns buttons, turntables, coin state, and lights, while external `--io-config` INI supplies Windows keyboard input to the product run. Task 087 corrects the initial coin-pulse interpretation to match the original counter calculation. No GPL code is included. Evidence: [design](design/20260828-085-ez2dj-io-board-emulation.md), [work log](work-logs/20260828-085-ez2dj-io-board-emulation.md).*

- **작업 084 — Win32 창 모드와 메시지 pump 완료.** 기본 제품 실행은 원본 HWND를 `re2DJ` 제목의 고정 640×480 client-area 창으로 표시하고, 외부 `--fullscreen`은 원본 EXE·INI나 host display mode를 바꾸지 않고 monitor-sized popup을 선택한다. SDL Present가 매 frame event queue를 처리해 창 상호작용 뒤 Win32 message starvation을 막는다. asset-free runtime probe는 제목·style·client 크기와 양 모드 전환을 검증했고 실제 두 모드는 DrawPrimitive 루프까지 진행했다. 근거: [설계](design/20260828-084-window-mode-message-pump.md), [작업 로그](work-logs/20260828-084-window-mode-message-pump.md).

  *Task 084 — Win32 window mode and message pump complete. The default product run presents the original HWND as a fixed 640x480 client-area window titled `re2DJ`; external `--fullscreen` selects a monitor-sized popup without changing the original EXE, INI, or host display mode. SDL Present processes the event queue every frame to prevent Win32 message starvation after window interaction. The asset-free runtime probe verifies title, styles, client size, and both transitions, while real runs in both modes reached the DrawPrimitive loop. Evidence: [design](design/20260828-084-window-mode-message-pump.md), [work log](work-logs/20260828-084-window-mode-message-pump.md).*

- **작업 083 — DirectSound streaming/ring-buffer 동기화 완료.** 정적 효과음은 기존 `MIX_Audio` snapshot으로 유지하고, 관찰된 hardware-placement looping buffer는 `SDL_AudioStream`으로 분리했다. Play 시 ring 한 바퀴를 선행 큐잉하고 whole-buffer Unlock에서는 committed snapshot과 다른 최소 circular frame 구간만 추가한다. 최종 0 dB 실행은 45,056바이트 dirty 청크의 순환과 251,160–358,684바이트의 안정된 queue, AV/OpenGL 오류 0건을 확인했다. 근거: [설계](design/20260828-083-directsound-streaming-ring-buffer.md), [작업 로그](work-logs/20260828-083-directsound-streaming-ring-buffer.md).

  *Task 083 — DirectSound streaming/ring-buffer synchronization complete. Static effects retain `MIX_Audio` snapshots, while the observed hardware-placement looping buffer uses `SDL_AudioStream`. Play queues one ring revolution and whole-buffer Unlocks append only the smallest circular frame interval that differs from the committed snapshot. The final 0 dB run confirms cycling 45,056-byte dirty chunks, a stable 251,160–358,684-byte queue, and zero AV/OpenGL failures. Evidence: [design](design/20260828-083-directsound-streaming-ring-buffer.md), [work log](work-logs/20260828-083-directsound-streaming-ring-buffer.md).*

- **작업 082 — Win32 오디오 음량 추적과 원인 규명 완료.** 제품 `--audio-volume-trace`가 DirectSound buffer 형식·dB·PCM peak/RMS와 일곱 WINMM mixer import의 control 값을 별도 bounded 로그에 기록한다. 실제 `0 dB` 실행은 HLE buffer의 첫 PCM snapshot이 원본 `title.wav` 첫 청크와 정확히 일치함을 확인했다. 첫 청크와 WAV 전체 RMS 차이는 `13.14 dB`였고, 원본의 이후 ring-buffer `Unlock` 갱신은 더 큰 청크를 기록했지만 현재 SDL backend에는 반영되지 않았다. 따라서 `+12 dB` 체감 차이의 주원인은 decode 감쇠나 WINMM 누락이 아니라 조용한 첫 streaming snapshot 반복으로 확정했다. 근거: [설계](design/20260828-082-win32-audio-volume-trace.md), [작업 로그](work-logs/20260828-082-win32-audio-volume-trace.md).

  *Task 082 — Win32 audio-volume tracing and cause attribution complete. Product `--audio-volume-trace` records DirectSound buffer formats, dB, PCM peak/RMS, and control values from seven WINMM mixer imports in a separate bounded log. A real 0 dB run confirmed that the HLE buffer's first PCM snapshot exactly matches the first chunk of the original `title.wav`. The first chunk and complete WAV differ by 13.14 dB RMS; later, louder ring-buffer Unlock updates from the original never reach the current SDL backend. The perceived +12 dB difference is therefore attributed to repetition of the quiet first streaming snapshot rather than decode attenuation or a missing WINMM call. Evidence: [design](design/20260828-082-win32-audio-volume-trace.md), [work log](work-logs/20260828-082-win32-audio-volume-trace.md).*

- **작업 076 — SDL3/OpenGL 공용 backend 구현 완료.** Windows x86 facade에서 WGL과 `opengl32` 직접 경계를 제거하고, SDL3가 기존 HWND wrapping, OpenGL context, 함수 해석, drawable 크기와 present를 담당하는 공용 backend로 이전했다. 같은 source는 Win32, Linux와 Web build에 포함되며 desktop GLSL 1.20과 Web GLSL ES 1.00을 분리한다. Windows x64 preset·CI·helper script는 제거하고 Windows CI는 실제 Win32 runtime을 검증한다. 근거: [설계](design/20260827-076-sdl3-opengl-shared-backend.md), [작업 로그](work-logs/20260827-076-sdl3-opengl-shared-backend.md).

  *Task 076 — shared SDL3/OpenGL backend complete. The Windows x86 facade no longer owns direct WGL or `opengl32` boundaries. SDL3 wraps the existing HWND and provides the OpenGL context, symbol resolution, drawable sizing, and presentation in shared code compiled by Win32, Linux, and Web builds, with separate desktop GLSL 1.20 and Web GLSL ES 1.00 paths. Windows x64 presets, CI, and helper scripts are removed, and Windows CI now validates the actual Win32 runtime. Evidence: [design](design/20260827-076-sdl3-opengl-shared-backend.md), [work log](work-logs/20260827-076-sdl3-opengl-shared-backend.md).*

- **작업 070 — Direct3D 3 정점 버퍼 HLE 완료.** `IDirect3DVertexBuffer::Lock`의 nullable `lpdwSize` 계약을 바로잡고, XYZ/NORMAL/TEX1 정점 121개에 필요한 3,872바이트 storage와 원본 11×11 grid fill을 AV 없이 통과했다. 근거: [설계](design/20260826-070-direct3d3-vertex-buffer-hle.md), [작업 로그](work-logs/20260826-070-direct3d3-vertex-buffer-hle.md).

  *Task 070 — Direct3D 3 vertex-buffer HLE complete. The nullable `lpdwSize` Lock contract, 3,872-byte storage for 121 XYZ/NORMAL/TEX1 vertices, and the original 11×11 grid fill now pass without an access violation. Evidence: [design](design/20260826-070-direct3d3-vertex-buffer-hle.md), [work log](work-logs/20260826-070-direct3d3-vertex-buffer-hle.md).*

- **작업 071 — DirectSound duplicate buffer HLE 완료.** duplicate 사이 PCM storage를 공유하면서 cursor/control/Play 상태와 SDL voice를 분리했다. 원본 실행 두 번은 기존 `KSnd(ksndDuplicate)` 종료를 통과하고 AV, OpenGL 실패, SDL 오류 없이 메인 루프를 유지했다. 근거: [설계](design/20260826-071-directsound-duplicate-buffer-hle.md), [작업 로그](work-logs/20260826-071-directsound-duplicate-buffer-hle.md).

  *Task 071 — DirectSound duplicate-buffer HLE complete. Duplicates share PCM storage while retaining independent cursor/control/Play state and SDL voices. Two original runs pass the former `KSnd(ksndDuplicate)` exit and remain in the main loop without access violations, OpenGL failures, or SDL errors. Evidence: [design](design/20260826-071-directsound-duplicate-buffer-hle.md), [work log](work-logs/20260826-071-directsound-duplicate-buffer-hle.md).*

- **현재 도달점 — 보호된 원본 실행 파일의 메인 루프.** 최소 target state `0900000000000000`으로 원본 `.text` initializer를 안정적으로 복원하고, VFS read 경로, 표시 초기화, I/O port trap, 그래픽·오디오 초기화와 sound duplication을 통과했다. 실제 화면·소리·입력 정확성은 사용자 검증 대상으로 남아 있다.

  *Current milestone — protected original executable main loop. Minimal target state `0900000000000000` restores the original `.text` initializer deterministically, and the runtime passes VFS reads, display initialization, I/O-port traps, graphics/audio initialization, and sound duplication. User-visible visual, audible, and input accuracy remains to be verified.*

- **사용자 검증 결과 — 기본 화면 출력만 확인.** 화면은 표시되지만 일부 그림 누락, 투명 영역의 테두리와 매우 낮은 성능이 확인됐다. 따라서 작업 067의 OpenGL backend는 실행 경계 통과 구현으로만 완료됐으며 시각 정확성과 실시간 성능은 작업 072의 활성 항목이다.

  *User-validation result — basic output only. The display is visible, but missing images, transparent borders, and very poor performance are confirmed. Task 067 is therefore complete only as an execution-boundary implementation; visual accuracy and real-time performance are active Task 072 work.*

- **작업 072 — 렌더링·실행 성능 구현 완료.** surface identity/revision별 OpenGL texture cache, RGB565 color-key alpha, linear filtering, 관찰된 modulate·alpha test·blend factor를 적용했다. 기본 실행의 draw/I/O 상세 로그를 제한했고 `--run-detached`가 injected vectored handler로 확인된 I/O helper만 처리한다. x86/x64 build·CTest, debugger mode 무오류 실행과 detached 40초 생존을 확인했다. 실제 화면 개선은 사용자 재검증 대기다. 근거: [설계](design/20260826-072-render-correctness-performance.md), [작업 로그](work-logs/20260826-072-render-correctness-performance.md).

  *Task 072 — Rendering and runtime-performance implementation complete. The backend now uses per-surface identity/revision texture caches, RGB565 color-key alpha, linear filtering, and the observed modulate, alpha-test, and blend factors. Default draw/I/O diagnostics are bounded, while `--run-detached` uses an injected vectored handler for only the confirmed I/O helpers. x86/x64 builds and CTest pass, debugger mode remains error-free, and the detached process survives the 40-second verification window. User-visible improvement still awaits revalidation. Evidence: [design](design/20260826-072-render-correctness-performance.md), [work log](work-logs/20260826-072-render-correctness-performance.md).*

- **작업 073 — DirectDraw 오프스크린 합성 및 크래시 복구 완료.** `DDSCAPS_OFFSCREENPLAIN` RGB565/GDI surface, source-copy `Blt`/`BltFast`, inclusive source color key와 화면 대상 OpenGL 합성을 구현했다. 사용자 WER dump의 `ez2dj.exe+0x88d6` null image pointer 원인을 제거했고 수정 실행은 기존 크래시 시점을 넘어 120초 생존했다. 최종 화면 정확성은 작업 072의 사용자 검증으로 남는다. 근거: [설계](design/20260826-073-directdraw-offscreen-blit.md), [작업 로그](work-logs/20260826-073-directdraw-offscreen-blit.md).

  *Task 073 — DirectDraw offscreen composition and crash recovery complete. RGB565/GDI `DDSCAPS_OFFSCREENPLAIN` surfaces, source-copy `Blt`/`BltFast`, inclusive source keys, and OpenGL composition for visible destinations are implemented. The null image pointer behind the user's WER crash at `ez2dj.exe+0x88d6` is removed, and the updated runtime survives 120 seconds beyond the former crash point. Final visual accuracy remains Task 072 user validation. Evidence: [design](design/20260826-073-directdraw-offscreen-blit.md), [work log](work-logs/20260826-073-directdraw-offscreen-blit.md).*

## 기반 / Foundation

- Repository workflow, bilingual documentation structure, coding rules, BSD baseline, and original-asset handling policy
- HDD directory input, case-insensitive path resolution, executable scan, and target profiles
- PE32 reader, guest address space, section mapping, relocation handling, import gate assignment, and TLS directory inspection
- Replaceable `ExecutionBackend` boundary

## Native execution / Native 실행

- Windows x86 native host made the primary Windows build target; x64 expansion deferred
- Windows native helper protocol and `ExecutionBackend` adapter
- Linux i386 helper gate prototype and PE32 mapping adapter
- Web x86 engine survey and v86 separability spike; custom interpreter deferred

## Windows original process / 원본 프로세스

- Windows loader preferred-base placement of `ez2dj1.exe` at `0x00400000`
- Suspended and initial-breakpoint IAT observations
- x86 entry stop using temporary child-memory `INT3`
- x86 runtime DLL injection through same-bitness `LoadLibraryW`
- Runtime-to-original `GetCommandLineA` forwarding handoff
- First HLE `GetCommandLineA`, returning the original executable basename
- HLE `GetWindowsDirectoryA`, returning virtual `C:\\windows` mapped to the support directory beside `re2dj.exe`

## VFS foundation / VFS 기반

- `C:\\windows` support mount and `D:\\ez2dj` HDD mount resolver
- CWD-based `overlays/<target-id>` policy
- Overlay-first read and overlay-only write path foundation
- Platform-neutral `VfsFileTable` with open/read/write/seek/size/close operations
- Windows x86 runtime root injection and IAT wrappers for `CreateFileA`, `ReadFile`, `WriteFile`, `SetFilePointer`, `GetFileSize`, `GetFileType`, and `CloseHandle`
- Synthetic overlay-first read and copy-on-write verification; original-entry observation remains active work
- `USER32!LoadImageA` image-loader wrapper for the confirmed relative-path `IMAGE_BITMAP | LR_LOADFROMFILE` case, with launcher readiness tracked separately from the other VFS patches
- Bounded `.bmp`/`.str` asset-open diagnostic with calling-API tags, per-extension budgets, and a mapping-failure path that preserves the guest-visible Win32 error
- `FILE_FLAG_NO_BUFFERING` stripped at the VFS `CreateFileA` boundary, restoring the Windows 9x read semantics the original `.str` scene-script loader depends on

## Graphics correctness / 그래픽 정확성

- Direct3D color keying implemented as a shader discard gated on the guest `COLORKEYENABLE`, so keyed texels vanish under copy-style blend factors instead of being written in the key color
- `LateDraw` diagnostics report the guest `COLORKEYENABLE` and `ALPHATESTENABLE` alongside the surface key range

## Protected executable analysis / 보호 실행 파일 분석

- `--api-trace` observation: post-entry API flow with stable callers (GetVersion, CreateFileA on `\\.\LPTDI1`, WSOCK32 load/probe/free)
- Runtime confirmation that the parallel-port device path `\\.\LPTDI1` is opened by the protection stub
- Failed-open path observed with zero follow-up handle use; successful synthetic open reveals two IOCTL calls
- Optional `--device-mock-lptdi` import-thunk HLE with reserved synthetic handles and file-wrapper semantics
- Matched mock-off/on confirmation: failed LPTDI open selects the private-page #UD path, while a successful synthetic open issues two IOCTLs and reaches the original entry at `0x0043a640`
- Runtime-injected API-trace resume fixed to resume the suspended primary thread instead of continuing an already-released debug event
- First-chance access-violation diagnostics with access metadata, full registers, image-pointer windows, and return-site code windows
- Original-initialization AV attributed to `call dword [edx]` at `0x0043b683` consuming corrupt `.data` initializer slot `[0x0045c008]=0x19d521bd`
- DeviceIoControl entry/return tracing with eight arguments, bounded buffers, EAX, and bytes-returned
- Two LPTDI IOCTLs confirmed as failed, buffer-preserving calls with run-varying challenge inputs
- Optional zero-byte IOCTL-success experiment with selective IAT replacement and runtime contract coverage
- Repeated confirmation that TRUE with no response data avoids the later initializer AV but instead selects the protected stub's early private-page #UD teardown path
- HASP4/Hardlock/Win32 IOCTL background comparison with explicit vendor-identification boundary
- Full-size preserving IOCTL-success mode and repeated confirmation that bytes-returned alone does not satisfy the LPTDI check
- Synthetic-wrapper return tracing and repeatable attribution of the first 8-byte output DWORD: zero comparison at `0x01ed4253`, return load at `0x01ed4279`, and upstream nonzero checks
- Versioned external LPTDI response-profile parser, validated runtime injection, and repeated proof that first-IOCTL DWORD zero advances to `0x9c406414` while one selects the private-page #UD path
- Repeated second-response consumption attribution: DWORD0 zero advances, offsets 4–11 are XORed with an eight-byte mask derived from the second-input seed, and changing those bytes changes the initializer AV and `.data` restoration result
- Runtime-confirmed LPTDI challenge-mask transform, adaptive `--device-mock-lptdi-target-state` responses, and repeated proof that a fixed eight-byte target state removes per-run challenge variation from the initializer AV and `.data` result
- Protected `.data` byte transform recovered as `state = Advance(state); byte -= low8(state)`, with minimal target state `0900000000000000` repeatedly restoring the normal initializer and eliminating the initializer AV
- One-shot `original_initializer_window` diagnostic captured at the original entry's first `GetVersion` call
- USER32 startup tracing through window creation and display-mode setup; both host and VFS runs attribute the pre-asset exit to the failed 640×480×16 `ChangeDisplaySettingsExA` branch
- Strict-match `--hle-display-mode` import-thunk policy that accepts the observed 640×480×16 guest mode without changing the host desktop; repeated runs advance to the next Direct3D initialization blocker
- Target-specific `--d3d-init-trace` one-shot return diagnostics; repeated runs identify hardware-only `IDirect3D3::FindDevice` returning `DDERR_NOTFOUND` after successful DirectDraw and Direct3D3 interface creation
- `--hle-d3d3` Windows x86 COM facade with shared root identity, separately lived surfaces/device/viewport, virtual HAL discovery, 16-bit format enumeration, and a logical 640×480×16 flip chain; all five graphics initialization stages pass repeatedly and the former null-device AV is eliminated
- Platform-neutral legacy byte-I/O bus and target-limited `--hle-io-ports` Windows x86 trap; confirmed active-low idle ports and counter bytes advance repeatedly without privileged-instruction or access-violation failure
- Platform-neutral semantic EZ2DJ I/O board with active-low buttons, absolute turntables, a stable modulo-256 coin counter, active-high lights, and optional Windows `--io-config` keyboard bindings; GPL implementation code is not included
- DirectDraw4 `RestoreDisplayMode` cleanup contract, removing the null vtable execute AV exposed after port-I/O progress
- Controlled-exit EBP-frame attribution for the shared original helper; repeated runs identify caller `0x00424813`, KSND load failure, and detail `coin0.wav` without an access violation
- Bounded KSND search-path-state observation; repeated API traces confirm one `System/Common` entry and expose the VFS mount-root mismatch in the resulting `coin0.wav` host candidate
- Target-profile working-directory VFS source mount; original asset APIs now open `coin0.wav`, `coin1.wav`, and `WarningMsg.bmp` from the supplied read-only HDD before the next stable boundary
- RGB565 DirectDraw texture/primary/back CPU backing, GDI GetDC/ReleaseDC bitmap upload, source color key, IDirect3DTexture2 identity, and observed DDBLT_COLORFILL rectangle path; former surface null AVs are removed
- Platform-neutral transformed/lit vertex command plus a shared SDL3/OpenGL shader backend for the observed RGB565 textured triangle strip; the Windows facade wraps the original HWND while Linux and Web compile the same renderer contract
- Target-limited KSND load-stage tracing with repeatable breakpoint rearming and filename attribution; `title.wav` path/open/read and 9,438,264-byte PCM parsing succeed, while system `IDirectSound::CreateSoundBuffer` returns `E_NOTIMPL` on all ten retries without an access violation
- Platform-neutral legacy PCM/circular-lock state plus a Windows x86 DirectSound COM facade backed by pinned zlib-licensed SDL 3.4.14 and SDL_mixer 3.2.4; ordinal `DSOUND.dll` replacement advances 121 secondary buffers and 299 Lock/Unlock pairs through looping `title.wav` playback to the next Direct3D vertex-buffer boundary
- Protected `.gidata` static import surface mapped slot-by-slot; dynamic resolution observed for WSAGetLastError only
- Illegal-instruction caller identified: WOW64 win32k syscall transition inside the DLL-unload tail, not a guest branch
- Termination path attributed: stub-planted stack block → register restore at `0x01ed2730` → `.gdata` pointer jump → `ret` onto the undecrypted continuation page
- Raw-run exit code explained as residual EAX register value at the fault
- `.gtide` confirmed self-modifying with anti-disassembly obfuscation; runtime bytes differ from file bytes

## Verification / 검증

- Windows x86 warnings-as-errors builds
- Windows x86 CTest unit suite passing

## Win32 제품 loader / Win32 product loader

- 일반 `re2dj --run`에서 Windows가 원본을 main image로 적재하는 original-process engine 연결
- 제품 CLI와 진단 launcher가 공유하는 `re2dj_windows_original_process_backend` static library
- `ez2dj1stse` canonical detached HLE policy와 미검증 target의 process-creation 전 거절
- 기존 `re2dj_windows_x86_launcher_probe` 진단 option 호환 유지

*Ordinary Windows `re2dj --run` now uses the shared `re2dj_windows_original_process_backend`, preserving Windows-loader main-image execution, canonical detached HLE policy for `ez2dj1stse`, pre-creation rejection of unverified targets, and compatibility with the existing diagnostic launcher options.*

## Win32 오디오 master gain / Win32 audio master gain

- DirectSound buffer dB와 분리된 SDL mixer 최종 출력 gain
- 제품 기본값 `0 dB`, `--audio-gain-db` 허용 범위 `-24..+18 dB`
- original-process engine에서 injected runtime export로 linear gain 전달
- dummy audio runtime probe에서 master gain 적용 검증

*The Win32 audio path applies an independent SDL mixer master gain after preserving per-buffer DirectSound dB. Product execution defaults to `0 dB`, accepts `--audio-gain-db` from `-24` through `+18`, transports linear gain into the injected runtime, and verifies application with the dummy-audio runtime probe.*
