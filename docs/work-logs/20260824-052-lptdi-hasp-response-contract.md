# LPTDI/HASP 응답 계약 탐색 작업 로그

관련 설계: [LPTDI/HASP 응답 계약 탐색](../design/20260824-052-lptdi-hasp-response-contract.md)
관련 작업 지시: [LPTDI/HASP 응답 계약 탐색 작업 지시](../work-orders/20260824-052-lptdi-hasp-response-contract.md)

## 조사

Aladdin HASP4 Programmer's Guide에서 Service 2 `HaspCode`가 seed와 네 개의 16-bit return code를 사용함을 확인했다. 첫 LPTDI IOCTL의 4-byte input/8-byte output과 정확히 같은 폭이다. 반면 공개 driver inventory와 독립 호환성 조사에서 classic HASP는 `hasp95.vxd`/`haspnt.sys`, device `\\.\HASP`, 28-byte packet 계열로 설명된다. `LPTDI`는 다른 동글 보호 셸 사례에서도 나타나므로 Aladdin driver 이름이 아니라 protector의 병렬포트 추상화일 가능성이 높다.

외부 구현은 BSD 호환 라이선스가 확인되지 않았고 일부는 원출처가 불명확해 코드를 재사용하지 않았다. 공개 문서의 API shape와 실행 관찰만 사용했다.

## 구현

주입 runtime에 `g_re2dj_device_ioctl_mode` export를 추가했다. mode 1은 기존 TRUE/0-byte/buffer-preserving 계약을 유지하고, mode 2는 TRUE/full-output-size/buffer-preserving 계약을 제공한다. launcher의 `--device-mock-lptdi-ioctl-full-success`가 mode 2와 DeviceIoControl IAT wrapper를 선택한다. runtime probe는 두 mode의 return, last-error, bytes-returned와 output 불변을 검증한다.

## 검증 결과

- Windows x86 Debug build: 통과
- CTest: 2/2 통과
- `20260824-012054-417.jsonl`: private page `0x00310004` #UD
- `20260824-012118-392.jsonl`: private page `0x00237004` #UD

두 canonical 실행 모두 새 mode diagnostic을 기록했고 host DeviceIoControl 호출, 원본 `.text` API, initializer AV는 없었다. zero-byte mode와 같은 WSOCK32 unload 후 실패 choreography를 선택했다.

## 결론

BOOL과 bytes-returned는 각각 제어 흐름 입력이지만 full size만으로도 검사를 통과하지 못한다. 호출 전 고정 output도 그대로는 유효 응답이 아니다. 다음 단계는 두 caller 이후 output read/compare/XOR를 추적하고, 저장소 밖 사용자 제공 response profile을 runtime HLE에 주입하는 것이다.

---

# LPTDI/HASP Response-Contract Exploration Work Log

Related design: [LPTDI/HASP Response-Contract Exploration](../design/20260824-052-lptdi-hasp-response-contract.md)
Related work order: [LPTDI/HASP Response-Contract Exploration Work Order](../work-orders/20260824-052-lptdi-hasp-response-contract.md)

## Research

The Aladdin HASP4 Programmer's Guide defines Service 2 HaspCode as a seed with four 16-bit return codes, exactly matching the first LPTDI IOCTL's four-byte input and eight-byte output widths. Public driver inventories and independent compatibility notes instead describe classic HASP through hasp95/haspnt, device `\\.\HASP`, and a 28-byte packet family. LPTDI also appears in another dongle protection shell, making it more likely a protector parallel-port abstraction than an Aladdin device name.

No external implementation code was reused because a BSD-compatible license and clean provenance were not established. Only published API shapes and runtime observations informed this work.

## Implementation

Added exported runtime policy `g_re2dj_device_ioctl_mode`. Mode 1 preserves TRUE/zero-byte/unchanged-output behavior; mode 2 returns TRUE/full-output-size/unchanged-output. Launcher option `--device-mock-lptdi-ioctl-full-success` selects mode 2 and the DeviceIoControl IAT wrapper. Runtime-probe coverage verifies return, last error, byte count, and unchanged output for both modes.

## Verification and conclusion

The Windows x86 Debug build and CTest 2/2 passed. Canonical logs `20260824-012054-417.jsonl` and `20260824-012118-392.jsonl` raised #UD at private pages 0x00310004 and 0x00237004. Neither run reached host DeviceIoControl, original `.text` APIs, or the initializer AV; both selected the same failure choreography as zero-byte success.

BOOL and bytes-returned affect the contract, but full size alone is insufficient, and the pre-call fixed output is not valid unchanged. Next, trace output reads/compares/XORs and inject a user-supplied response profile from outside the repository.
