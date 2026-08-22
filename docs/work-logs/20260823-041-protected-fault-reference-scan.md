# 보호 실행 파일 fault reference scan 작업 로그

관련 작업 지시: [보호 실행 파일 fault reference scan 작업 지시](../work-orders/20260823-041-protected-fault-reference-scan.md)  
관련 설계: [보호 실행 파일 fault reference scan](../design/20260823-041-protected-fault-reference-scan.md)

## 구현 결과

`re2dj_windows_x86_launcher_probe`에 `--scan-fault-references`를 추가했습니다. 이 옵션은 software entry 및 native `ExitProcess` breakpoint trace를 사용하며, first-chance `EXCEPTION_ILLEGAL_INSTRUCTION`에서만 한 번 스캔합니다.

scanner는 `VirtualQueryEx`로 committed `MEM_PRIVATE`/`MEM_IMAGE` region을 순회하고, 읽을 수 있는 region을 최대 64 KiB block으로 읽습니다. fault address와 page-aligned base를 32-bit little-endian으로 검색하며, candidate는 최대 64개로 제한합니다. JSONL summary는 scanned region/byte 수와 exact-fault/page-base match 수를 구분합니다.

## 검증 결과

1. `cmake --build --preset windows-x86-debug --config Debug` 성공.
2. `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure` 성공: 2/2 통과.
3. canonical `ez2dj.exe`에 `--scan-fault-references` 실행.
   - fault는 다시 `0xC000001D`였고 address는 실행마다 달라지는 `0x00359004`였습니다.
   - scan은 197 region, 53,836,035 byte를 조사했습니다.
   - exact fault-address match는 0개, page-base `0x00359000` match는 20개였습니다.
   - fault page는 `MEM_PRIVATE | PAGE_READWRITE`, allocation base는 기존처럼 `0x00200000`이었습니다.

## 결론

fault target을 그대로 저장한 안정적인 memory location은 이 관찰에서 확인되지 않았습니다. 발견된 page-base reference는 stack, heap 및 system image에도 있어 caller를 증명하지 않습니다. 다음 단계는 target 계산을 수행하는 register/temporary storage를 잡기 위한 더 가까운 branch 또는 data-access 관찰입니다.

---

# Protected Executable Fault Reference Scan Work Log

Related work order: [Protected Executable Fault Reference Scan Work Order](../work-orders/20260823-041-protected-fault-reference-scan.md)  
Related design: [Protected Executable Fault Reference Scan](../design/20260823-041-protected-fault-reference-scan.md)

## Implementation result

Added `--scan-fault-references` to `re2dj_windows_x86_launcher_probe`. It uses the software entry and native `ExitProcess` breakpoint trace and scans only once on the first-chance `EXCEPTION_ILLEGAL_INSTRUCTION`.

The scanner walks committed `MEM_PRIVATE`/`MEM_IMAGE` regions with `VirtualQueryEx`, reads accessible regions in blocks up to 64 KiB, and searches for the fault address and page-aligned base as 32-bit little-endian values. Candidates are capped at 64. JSONL summary separates scanned region/byte count from exact-fault and page-base match counts.

## Verification result

1. `cmake --build --preset windows-x86-debug --config Debug` succeeded.
2. `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure` succeeded: 2/2 passed.
3. Ran canonical `ez2dj.exe` with `--scan-fault-references`.
   - The fault was again `0xC000001D`, at run-varying address `0x00359004`.
   - The scan covered 197 regions and 53,836,035 bytes.
   - It found zero exact fault-address matches and 20 page-base `0x00359000` matches.
   - The fault page was `MEM_PRIVATE | PAGE_READWRITE`; allocation base remained `0x00200000`.

## Conclusion

This observation did not identify a stable memory location that stores the exact fault target. Page-base references also occur in stack, heap, and system images, so they do not prove a caller. The next step needs a closer branch or data-access observation to capture the register or temporary storage that computes the target.
