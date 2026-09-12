# 진단 로그 완전성 보완 작업 로그

## 작업 개요

사용자가 실제 게임을 진행했는데 후반 게임 로그가 없었던 원인을 재검증하고, 명시적인 `--graphics-draw-diagnostics` 조사 실행에서 logger가 기록을 조용히 버리지 않도록 수정했다.

설계: [진단 로그 완전성 설계](../design/20260913-265-diagnostic-trace-completeness.md)

작업 지시서: [진단 로그 완전성 보완 작업 지시서](../work-orders/20260913-265-diagnostic-trace-completeness.md)

## 확인된 원인

- 기존 `LateDraw` 로그는 `16,384 + 4,096 = 20,480`개 상한에 도달하면 이후 기록을 버렸다.
- 실제 실행 `20260913-024058-981`에서는 `LateDraw`가 프레임 3043에서 끝났지만 `DrawPrimitive`는 프레임 4659까지 계속되어, “게임을 진행하지 않았다”는 이전 해석이 성립하지 않았다.
- 성공 `DrawPrimitive`는 텍스처별 최초 1회로 축약되어 전체 draw 호출을 나타내지 않았다.
- `FrameDraws`는 900개, `TransformDraw`는 128개, music-select disc draw는 2,048개로 제한되어 있었다.
- VFS file query와 `ReadFile` enter/result 이벤트는 하나의 1,024 이벤트 예산을 공유했다. 해당 실행은 event 1024에서 끝났지만 파일 작업은 계속될 수 있었다.
- graphics trace formatter도 고정 1,024바이트 버퍼를 사용해 긴 `LateDraw` 레코드의 뒤쪽 필드를 자를 수 있었다.

## 변경 사항

1. `AreCompleteDiagnosticsEnabled()`를 추가했다. 이 값은 명시적으로 켜진 `--graphics-draw-diagnostics` 조사 요청과 연결된다.
2. complete mode에서는 Direct3D create/blt/surface/texture-load, draw success/failure, music-select, late draw, transform, frame summary의 기존 상한을 우회한다.
3. complete mode에서는 성공 textured draw를 텍스처별 최초 1회로 축약하지 않고 매 호출을 기록한다.
4. complete mode에서는 VFS asset/open/file/profile 진단의 상한을 우회한다. device/raw-I/O 진단의 독립 상한은 유지한다.
5. `WriteGraphicsTraceFormat()`은 필요한 길이를 먼저 계산하고, 짧은 레코드는 stack buffer, 긴 레코드는 동적 buffer를 사용한다.
6. `--graphics-draw-diagnostics`가 `--hle-d3d3`와 runtime injection을 자동으로 활성화하도록 launcher parser를 보완했다.
7. bootstrap child handoff에도 draw diagnostic 설정을 전달한다.

## 검증

- `cmd /c scripts\build_win32.bat`: 성공.
- `ctest --test-dir build/windows-x86 -C Debug -E re2dj_windows_vfs_runtime_probe --output-on-failure`: 4/4 통과.
- `re2dj_unit_tests.exe`: `checks: 1692, failures: 0`.
- `re2dj_ez2dj_keyboard_input_test.exe`: `checks: 7, failures: 0`.
- `re2dj_ez2dancer_keyboard_input_test.exe`: `checks: 8, failures: 0`.
- `re2dj_windows_product_loader_probe.exe`: profile defaults, second defaults, unsupported target, IAT slot resolution 모두 성공.
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: exit 0.
- 전체 VFS runtime probe는 창/오디오 lifecycle child probe에서 장시간 응답하지 않아 중단했다. 이 작업의 변경과 직접 관련된 enumeration 경로는 별도로 성공했으며, 해당 lifecycle probe의 완전 통과 여부는 별도 환경 조사 항목으로 남긴다.
- `git diff --check`: 통과.

## 해석 및 잔여 사항

이번 변경은 로그의 완전성만 고친다. Note/롱노트가 실제로 보이지 않는 현상의 원인이 데이터 로딩인지 렌더링인지에 대한 결론은 새 complete-capture 로그를 확보한 뒤 내려야 한다. 다음 조사에서는 기존과 같은 명령에 `--graphics-draw-diagnostics`를 포함하고, 실제 플레이 구간을 기록한 후 `.ddraw.log`와 `.vfs.log`를 함께 비교한다.

complete mode는 로그 크기와 실행 비용이 커질 수 있으므로 제품 기본 실행에는 적용하지 않는다.

## English

# Work Log: Diagnostic Trace Completeness

## Summary

Rechecked why later game logs were absent even though the user actually played, then changed the logger so an explicit `--graphics-draw-diagnostics` investigation no longer silently drops records.

Design: [Diagnostic Trace Completeness Design](../design/20260913-265-diagnostic-trace-completeness.md)

Work order: [Diagnostic Trace Completeness Work Order](../work-orders/20260913-265-diagnostic-trace-completeness.md)

## Confirmed causes

- `LateDraw` stopped recording after its `16,384 + 4,096 = 20,480` record budget.
- In run `20260913-024058-981`, `LateDraw` ended at frame 3043 while `DrawPrimitive` continued through frame 4659, so the earlier conclusion that the game had not been played was invalid.
- Successful `DrawPrimitive` records were reduced to the first report per texture and were not a complete draw stream.
- `FrameDraws`, `TransformDraw`, and music-select disc draws had independent budgets of 900, 128, and 2,048 records.
- VFS file-query and `ReadFile` enter/result records shared one 1,024-event budget. That run ended at event 1024 while file activity could continue.
- The graphics formatter used a fixed 1,024-byte buffer and could cut the tail of long `LateDraw` records.

## Changes

1. Added `AreCompleteDiagnosticsEnabled()`, connected to an explicitly enabled `--graphics-draw-diagnostics` investigation.
2. Complete mode bypasses existing limits for Direct3D create/blt/surface/texture-load, draw success/failure, music-select, late draw, transform, and frame-summary diagnostics.
3. Complete mode records every successful textured draw instead of reducing it to the first report per texture.
4. Complete mode bypasses VFS asset/open/file/profile diagnostic limits while retaining independent device/raw-I/O limits.
5. `WriteGraphicsTraceFormat()` now sizes each record first, using a stack buffer for short lines and a dynamic buffer for long lines.
6. The launcher parser now makes `--graphics-draw-diagnostics` enable `--hle-d3d3` and runtime injection automatically.
7. Bootstrap child handoff now carries the draw-diagnostics setting.

## Verification

- `cmd /c scripts\build_win32.bat`: passed.
- `ctest --test-dir build/windows-x86 -C Debug -E re2dj_windows_vfs_runtime_probe --output-on-failure`: 4/4 passed.
- `re2dj_unit_tests.exe`: `checks: 1692, failures: 0`.
- `re2dj_ez2dj_keyboard_input_test.exe`: `checks: 7, failures: 0`.
- `re2dj_ez2dancer_keyboard_input_test.exe`: `checks: 8, failures: 0`.
- `re2dj_windows_product_loader_probe.exe`: profile defaults, second defaults, unsupported target, and IAT slot resolution all passed.
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: exit 0.
- The full VFS runtime probe was stopped after its window/audio lifecycle child probe remained unresponsive. The affected enumeration path passed separately; full lifecycle-probe completion remains a separate environment investigation item.
- `git diff --check`: passed.

## Interpretation and remaining work

This change fixes logger completeness only. Whether invisible notes/long notes are caused by data loading or rendering must be decided from a new complete-capture run. The next investigation should repeat the previous command with `--graphics-draw-diagnostics`, record the actual play interval, and compare `.ddraw.log` with `.vfs.log`.

Complete mode is intentionally not enabled for product runs because it can increase log size and execution cost.
