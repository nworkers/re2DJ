# src/platform/linux

Linux x86-64 host backend와 별도 i386 production helper를 둡니다. `original_runner.cpp`는 제품 CLI를 `ExecutionBackend`에 연결합니다. `native_process_bootstrap.cpp`는 guard-page guest stack, 최소 TEB/PEB, FS selector와 signal fault context를 소유합니다. `native_ipc_host_probe.cpp`는 PE32 mapping·재배치·TLS·import gate event/reply, 제한된 stack memory IPC와 구조화된 invalid-instruction fault를 합성 이미지로 검증합니다.

*This directory contains the Linux x86-64 host backend and a separate i386 production helper. `original_runner.cpp` connects the product CLI to `ExecutionBackend`. `native_process_bootstrap.cpp` owns the guarded guest stack, minimal TEB/PEB, FS selector, and signal-fault context. `native_ipc_host_probe.cpp` validates PE32 mapping, relocation, TLS, import-gate event/reply, bounded stack-memory IPC, and a structured invalid-instruction fault with synthetic images.*

Linux POSIX 헤더는 이 플랫폼 디렉터리에서만 포함할 수 있습니다. 대소문자를 구분하는 파일 시스템 때문에 게스트 경로 해석은 `re2dj::hdd::HddRoot`를 반드시 거쳐야 합니다. 현재 제품 경로는 첫 import·process exit·guest signal fault에서 통제 정지하며, Win32 HLE dispatch는 후속 단계입니다.

*Linux POSIX headers may only be included in this platform directory. Because the file system is case-sensitive, guest path resolution must always go through `re2dj::hdd::HddRoot`. The product path currently stops in a controlled way at the first import, process exit, or guest-signal fault; Win32 HLE dispatch is the next phase.*
