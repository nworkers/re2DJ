# LPTDI IOCTL 0바이트 성공 응답 실험 작업 로그

관련 설계: [LPTDI IOCTL 0바이트 성공 응답 실험](../design/20260824-051-lptdi-ioctl-zero-success.md)
관련 작업 지시: [LPTDI IOCTL 0바이트 성공 응답 실험 작업 지시](../work-orders/20260824-051-lptdi-ioctl-zero-success.md)

## 변경

주입 runtime에 synthetic LPTDI handle 전용 `DeviceIoControl` wrapper를 추가했다. 이 wrapper는 output buffer를 유지하고 bytes-returned를 0으로 쓴 뒤 `TRUE`를 반환하며, 다른 handle은 host API로 전달한다. launcher의 새 `--device-mock-lptdi-ioctl-success` 옵션만 canonical IAT를 이 wrapper로 교체한다. 기존 `--device-mock-lptdi`의 host-failure baseline은 유지했다. runtime probe에는 TRUE, 0 bytes, output 무변화 계약 검증을 추가했다.

## 검증

- `cmake --build --preset windows-x86-debug --config Debug`: 통과
- `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure`: 2/2 통과
- API trace 실행 1: `20260824-010838-936.jsonl`, 원본 entry 이전 private page `0x002d6004` #UD
- API trace 실행 2: `20260824-010905-364.jsonl`, 원본 entry 이전 private page `0x00209004` #UD
- exit-break 실행: `20260824-011032-380.jsonl`, private page `0x0038b004`, 종료 코드 `0xc000001d`

두 API trace 모두 launcher diagnostic에 새 옵션이 기록됐고 host `DeviceIoControl` 호출은 없었다. 기존 baseline의 `0x0043b683` initializer call과 execute AV `0x19d521bd`도 나타나지 않았다. 대신 WSOCK32 해제 뒤 보호 스텁의 private-page 실패 choreography가 반복됐다.

## 결론

IOCTL BOOL 성공은 제어 흐름을 바꾸지만 빈 output과 결합하면 유효한 성공 경로가 아니다. 정상 원본 entry 및 `.data` initializer 복원에는 challenge에 맞는 실제 response data가 필요하다. 다음 단계는 input 생성과 output 소비 코드를 연결해 response 규칙을 도출하는 것이다.

---

# LPTDI IOCTL Zero-Byte Success Experiment Work Log

Related design: [LPTDI IOCTL Zero-Byte Success Experiment](../design/20260824-051-lptdi-ioctl-zero-success.md)
Related work order: [LPTDI IOCTL Zero-Byte Success Experiment Work Order](../work-orders/20260824-051-lptdi-ioctl-zero-success.md)

## Changes

Added an injected-runtime DeviceIoControl wrapper for synthetic LPTDI handles. It preserves output, writes zero bytes returned, returns TRUE, and forwards other handles to the host. Only the new launcher option `--device-mock-lptdi-ioctl-success` replaces the canonical IAT slot; the existing host-failure baseline remains unchanged. Runtime-probe coverage verifies the TRUE/zero-byte/unchanged-output contract.

## Verification

- Windows x86 Debug build passed.
- CTest passed 2/2.
- Two API-trace runs raised pre-original-entry #UD at per-run private pages 0x002d6004 and 0x00209004.
- A separate exit-break run raised #UD at private page 0x0038b004 and exited with 0xc000001d.

Both traced runs recorded the new launcher mode, made no host DeviceIoControl call, and did not reproduce the baseline initializer call at 0x0043b683 or execute AV at 0x19d521bd. They returned to the protected stub's private-page failure choreography after WSOCK32 unload.

## Conclusion

The IOCTL success BOOL changes control flow, but empty output is not a valid success response. Challenge-dependent response data is required to reach the original entry with restored `.data`. The next step is to connect input generation with output consumption and derive the response rule.
