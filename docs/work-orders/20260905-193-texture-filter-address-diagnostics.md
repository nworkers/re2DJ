# 20260905-193 텍스처 필터·주소 모드 진단 작업 지시서

## 목표

Music Select 중앙 artwork의 alpha/blend 상태와 별개로, 컬러키 텍스처의 필터·주소 모드가 원본 Direct3D 요청과 OpenGL backend에서 다르게 적용되는지 확인한다.

Determine whether color-keyed texture filtering or addressing differs between the original Direct3D request and the OpenGL backend, independently of the Music Select center artwork's alpha/blend state.

## 작업 항목

1. `ReportLateDrawDiagnostic`에 stage-0 `MINFILTER`, `MAGFILTER`, `ADDRESSU`, `ADDRESSV`를 추가한다.
2. 기존 bounded trace budget과 texture content summary를 유지한다.
3. 기존 shader, color-key discard, blend mapping, sampler 동작은 변경하지 않는다.
4. Windows x86 Debug 빌드와 단위 테스트를 실행한다.
5. 사용자 확인 Music Select 실행에서 중앙 artwork와 선택 링의 새 상태를 분석한다.
6. 새 값이 backend 누락을 입증할 때만 후속 의미 수정 작업을 별도 설계한다.

## 완료 조건

- `LateDraw`에 네 가지 stage filter/address 값이 기록된다.
- 빌드와 단위 테스트가 통과한다.
- 사용자 확인 실행의 결과와 후속 후보가 작업 로그에 기록된다.

---

# 20260905-193 Texture-Filter and Address-Mode Diagnostics Work Order

## Goal

Determine whether color-keyed texture filtering or addressing differs between the original Direct3D request and the OpenGL backend, independently of the Music Select center artwork's alpha/blend state.

## Work items

1. Add stage-0 `MINFILTER`, `MAGFILTER`, `ADDRESSU`, and `ADDRESSV` to `ReportLateDrawDiagnostic`.
2. Preserve the existing bounded trace budget and texture-content summary.
3. Do not change the shader, color-key discard, blend mapping, or sampler behavior.
4. Run the Windows x86 Debug build and unit tests.
5. Analyze the new state from the user-confirmed Music Select run for the center artwork and selection ring.
6. Design a separate semantic change only if the new values demonstrate a backend omission.

## Completion criteria

- The four stage filter/address values appear in `LateDraw`.
- The build and unit tests pass.
- The user-confirmed run and the next candidate are recorded in the work log.
