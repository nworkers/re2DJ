# 작업 로그 315 — Linux guest memory lifecycle / Work log 315 — Linux guest memory lifecycle

## 결과 / Result

Linux x86 및 x64 product host와 공용 i386 helper 사이의 native-helper protocol을 v4로 올리고, pending import 동안 anonymous committed guest mapping을 allocate·protect·free하는 전송을 구현했습니다. `ExecutionBackend`는 고정 폭 `GuestMemoryAccess`와 lifecycle 연산을 제공하며, Linux backend는 helper result의 32비트 base와 page-rounded size를 검증합니다.

*The native-helper protocol between Linux x86/x64 product hosts and the common i386 helper is now v4, adding transport for allocation, protection, and release of anonymous committed guest mappings while an import is pending. `ExecutionBackend` exposes fixed-width `GuestMemoryAccess` lifecycle operations, and the Linux backend validates the helper result's 32-bit base and page-rounded size.*

helper는 dynamic mapping registry에 mapping의 base, size, access를 저장합니다. image와 pending stack의 기존 전송 범위는 유지했고, dynamic 영역은 `ReadMemory`에 read 권한, `WriteMemory`에 write 권한을 추가로 요구합니다. read-only 보호 뒤 write와 free 뒤 read는 오류 packet으로 거부되므로 helper가 보호된 주소를 직접 복사해 fault 내지 않습니다. allocation은 exact base로만 해제하며 helper가 exit·fault·load failure로 끝날 때 RAII cleanup으로 남은 mapping을 정리합니다.

*The helper stores each dynamic mapping's base, size, and access in a registry. Existing image and pending-stack transport remains available, while dynamic ranges additionally require read access for `ReadMemory` and write access for `WriteMemory`. A write after read-only protection and a read after free are rejected through error packets, so the helper never faults by copying a protected address. Allocation release requires the exact base, and RAII cleanup releases remaining mappings on helper exit, fault, and load failure.*

동일 synthetic import fixture는 첫 gate에서 5,000-byte request를 8 KiB mapping으로 반올림해 할당하고, read/write round-trip, read-only 전환 뒤 write 거부, RW 복원, free 뒤 read 거부를 검사합니다. 이 단계는 실제 `VirtualAlloc` HLE 또는 reserve/commit/decommit의 Win32 의미를 구현하지 않으며, 그 정책은 실제 원본 import 관찰 뒤에 추가합니다.

*At the first gate, the shared synthetic import fixture rounds a 5,000-byte request to an 8 KiB mapping and checks read/write round trip, rejected write after read-only protection, RW restoration, and rejected read after free. This does not implement actual `VirtualAlloc` HLE or Win32 reserve/commit/decommit semantics; add that policy after observing original imports.*

## 검증 / Verification

- WSL2 Ubuntu 24.04에서 Linux x64 Debug CTest 1/1 통과
- i386 helper build 통과
- Linux x64 host probe가 lifecycle fixture, 기존 `result=51`, `child=0`, fault signal 4, terminal stop fixture를 통과
- Linux x86 Debug CTest 1/1 통과
- Linux x86 host probe가 같은 i386 helper로 동일 lifecycle 및 기존 fixture를 통과
- Windows x86 Debug build가 공용 `ExecutionBackend` 및 unit-test fake 변경을 포함해 통과
- `git diff --check` 통과

*On WSL2 Ubuntu 24.04, Linux x64 Debug CTest passed 1/1 and the i386 helper built. The Linux x64 host probe passed the lifecycle fixture plus the existing `result=51`, `child=0`, fault-signal-4, and terminal-stop fixtures. Linux x86 Debug CTest passed 1/1, and its host probe passed the same lifecycle and existing fixtures against the same i386 helper. The Windows x86 Debug build also passed with the shared `ExecutionBackend` and unit-test-fake changes. `git diff --check` passed.*

SDL configure 출력은 optional `fribidi`, `libthai`, `libdecor-0`, `libunwind` development package가 없음을 계속 보고했습니다. 이들은 이번 helper transport와 CTest/probe 성공을 막지 않았고 설치하지 않았습니다.

*SDL configure output continued to report missing optional `fribidi`, `libthai`, `libdecor-0`, and `libunwind` development packages. They did not block this helper transport, CTest, or probe validation, so this task did not install them.*

## 다음 단계 / Next step

원본 실행 파일의 실제 `kernel32` memory API 호출이 확인되면, target별 Win32 allocation flags와 reserve/commit 정책을 shared HLE로 설계하고 이 transport에 연결합니다. 그 전에는 process/module/handle/last-error 상태와 API 선택 근거를 함께 확정해야 합니다.

*When actual `kernel32` memory-API calls are observed in an original executable, design target-specific Win32 allocation flags and reserve/commit policy in shared HLE, then connect it to this transport. Before then, establish process/module/handle/last-error state and the evidence for API selection together.*
