# ez2dj3rd transform challenge 관찰 작업 지시

관련 설계: [ez2dj3rd transform challenge 관찰](../design/20260902-138-ez2dj3rd-transform-challenge-observation.md)

*Related design: [ez2dj3rd transform challenge observation](../design/20260902-138-ez2dj3rd-transform-challenge-observation.md).*

## 범위

1. launcher에 진단 flag `--hle-dynamic-vfs`를 추가합니다. profile 기본값이 꺼져 있어도 dynamic resolver를 켭니다. 기본 동작은 바뀌지 않습니다.
2. 3rd를 실제 실행해 `--hardlock-transform-inputs`로 challenge 목록을 기록합니다.
3. 같은 목록을 두 번째 실행으로 재현해 실행 간 동일성을 확인합니다.
4. 원본 `EZ2DJ.EXE`에서 유도 규칙을 적용한 목록과 값·순서를 대조합니다.
5. 결과를 `docs/analysis/ez2dj3rd-hardlock-function-0e.md`에 반영하고, 기록된 18의 지위를 정정합니다.
6. seed 복구 워크스루의 3rd challenge 수와 3rd 실행 절차를 갱신합니다.
7. `ARCHITECTURE.md`에 resolver gate와 진단 override를 기록하고 작업 로그를 남깁니다.

*Add the diagnostic launcher flag `--hle-dynamic-vfs`, which enables the dynamic resolver even when the profile default is off and changes no default behavior; run 3rd for real and record the challenge list with `--hardlock-transform-inputs`; reproduce the list in a second run to confirm run-to-run identity; compare it value-by-value and in order against a list derived from the original `EZ2DJ.EXE`; record the outcome in `docs/analysis/ez2dj3rd-hardlock-function-0e.md` and correct the standing of the recorded 18; update the seed recovery walkthrough's 3rd challenge count and 3rd run procedure; and record the resolver gate and the diagnostic override in `ARCHITECTURE.md` with a work log.*

## 비범위

- 3rd profile 기본값 변경.
- 3rd 응답 주입 실행과 후보 판별.
- Function `0x0e` 변환 구현과 seed 탐색.

*Out of scope: changing the 3rd profile default, injecting 3rd responses and judging candidates, and implementing the transform or searching for seeds.*

## 완료 조건

- flag 없이 실행하면 기존 동작이 그대로입니다.
- 3rd 실행 두 번이 동일한 challenge 목록을 기록합니다.
- 유도 목록이 관찰 목록을 값과 순서까지 재현합니다.
- 관찰 결과에 따라 18의 지위가 문서에서 정정됩니다.
- 원본 process가 남지 않고 원본 HDD와 overlay가 변경되지 않습니다.
- Windows x86 build가 통과합니다.

*Completion requires unchanged behavior without the flag, two 3rd runs recording the same challenge list, a derived list reproducing the observed one in value and order, the standing of the 18 corrected in the documents according to the result, no original process left running with the original HDD and overlay unchanged, and a passing Windows x86 build.*
