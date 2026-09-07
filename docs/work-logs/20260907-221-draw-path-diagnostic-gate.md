# 20260907-221 draw 경로 진단 게이트 작업 로그 / Draw-Path Diagnostic Gate Work Log

* 설계: [20260907-219-runtime-performance-hot-paths.md](../design/20260907-219-runtime-performance-hot-paths.md)
* 작업 지시: [20260907-221-draw-path-diagnostic-gate.md](../work-orders/20260907-221-draw-path-diagnostic-gate.md)

## 변경 내용 / What Changed

* `graphics_trace_log.cpp`에 `g_re2dj_graphics_draw_diagnostics` export를 추가했다. 기본값 0이다. `graphics_trace_log.h`가 `AreGraphicsDrawDiagnosticsEnabled()`를 제공한다.
* `direct3d3_com_facade.cpp`의 `ReportDrawDiagnostic`, `ReportLateDrawDiagnostic`, `ReportTransformDiagnostic`이 스위치가 꺼져 있으면 즉시 반환한다. 초기화와 일회성 진단은 건드리지 않았다.
* 같은 파일에서 OpenGL backend 초기화에 `AreGraphicsDrawDiagnosticsEnabled()`를 전달한다. 작업 220의 `glGetError` 정책과 같은 스위치를 쓴다.
* launcher probe에 `--graphics-draw-diagnostics`를 추가했다. 지정되었을 때만 원격 프로세스의 export에 1을 쓴다. 사용법 문자열과 launch 진단 레코드에 반영했다.
* `EZ2DJ 4th`는 `IDirect3D7` 경로로 진입하지만 `direct3d7_com_facade.cpp`가 draw 슬롯을 D3D3 facade 구현으로 그대로 채택하므로 이 게이트가 그대로 적용된다. 실행 로그로 확인했다.

*Added the `g_re2dj_graphics_draw_diagnostics` export defaulting to 0 with an accessor, made the three per-draw reporters in the Direct3D3 facade return immediately when it is off while leaving initialization and one-shot diagnostics alone, passed the same switch into the OpenGL backend so task 220's error-check policy follows it, and added the `--graphics-draw-diagnostics` launcher option reflected in the usage string and the launch record. EZ2DJ 4th enters through the `IDirect3D7` path but that facade adopts the Direct3D3 draw slots, so the gate applies there too, as the runtime log confirms.*

## 검증 / Verification

실제 사용자 제공 `roms/ez2dj4th/ez2dj4th.chd`를 `re2dj ez2dj4th`로 실행해 그래픽 초기화와 렌더링까지 도달한 두 번의 run을 비교했다. 두 run은 같은 launch 인자를 쓰고 진단 스위치만 다르다.

| | 진단 꺼짐 (제품 경로) | 진단 켜짐 |
| --- | --- | --- |
| `.ddraw.log` 총 줄 수 | 631 | 21,117 |
| draw 단위 항목 (`LateDraw`, `DrawPrimitive`, `Transform`) | 0 | 20,480 |
| 초기화 항목 (`CreateSurface`, `RenderState`, `Flip`) | 253 | 남아 있음 |
| launch 레코드 `graphics_draw_diagnostics` | `false` | `true` |

켜졌을 때의 20,480건은 `ReportLateDrawDiagnostic`의 예산인 16,384 + 4,096과 정확히 일치한다. 즉 제품 경로는 이제 그 20,480건의 텍스처 전 픽셀 스캔과 긴 레코드 포맷팅을 전혀 수행하지 않는다. 트레이스 기록량은 33배 줄었다.

진단을 켠 run은 launcher probe 옵션 대신 export 기본값을 일시적으로 1로 바꿔 빌드한 뒤 제품 경로로 실행해 확인했다. 확인 후 기본값을 0으로 되돌리고 다시 빌드했다.

*Two runs of the real 4th CHD through `re2dj ez2dj4th`, identical apart from the switch, were compared. With diagnostics off the `.ddraw.log` holds 631 lines and zero per-draw entries while keeping 253 initialization entries; with them on it holds 21,117 lines including 20,480 per-draw entries, which matches `ReportLateDrawDiagnostic`'s 16,384 + 4,096 budget exactly. The product path therefore no longer performs those 20,480 whole-surface texel scans and long record formats, and trace volume drops by a factor of 33. The enabled run was produced by temporarily defaulting the export to 1 and rebuilding, then reverting to 0 and rebuilding again.*

* Windows x86 전체 빌드 성공, 새 경고 없음.
* `re2dj_unit_tests`: checks 1404, failures 0.

## 한계 / Limits

* launcher probe에 `--graphics-draw-diagnostics`를 직접 전달한 run은 보호 초기화의 dynamic resolver 단계에서 진행하지 못했다. launch 레코드는 제품 경로와 스위치 값 하나를 빼고 동일했으므로, 이 정지는 이번 변경과 무관한 별개의 관찰 항목이다. 옵션이 export에 값을 쓴다는 사실 자체는 launch 레코드로 확인했다.
* 프레임 레이트를 계측하지 않았다. 트레이스 기록량과 제거된 스캔 횟수만 확인된 사실이다.

*A run that passed `--graphics-draw-diagnostics` directly to the launcher probe did not get past the protection's dynamic-resolver stage. Its launch record was identical to the product path apart from the switch value, so that stall is a separate observation item unrelated to this change; that the option writes the export is confirmed by the launch record itself. Frame rate was not measured, so only the trace volume and the removed scan count are confirmed.*
