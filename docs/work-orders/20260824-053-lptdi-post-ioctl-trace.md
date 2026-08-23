# LPTDI IOCTL 복귀 후 소비 추적 작업 지시

관련 설계: [LPTDI IOCTL 복귀 후 소비 추적](../design/20260824-053-lptdi-post-ioctl-trace.md)

## 목표

합성 LPTDI `DeviceIoControl`의 guest 복귀 지점부터 output buffer가 비교·변환·분기에 사용되는 구간을 bounded instruction trace로 수집한다.

## 작업 범위

1. injected runtime의 합성 IOCTL export를 기존 API watch에 연결한다.
2. `--lptdi-post-ioctl-trace <max-steps>` 옵션과 호출별 추적 상태를 추가한다.
3. instruction bytes, GP registers, output-range register alias와 종료 이유를 diagnostic에 기록한다.
4. Windows x86 build와 CTest를 수행한다.
5. full-size preserving canonical 실행을 두 번 이상 수행해 소비 지점을 비교한다.
6. 확인된 사실을 analysis, 설계, 작업 로그와 TODO에 반영하고 커밋한다.

## 완료 조건

full-size preserving 경로에서 실제 도달한 합성 LPTDI IOCTL의 entry와 return을 관찰할 수 있고, 복귀 후 output 소비에 관한 반복 가능한 instruction/register 증거를 확보해야 한다. 기존 runtime probe와 API trace 검증은 회귀하지 않아야 한다.

## 완료

합성 래퍼 API watch, return breakpoint, bounded instruction trace와 외부 call resume breakpoint를 구현했다. 첫 IOCTL의 세 번 반복과 첫 output DWORD의 zero 비교·반환·상위 nonzero 검사를 반복 확인했다. Windows x86 build와 CTest 2/2가 통과했고 최종 canonical 두 실행은 기존 private-page #UD 경로를 유지했다. full-size 경로에서는 두 번째 IOCTL에 도달하지 않아 그 output 소비는 다음 response-profile 실험과 함께 계속 추적한다.

---

# LPTDI Post-IOCTL Consumption Trace Work Order

Related design: [LPTDI Post-IOCTL Consumption Trace](../design/20260824-053-lptdi-post-ioctl-trace.md)

## Goal

Collect a bounded instruction trace from the guest return point of synthetic LPTDI DeviceIoControl through the output buffer's comparison, transformation, and branch use.

## Scope

Connect the injected synthetic IOCTL export to the existing API watch, add `--lptdi-post-ioctl-trace <max-steps>` and per-call state, record instruction bytes/registers/output aliases/end reasons, run the Windows x86 build and CTest, compare at least two canonical full-size runs, update cumulative documentation, and commit.

## Completion criteria

The trace must observe entry and return for every synthetic LPTDI IOCTL reached on the full-size-preserving path and produce repeatable instruction/register evidence of output consumption without regressing the runtime probe or ordinary API trace.

## Completion

Implemented a synthetic-wrapper API watch, return breakpoint, bounded instruction trace, and external-call resume breakpoint. Repeated runs confirmed three first-IOCTL attempts and the first output DWORD's zero comparison, return, and upstream nonzero checks. The Windows x86 build and CTest 2/2 passed, and two final canonical runs retained the existing private-page #UD path. This full-size path did not reach the second IOCTL, so its consumption remains part of the next response-profile experiment.
