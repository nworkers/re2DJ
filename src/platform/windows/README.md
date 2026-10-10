# src/platform/windows

공용 in-process 러너([`../native/`](../native/README.md))의 Windows 구현입니다. 작업 450에서 원본 프로세스 주입 경로(주입 런타임 DLL, COM facade, `original_process_backend`, 런처·VFS·product loader probe)를 지웠고, Windows 제품은 Linux와 같은 러너·HLE·SDL host를 씁니다([작업 446 설계](../../../docs/design/20261004-446-windows-in-process-loader.md)).

*The Windows implementation of the shared in-process runner ([`../native/`](../native/README.md)). Task 450 deleted the original-process injection path (the injected runtime DLL, the COM facades, `original_process_backend`, and the launcher, VFS and product-loader probes); the Windows product uses the same runner, HLE and SDL hosts as Linux ([task 446 design](../../../docs/design/20261004-446-windows-in-process-loader.md)).*

| 파일 | 역할 |
| --- | --- |
| `native_host_services.cpp`, `native_host_protection.h` | OS 계약(`native_host_services.h`)의 Windows 구현: `VirtualAlloc`·`VirtualProtect`·`VirtualFree`, 코드 캐시, 시계, 잠자기 |
| `native_guest_reservation.h` | 게스트 이미지 영역(0x400000~) 예약을 넘겨받고 푸는 계약 |
| `host_process_launcher.cpp` | 게스트의 자식 프로세스를 re2dj의 다른 실행으로 띄우는 런처(상속 파이프로 종료 코드) |
| `self_process.cpp` | 런처(#12)가 고른 게임을 자기 실행 파일의 새 프로세스로 실행하고 끝날 때까지 기다림(`platform/self_process.h`) |
| `x86/` | x86 backend: VEH, 그림자 TEB, naked asm 전환, 게스트 스레드, 시작 시 일시 정지 재실행과 예약([`x86/README.md`](x86/README.md)) |
| `x64/` | x64 backend 조사용 호환 모드 probe(작업 452). 제품 코드 없음([`x64/README.md`](x64/README.md)) |

*`native_host_services.cpp` and `native_host_protection.h` implement the OS contract (`native_host_services.h`) with `VirtualAlloc`, `VirtualProtect`, `VirtualFree`, the code cache, clocks and sleeping; `native_guest_reservation.h` is the contract for taking over and releasing the guest image range (0x400000 up); `host_process_launcher.cpp` starts a guest's children as other re2dj runs with the exit code over an inherited pipe; `self_process.cpp` runs the game the launcher (#12) chose as a new process of the program's own executable and waits for it (`platform/self_process.h`); `x86/` holds the x86 backend: the VEH, the shadow TEB, the naked asm transitions, guest threads, and the suspended relaunch with its reservation ([`x86/README.md`](x86/README.md)); `x64/` holds only the compatibility-mode probe researching an x64 backend (task 452), no product code ([`x64/README.md`](x64/README.md)).*
