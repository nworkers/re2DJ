# 20260907-221 draw 경로 진단 게이트 계획 / Draw-Path Diagnostic Gate Work Order

설계: [20260907-219-runtime-performance-hot-paths.md](../design/20260907-219-runtime-performance-hot-paths.md)

## 목표 / Goal

draw 경로 안에서 실행되는 진단을 명시적 스위치 뒤로 옮긴다. 제품 실행 경로에서는 꺼져 있고, 조사 목적일 때만 켠다.

*Move the diagnostics that run inside the draw path behind an explicit switch: off on the product execution path, on only for investigation.*

## 작업 항목 / Work Items

1. `src/platform/windows/graphics_trace_log.h`, `graphics_trace_log.cpp`
   * `g_re2dj_graphics_draw_diagnostics` export를 추가한다. 기본값 0.
   * `AreGraphicsDrawDiagnosticsEnabled()`를 제공한다.
2. `src/platform/windows/direct3d3_com_facade.cpp`
   * `ReportDrawDiagnostic`, `ReportLateDrawDiagnostic`, `ReportTransformDiagnostic`이 스위치가 꺼져 있으면 즉시 반환하도록 한다.
   * 초기화와 일회성 진단은 건드리지 않는다.
3. `src/tools/windows_x86_launcher_probe/main.cpp`
   * `--graphics-draw-diagnostics` 옵션을 추가한다.
   * 지정된 경우에만 주입 런타임의 새 export에 1을 쓴다.
   * 사용법 문자열과 launch 진단 레코드에 반영한다.
4. 이 스위치를 필요로 하는 기존 가이드 문서에 사용법을 반영한다.

*Add the `g_re2dj_graphics_draw_diagnostics` export defaulting to 0 with an `AreGraphicsDrawDiagnosticsEnabled()` accessor; make the three per-draw diagnostic reporters return immediately when it is off while leaving initialization and one-shot diagnostics untouched; add a `--graphics-draw-diagnostics` launcher option that writes 1 into the export only when passed, reflected in the usage string and the launch diagnostic record; and update the guides that depend on the draw trace.*

## 제약 / Constraints

* 기본값은 반드시 꺼짐이다. 제품 경로가 진단 비용을 지불하지 않아야 한다.
* 부팅 실패 조사에 쓰이는 일회성 진단은 계속 기록되어야 한다.
* export 이름은 launcher가 이름으로 조회하므로 정확히 유지한다.
* 소스 주석은 영어로만 작성한다.

*The default must be off so the product path pays nothing; the one-shot diagnostics used for boot investigations must keep recording; the export name is looked up by name and must stay exact; source comments are English only.*

## 검증 / Verification

* Windows x86 빌드 성공.
* `re2dj_unit_tests` 전체 통과.
* 옵션 없이 실행했을 때 `.ddraw.log`에 draw 단위 항목이 남지 않고, 초기화 항목은 남는지 확인한다.
* 옵션을 주었을 때 기존과 동일한 draw 트레이스가 남는지 확인한다.

*Build Windows x86, pass the unit tests, confirm that without the option the `.ddraw.log` carries initialization entries but no per-draw entries, and that with the option the previous draw trace is produced.*
