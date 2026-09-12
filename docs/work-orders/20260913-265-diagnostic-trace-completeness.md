# 진단 로그 완전성 보완 작업 지시서

## 목표

사용자가 `--graphics-draw-diagnostics`를 명시한 조사 실행에서 게임 후반의 draw와 VFS 읽기 로그가 고정 상한 때문에 누락되지 않도록 로거를 수정한다. 옵션이 없는 제품 실행의 기본 비용과 관찰 정책은 유지한다.

## 범위

- `graphics_trace_log` formatter의 장문 레코드 truncation 제거
- Direct3D draw 진단의 complete-capture 정책 적용
- VFS asset/open/file/profile 진단의 complete-capture 정책 적용
- launcher probe가 `--graphics-draw-diagnostics`를 독립적으로 활성화하고 complete-capture를 전달하도록 보완
- 설계·아키텍처·분석·작업 로그 문서 갱신

다음은 범위에서 제외한다.

- 원본 실행 파일, CHD, 프로파일, 게임 로직 변경
- VFS가 반환하는 바이트와 렌더링 결과 변경
- device/raw-I/O trace의 독립적인 기존 상한 제거
- 무제한 진단을 제품 기본 경로에 적용

## 구현 순서

1. [진단 로그 완전성 설계](../design/20260913-265-diagnostic-trace-completeness.md)에 관찰 오류와 complete-capture 정책을 기록한다.
2. `graphics_trace_log.*`에 complete-capture accessor를 추가하고 formatter를 동적 길이로 변경한다.
3. `direct3d3_com_facade.cpp`의 관련 그래픽 진단 상한을 complete mode에서 우회한다.
4. `injected_runtime.cpp`의 asset/open/file/profile 진단 상한을 complete mode에서 우회한다.
5. launcher probe와 child handoff에서 명시적 draw diagnostic 옵션을 complete mode로 전달한다.
6. 관련 `ARCHITECTURE.md`와 분석 문서에 새 관찰 계약을 반영한다.
7. Windows x86 빌드와 CTest를 실행하고 작업 로그를 작성한다.

## 완료 조건

- `--graphics-draw-diagnostics` 없이 draw-path 진단이 켜지지 않는다.
- 해당 옵션을 사용하면 `LateDraw`가 `20,480`개에서 자동 종료되지 않는다.
- 해당 옵션에서 성공 `DrawPrimitive`가 텍스처별 최초 1회로 축약되지 않는다.
- VFS read/query와 asset/open trace가 기존 1,024/확장자별 상한에서 조용히 끊기지 않는다.
- 1024바이트를 초과하는 graphics trace 레코드가 잘리지 않는다.
- Windows x86 빌드와 기존 테스트가 통과한다.

## English

# Work Order: Diagnostic Trace Completeness

## Goal

Fix the logger so an investigation run that explicitly passes `--graphics-draw-diagnostics` does not lose later draw and VFS read records because of fixed budgets. Preserve the product path's default cost and observation policy when the option is absent.

## Scope

- Remove long-record truncation from the `graphics_trace_log` formatter.
- Apply a complete-capture policy to Direct3D draw diagnostics.
- Apply the complete-capture policy to VFS asset/open/file/profile diagnostics.
- Make the launcher probe independently activate `--graphics-draw-diagnostics` and pass complete capture through.
- Update design, architecture, analysis, and work-log documentation.

Out of scope:

- Original executable, CHD, profile, or game-logic changes.
- Changes to VFS-returned bytes or rendered output.
- Removing independent existing limits from device/raw-I/O traces.
- Enabling unbounded diagnostics on the product default path.

## Implementation order

1. Record the observation failure and complete-capture policy in the [diagnostic trace completeness design](../design/20260913-265-diagnostic-trace-completeness.md).
2. Add the complete-capture accessor and dynamic-length formatter to `graphics_trace_log.*`.
3. Bypass relevant graphics diagnostic limits in `direct3d3_com_facade.cpp` when complete mode is active.
4. Bypass asset/open/file/profile diagnostic limits in `injected_runtime.cpp` when complete mode is active.
5. Pass the explicit draw-diagnostics option as complete capture through the launcher probe and child handoff.
6. Reflect the new observation contract in `ARCHITECTURE.md` and the relevant analysis document.
7. Run the Windows x86 build and CTest, then write the work log.

## Acceptance criteria

- Draw-path diagnostics remain off without `--graphics-draw-diagnostics`.
- With the option, `LateDraw` does not stop at 20,480 records.
- With the option, successful `DrawPrimitive` calls are not reduced to one per texture.
- VFS read/query and asset/open traces do not silently stop at the existing 1,024/extension budgets.
- Graphics trace records longer than 1,024 bytes are preserved.
- The Windows x86 build and existing tests pass.
