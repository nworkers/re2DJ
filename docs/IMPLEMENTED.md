# 구현 완료 항목 / Implemented

완료된 구현·검증 항목을 누적 기록합니다. 상세 근거는 각 작업 로그와 설계 문서를 참조합니다.

*This document records completed implementation and verification items. Detailed evidence remains in the corresponding design and work-log documents.*

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
- Protected `.gidata` static import surface mapped slot-by-slot; dynamic resolution observed for WSAGetLastError only
- Illegal-instruction caller identified: WOW64 win32k syscall transition inside the DLL-unload tail, not a guest branch
- Termination path attributed: stub-planted stack block → register restore at `0x01ed2730` → `.gdata` pointer jump → `ret` onto the undecrypted continuation page
- Raw-run exit code explained as residual EAX register value at the fault
- `.gtide` confirmed self-modifying with anti-disassembly obfuscation; runtime bytes differ from file bytes

## Verification / 검증

- Windows x86 warnings-as-errors builds
- Windows x86 CTest unit suite passing
