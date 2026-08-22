# src/platform/linux

Linux x86-64 전용 backend와 native helper probe를 둡니다. 현재 `native_ipc_host_probe.cpp`와 별도 i386 `native_ipc_helper_probe.cpp`가 protocol v3 gate event/reply 및 제한된 stack memory IPC를 검증합니다.

*Linux x86-64 backend and native-helper probes. `native_ipc_host_probe.cpp` and the separate i386 `native_ipc_helper_probe.cpp` currently validate protocol-v3 gate event/reply and bounded stack-memory IPC.*

여기서만 POSIX 헤더를 포함할 수 있습니다. 대소문자를 구분하는 파일 시스템 때문에 게스트 경로 해석은 `re2dj::hdd::HddRoot`를 반드시 거쳐야 합니다.

*This is the only place that may include POSIX headers. Because the file system is case-sensitive, guest path resolution must always go through `re2dj::hdd::HddRoot`.*
