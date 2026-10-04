# src/platform/native

원본 PE32를 re2dj **자기 프로세스 안에서** 실행하는 OS 중립 러너입니다. Linux 러너의 공용 부분을 작업 446에서 이곳으로 옮겼고, Windows x86 backend도 같은 러너를 쓰게 됩니다([설계](../../../docs/design/20261004-446-windows-in-process-loader.md)). namespace는 `re2dj::platform::native`입니다.

*The OS-neutral runner that executes the original PE32 **inside re2dj's own process**. Task 446 moved the shared part of the Linux runner here, and the Windows x86 backend is to use the same runner ([design](../../../docs/design/20261004-446-windows-in-process-loader.md)). The namespace is `re2dj::platform::native`.*

## 구성 / Contents

| 파일 | 역할 |
| --- | --- |
| `native_pe_image`, `native_pe_session` | PE32 매핑, 재배치, TLS, import 바인딩 |
| `native_import_thunks`, `native_dynamic_thunk`, `native_import_gate.h` | 게스트가 부르는 import thunk와 그 host 쪽 handler 계약 |
| `native_guest_module_image`, `native_guest_module_set` | facade 모듈(kernel32 등)의 게스트 이미지 |
| `native_guest_seh` | 게스트 SEH 체인으로 예외를 배달할지 판단하고 dispatcher를 만든다 |
| `native_guest_threads` | 게스트 잠금과 스레드 스케줄 |
| `native_in_process_runner`, `native_continuation_observation`, `original_runner` | 실행 continuation과 CLI가 부르는 진입점 |
| `native_legacy_io`, `game_controls`, `native_instruction_trace`, `native_kernel32_diagnostic` | I/O 보드 트랩, OSD autoplay, 진단 |
| `*_observation`, `native_thread_probe`, `native_probe_fixture` | 진단 run과 합성 PE32 probe |
| `native_host_services.h` | **OS 계약**: 메모리 매핑·보호·해제, 코드 캐시, 페이지 크기, 시계, 잠자기 |
| `native_process_bootstrap.h`, `native_import_bridge.h`, `native_low_memory.h` | **backend 계약**: 게스트 스택·TEB·fault, 게스트→host 전환, 4 GiB 아래 메모리 |
| `native_guest_fault.h` | fault의 32비트 레지스터 모양과 OS 중립 `NativeFaultKind` |

## 규칙 / Rules

- 이 디렉터리는 OS 헤더(`<sys/*.h>`, `<unistd.h>`, `<signal.h>`, `<windows.h>` 등)를 포함하지 않는다. OS가 필요한 일은 `native_host_services.h`나 backend 계약을 거친다.
- 계약 구현은 OS 디렉터리에 둔다: Linux는 `../linux/native_host_services.cpp`와 `../linux/x86/`·`../linux/x64/`, Windows는 `../windows/native_host_services.cpp`와 `../windows/x86/`(작업 448). Windows에서는 HLE가 보는 TEB(`NativeGuestThread::teb`)가 그림자이고 게스트가 FS로 보는 TEB(`fs_teb`)는 host 스레드의 실제 TEB다.
- fault 판단은 `NativeGuestFault::kind`로 한다. `status_code`(시그널 번호나 예외 코드)는 로그에만 쓴다.

*Nothing here includes an OS header; OS work goes through `native_host_services.h` or the backend contracts, implemented in the OS directories (`../linux/native_host_services.cpp`, `../linux/x86/`, `../linux/x64/`, and `../windows/native_host_services.cpp` with `../windows/x86/` from task 448); on Windows the TEB the HLE sees (`NativeGuestThread::teb`) is a shadow, while the one guest code sees through FS (`fs_teb`) is the host thread's real TEB. Fault decisions use `NativeGuestFault::kind`; `status_code`, the signal number or exception code, is for logs only.*
