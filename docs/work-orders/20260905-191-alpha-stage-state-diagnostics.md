# 20260905-191 Direct3D texture-stage alpha 상태 진단 작업 지시서
# 20260905-191 Direct3D Texture-Stage Alpha State Diagnostics Work Order

## 목표

동일한 `ez2dj4th` Music Select 화면에서 원본과 re2DJ의 반투명 artwork·glow 차이를 설명할 수 있도록, 원본 Direct3D stage-0 alpha 상태를 draw 시점에 관찰한다.

Determine the original Direct3D stage-0 alpha state at draw time so the transparency and glow difference between the original and re2DJ in the same `ez2dj4th` Music Select screen can be explained.

## 작업 항목

1. `ReportLateDrawDiagnostic`에 stage-0 `ALPHAOP`, `ALPHAARG1`, `ALPHAARG2`를 추가한다.
2. 같은 진단에 `LIGHTING` 상태를 추가해 FVF 0x112 normal 경로와 구분한다.
3. 기존 bounded diagnostic budget, texture identity, bounds, diffuse, blend, color-key 정보를 유지한다.
4. Windows x86 Debug/Release 빌드와 CTest를 실행한다.
5. 실제 `ez2dj4th` Music Select 실행에서 중앙 artwork와 glow에 해당하는 draw records를 수집한다.
6. alpha-stage가 원인인지, color-key filtering 또는 다른 상태가 원인인지 판정한다.

## 범위 제외

- 이번 작업에서 shader alpha semantics를 변경하지 않는다.
- blend factor 지원 범위를 임의로 확장하지 않는다.
- color-key filtering 정책을 변경하지 않는다.
- 원본 HDD/CHD, 실행 파일, 원본 픽셀을 저장소에 추가하지 않는다.

## 완료 조건

- alpha-stage 값이 draw 기록에 남는다.
- 빌드 및 테스트가 통과한다.
- 실제 실행에서 관찰된 alpha-stage/blend 조합과 원인 판정이 작업 로그에 기록된다.

---

## Goal

Observe the original Direct3D stage-0 alpha state at draw time so the transparency and glow difference in the same `ez2dj4th` Music Select screen can be explained.

## Work items

1. Add stage-0 `ALPHAOP`, `ALPHAARG1`, and `ALPHAARG2` to `ReportLateDrawDiagnostic`.
2. Add `LIGHTING` to the same diagnostic to distinguish the FVF 0x112 normal path.
3. Preserve the existing bounded budget, texture identity, bounds, diffuse, blend, and color-key fields.
4. Run Windows x86 Debug/Release builds and CTest.
5. Collect draw records for the center artwork and glow in an actual `ez2dj4th` Music Select run.
6. Classify alpha-stage semantics against color-key filtering and other state as the likely cause.

## Out of scope

- Do not change shader alpha semantics in this task.
- Do not arbitrarily expand blend-factor support.
- Do not change color-key filtering policy.
- Do not add the original HDD/CHD, executable, or original pixels to the repository.

## Completion criteria

- Alpha-stage values appear in draw diagnostics.
- Builds and tests pass.
- The observed alpha-stage/blend combinations and the cause judgement are recorded in the work log.
