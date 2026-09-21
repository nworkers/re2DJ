# 작업 로그 319: Linux guest-memory subrange protection / Work log 319: Linux guest-memory subrange protection

## 결과 / Result

Linux i386 helper의 dynamic allocation registry가 allocation마다 4 KiB page access 목록을 보관하도록 바뀌었습니다. `ProtectGuestMemory`는 allocation 내부의 page-aligned 범위를 받으며, 범위의 모든 page가 같은 prior access일 때 `mprotect`와 registry 갱신을 수행합니다. 이전 상태가 섞인 범위는 `ProtectMemoryResult`가 단일 previous-access만 표현할 수 있으므로 오류로 거부합니다.

*The Linux i386 helper's dynamic-allocation registry now stores a 4 KiB page-access list per allocation. `ProtectGuestMemory` accepts a page-aligned range inside an allocation and applies `mprotect` plus registry update when every page in the range has the same prior access. A range with mixed prior state is rejected because `ProtectMemoryResult` represents only one previous-access value.*

read/write transfer는 대상 dynamic mapping에서 닿는 모든 page의 access를 확인합니다. free는 계속 allocation의 원래 base만 받으며 전체 mapping을 해제합니다. 이 transport 변경은 Win32 `VirtualProtect`나 원본 실행 파일의 memory API 호출 세부를 구현하거나 확정하지 않습니다.

*Read/write transfer now checks access on every touched page of the target dynamic mapping. Free still accepts only the allocation's original base and releases the entire mapping. This transport change neither implements nor establishes Win32 `VirtualProtect` or the original executable's memory-API call details.*

## 검증 / Verification

- WSL2 Ubuntu 24.04에서 `bash scripts/test_linux_native_helper_probe.sh` 실행
- Linux x64 Debug CTest 1/1 통과, i386 production helper build 통과
- 공용 host probe가 8 KiB allocation의 두 번째 4 KiB를 read-only로 만들고 첫 page write 성공·두 번째 page write 거부·restore를 확인
- 기존 full-range protection, import 간 persistence/free, fault, terminal stop, capability rejection probe 유지
- Linux x86 Debug CTest와 같은 i386 helper에 대한 x86 host probe까지 포함해 script 성공 종료

*Ran `bash scripts/test_linux_native_helper_probe.sh` on WSL2 Ubuntu 24.04. Linux x64 Debug CTest passed 1/1 and the production i386 helper built successfully. The shared host probe made the second 4 KiB of an 8 KiB allocation read-only, verified successful first-page write, rejected second-page write, and restore. It retained full-range protection, cross-import persistence/free, fault, terminal-stop, and capability-rejection probes. The script completed successfully including Linux x86 Debug CTest and the x86 host probe against the same i386 helper.*
