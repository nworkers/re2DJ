# 작업 지시서: Direct3D Clear를 retained frame에 연결

## 관련 설계

[Direct3D Clear와 retained frame 연결 설계](../design/20260912-257-d3d-clear-retained-frame.md)

## 작업 범위

1. 현재 branch와 작업 트리 상태를 확인한다.
2. `ez2d2m`/`ez2dj4th`의 surface caps와 `LegacyDeviceClear` trace를 근거로 profile 누락과 HLE 누락을 구분한다.
3. Direct3D `Clear`의 전체 대상 color clear를 RGB565 logical render target으로 전달한다.
4. backend 생성 전 clear를 pending 상태로 보존하고 첫 draw 전에 적용한다.
5. 전체 화면 `DDBLT_COLORFILL`도 동일한 pending/immediate 경계를 사용하도록 정리한다.
6. 설계·분석·architecture 문서를 갱신한다.
7. Windows x86 build와 기존 unit test를 실행하고 가능한 범위에서 두 profile runtime trace를 확인한다.
8. 작업 로그를 작성하고 변경을 하나의 Git commit으로 남긴다.

## 제외 범위

- profile별 retained-frame 설정 추가
- 원본 실행 파일 또는 원본 자산 수정
- 부분 `D3DRECT` clear의 backend 합성
- 별도 offscreen render target을 OpenGL target으로 재구성
- non-flip copy-present 경로의 정책 변경

## 완료 조건

- `LegacyDeviceClear`의 전체 대상 색상 clear가 HLE logical target에 반영된다.
- 첫 draw 전 호출된 clear가 유실되지 않는다.
- retained-frame 화면은 명시적 clear 이후 올바른 색상에서 시작한다.
- `ez2d2m`과 `ez2dj4th`에 적용되는 공통 원인이 문서에 확인 상태로 기록된다.
- 기존 build/test가 통과하고 관련 문서와 작업 로그가 남는다.

## English

### Related design

[Direct3D Clear to retained-frame design](../design/20260912-257-d3d-clear-retained-frame.md)

### Scope

1. Check the current branch and worktree.
2. Separate profile configuration from the HLE defect using the surface caps and `LegacyDeviceClear` traces from `ez2d2m` and `ez2dj4th`.
3. Forward full-target Direct3D color clears to the RGB565 logical render target.
4. Preserve clears that arrive before backend creation and apply them before the first draw.
5. Make full-surface `DDBLT_COLORFILL` use the same pending/immediate boundary.
6. Update the design, analysis, and architecture documents.
7. Run the Windows x86 build and existing unit tests, plus runtime trace checks where possible.
8. Write the work log and leave one Git commit for the task.

### Out of scope

- Adding a profile-specific retained-frame setting
- Modifying original executables or original assets
- Composing partial `D3DRECT` clears into the backend
- Reconstructing separate offscreen render targets as OpenGL targets
- Changing the non-flipping copy-present policy

### Completion criteria

- Full-target color clears from `LegacyDeviceClear` reach the HLE logical target.
- A clear issued before the first draw is not lost.
- Retained-frame output starts from the requested color after an explicit clear.
- The shared cause affecting `ez2d2m` and `ez2dj4th` is recorded with its confirmation status.
- Existing build/tests pass and the related documents and work log are present.
