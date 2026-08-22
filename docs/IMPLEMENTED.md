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
- Protected `.gidata` static import surface mapped slot-by-slot; dynamic resolution observed for WSAGetLastError only
- Illegal-instruction caller identified: WOW64 win32k syscall transition inside the DLL-unload tail, not a guest branch
- Raw-run exit code explained as residual EAX register value at the fault
- `.gtide` confirmed self-modifying with anti-disassembly obfuscation; runtime bytes differ from file bytes

## Verification / 검증

- Windows x86 warnings-as-errors builds
- Windows x86 CTest unit suite passing
