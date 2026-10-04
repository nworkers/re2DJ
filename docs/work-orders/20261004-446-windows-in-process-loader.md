# 작업 446 작업 지시서 — 공용 in-process 러너 추출 / Task 446 work order — extracting the shared in-process runner

설계: [20261004-446-windows-in-process-loader.md](../design/20261004-446-windows-in-process-loader.md)

이 지시서는 설계의 다섯 단계 중 첫 단계(446)만 다룬다. 447~450은 각자 지시서를 둔다.

*This work order covers only the first of the design's five phases (446); 447 to 450 get their own.*

## 절차 / Steps

1. `git mv`로 옮긴다: Linux 루트의 중립 러너 파일(`native_*`의 PE·thunk·모듈 이미지·SEH·게스트 스레드·runner·continuation·legacy I/O·instruction trace·observation·kernel32 진단·thread probe), 세 계약 헤더(`native_process_bootstrap.h`, `native_import_bridge.h`, `native_low_memory.h`), `original_runner.cpp`, `src/platform/native_probe_fixture.*` → `src/platform/native/`. `include/re2dj/platform/linux/original_runner.h` → `include/re2dj/platform/native/`.
   *Move with `git mv`: the neutral runner files at the Linux root, the three contract headers, `original_runner.cpp` and `src/platform/native_probe_fixture.*` to `src/platform/native/`, and the public `original_runner.h` to `include/re2dj/platform/native/`.*
2. 옮긴 파일과 그 계약을 구현하는 `linux/x86/`·`linux/x64/` 파일의 namespace를 `re2dj::platform::native`로, include 경로와 헤더 가드를 고친다. `linux/` 루트의 SDL host와 CLI는 `native::`를 쓴다.
   *Switch the moved files, and the `linux/x86/` and `linux/x64/` files implementing their contracts, to namespace `re2dj::platform::native`, fixing include paths and header guards; the SDL hosts at the `linux/` root and the CLI use `native::`.*
3. `src/platform/native/native_host_services.h`(메모리 매핑·보호·해제·코드 캐시·페이지 크기·단조 시계·잠자기·벽시계)를 만들고, 공용 파일의 `mmap`·`mprotect`·`munmap`·`__builtin___clear_cache`·`clock_*`·`sysconf`·`localtime_r`를 그것으로 바꾼다. Linux 구현은 `src/platform/linux/native_host_services.cpp`. `MapNativeLowMemory`의 `int protection`은 `HostProtection`으로.
   *Create `native_host_services.h` (mapping, protection, release, code cache, page size, monotonic clock, sleep, wall clock) and route the shared files' `mmap`, `mprotect`, `munmap`, `__builtin___clear_cache`, `clock_*`, `sysconf` and `localtime_r` through it, implemented for Linux in `linux/native_host_services.cpp`; `MapNativeLowMemory` takes a `HostProtection`.*
4. `NativeGuestFault`에 `NativeFaultKind kind`를 더하고, 두 폭의 bootstrap이 시그널을 바꿔 채운다. 공용 파일의 `SIGTRAP`·`SIGILL` 비교를 `kind`로 바꾸고 `<signal.h>`를 뺀다.
   *Add `NativeFaultKind kind` to `NativeGuestFault`, filled by both widths' bootstraps from the signal, and replace the shared files' `SIGTRAP`/`SIGILL` comparisons with it, dropping `<signal.h>`.*
5. `src/platform/native/`에 POSIX 헤더가 남지 않았는지 검사한다(`grep`).
   *Check that no POSIX header remains under `src/platform/native/` (`grep`).*
6. CMake, `src/platform/linux/README.md`, 새 `src/platform/native/README.md`, ARCHITECTURE, AGENTS.md의 디렉터리 규칙, 헌장의 Windows 실행 방향을 갱신한다.
   *Update CMake, `src/platform/linux/README.md`, a new `src/platform/native/README.md`, ARCHITECTURE, AGENTS.md's directory rules, and the charter's Windows execution direction.*
7. 검증: Linux x64·x86 debug build와 CTest(경고를 오류로), `re2dj ez2dj6th` 실행, Windows x86 build. 작업 로그.
   *Verification: the Linux x64 and x86 debug builds with CTest (warnings as errors), a `re2dj ez2dj6th` run, and the Windows x86 build; the work log.*

## 완료 조건 / Done when

Linux 동작이 이전과 같고(테스트와 6th 실행), `src/platform/native/`에 OS 헤더가 없으며, Windows 빌드가 깨지지 않는다.

*Linux behaves as before (tests and a 6th run), `src/platform/native/` holds no OS header, and the Windows build is intact.*
