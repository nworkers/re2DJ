# Direct3D 초기화 실패 추적 작업 지시

관련 설계: [Direct3D 초기화 실패 추적](../design/20260825-060-direct3d-init-failure-trace.md)

## 목표

`0x00422f39` 2차 null AV보다 앞선 최초 DirectDraw/Direct3D 초기화 실패를 stage·HRESULT·객체 전역 수준으로 확정한다.

## 작업 범위

1. launcher에 `--d3d-init-trace` 옵션과 다섯 일회성 return breakpoint를 추가한다.
2. 각 hit에서 EAX와 주요 graphics COM 전역을 JSONL로 기록한다.
3. breakpoint 원본 바이트를 복원하고 guest 반환값은 변경하지 않는다.
4. Windows x86 build와 CTest를 수행한다.
5. 정상 LPTDI·VFS·display HLE 조합으로 최소 두 번 재현한다.
6. 분석·TODO·아키텍처·작업 로그를 갱신하고 커밋한다.

## 완료 조건

최초 실패 stage와 HRESULT를 두 실행에서 일치하게 확인하거나, 관찰 자체를 막는 더 이른 차단점을 재현 가능한 주소로 확정한다.

## 수행 결과

두 최종 실행에서 `direct_draw=0`, `direct_3d=0x887600ff`를 동일하게 확인했다. 유효한 Direct3D3 객체와 0인 phase marker로 hardware-only `FindDevice` 실패를 확정했으며, 작업 범위의 진단과 문서 갱신을 완료했다.

---

# Direct3D Initialization-Failure Trace Work Order

Related design: [Direct3D Initialization-Failure Trace](../design/20260825-060-direct3d-init-failure-trace.md)

## Goal and scope

Identify the first graphics-initialization failure preceding the secondary AV at stage, HRESULT, and object-global level. Add one-shot return breakpoints and JSONL diagnostics, preserve guest results and original bytes, run the Windows x86 build and CTest, reproduce twice with the canonical policy combination, update cumulative documentation and the work log, and commit the task.

## Completion criteria

Observe the same first failing stage and HRESULT in two runs, or identify an earlier reproducible blocker that prevents the observation.

## Execution result

Both final runs record `direct_draw=0` and `direct_3d=0x887600ff`. The valid Direct3D3 object and zero phase markers confirm failure in hardware-only `FindDevice`; the scoped diagnostic and documentation work is complete.
