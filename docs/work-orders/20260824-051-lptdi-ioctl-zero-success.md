# LPTDI IOCTL 0바이트 성공 응답 실험 작업 지시

관련 설계: [LPTDI IOCTL 0바이트 성공 응답 실험](../design/20260824-051-lptdi-ioctl-zero-success.md)

## 목표

두 LPTDI IOCTL의 BOOL 성공만 합성해 `.data` 복원과 원본 초기화 진행에 미치는 영향을 비교한다.

## 작업 범위

1. synthetic handle에 TRUE/0-byte/buffer-preserving 응답을 주는 runtime DeviceIoControl wrapper를 추가한다.
2. runtime probe에 wrapper 계약 검증을 추가한다.
3. launcher에 `--device-mock-lptdi-ioctl-success` 옵션과 선택적 IAT 연결·diagnostic을 추가한다.
4. build·CTest와 canonical 2회 실행을 수행한다.
5. 결과를 architecture, analysis, TODO, 작업 로그에 반영하고 커밋한다.

## 검증

CTest 계약 검증과 두 canonical 실행의 동일한 결과로 완료를 판정한다.

## 완료

runtime wrapper, probe 계약 검증, launcher 선택 옵션을 구현했다. Windows x86 build와 CTest 2/2가 통과했으며 canonical 2회가 동일하게 원본 entry 이전 private-page #UD 경로를 선택했다. 별도 exit-break 실행에서도 종료 코드 `0xc000001d`를 확인했다.

---

# LPTDI IOCTL Zero-Byte Success Experiment Work Order

Related design: [LPTDI IOCTL Zero-Byte Success Experiment](../design/20260824-051-lptdi-ioctl-zero-success.md)

## Goal

Synthesize only the success BOOL for both LPTDI IOCTLs and compare its effect on `.data` restoration and original initialization.

## Scope

Add a TRUE/zero-byte/buffer-preserving runtime wrapper and probe coverage, a launcher option with selective IAT hookup and diagnostics, build/CTest, two canonical runs, cumulative documentation, and a commit.

## Verification

Completion requires passing wrapper-contract CTest coverage and matching results from two canonical runs.

## Completion

Implemented the runtime wrapper, probe contract coverage, and selective launcher option. The Windows x86 build and CTest 2/2 passed; both canonical runs selected the same pre-original-entry private-page #UD path. A separate exit-break run also confirmed exit code 0xc000001d.
