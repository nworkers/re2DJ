# 구현 완료 항목 / Implemented

완료된 구현·검증 항목을 누적 기록합니다. 상세 근거는 각 작업 로그와 설계 문서를 참조합니다.

*This document records completed implementation and verification items. Detailed evidence remains in the corresponding design and work-log documents.*

## 최신 실행 이정표 / Latest runtime milestones

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
- DirectDraw4 `RestoreDisplayMode` cleanup contract, removing the null vtable execute AV exposed after port-I/O progress
- Controlled-exit EBP-frame attribution for the shared original helper; repeated runs identify caller `0x00424813`, KSND load failure, and detail `coin0.wav` without an access violation
- Bounded KSND search-path-state observation; repeated API traces confirm one `System/Common` entry and expose the VFS mount-root mismatch in the resulting `coin0.wav` host candidate
- Target-profile working-directory VFS source mount; original asset APIs now open `coin0.wav`, `coin1.wav`, and `WarningMsg.bmp` from the supplied read-only HDD before the next stable boundary
- RGB565 DirectDraw texture/primary/back CPU backing, GDI GetDC/ReleaseDC bitmap upload, source color key, IDirect3DTexture2 identity, and observed DDBLT_COLORFILL rectangle path; former surface null AVs are removed
- Platform-neutral transformed/lit vertex command plus a Windows WGL/OpenGL shader backend for the observed RGB565 textured triangle strip; `DrawPrimitive`, stage-zero texture state, and Flip/present slots remove the former graphics AVs, with two runs reaching the later controlled `title.wav` sound-load exit
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
