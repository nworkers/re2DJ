# 작업 로그: 프레임별 draw 요약 진단

## 한국어

### 관련 문서

- 설계: [프레임별 draw 요약 진단 설계](../design/20260908-234-frame-draw-summary.md)
- 작업 지시: [프레임별 draw 요약 진단](../work-orders/20260908-234-frame-draw-summary.md)
- 후속 작업: [페이드 블렌드 인자 보존](20260908-235-fade-blend-factors.md)

### 해결한 문제

사용자가 보고한 첫 번째 문제, Warning에서 로고로 넘어갈 때의 깜빡임을 판별할 근거를 만들었습니다. 이 작업은 진단만 추가하며 화면 동작을 바꾸지 않습니다.

### 코드 변경

| 항목 | 내용 |
| --- | --- |
| `RootFacade` | `frame_draw_calls`, `frame_draw_vertices`, `frame_textured_draw_calls`, `frame_draw_summary_count`, `frame_first_diffuse`, `frame_first_diffuse_seen`, `frame_last_diffuse` 추가 |
| `CountFrameDraw` | draw 수·정점 수·텍스처 묶인 draw 수 누적 |
| `RecordFrameDiffuse` | 복호화된 draw command에서 프레임의 첫·마지막 정점 색 기록 |
| `SurfaceFlip` | `FrameDraws:frame=..:draws=..:vertices=..:textured=..:diffuse=0x..:last=0x..` 한 줄 기록 후 초기화 |

계수 지점은 `DeviceDrawPrimitive` 한 곳입니다. `DeviceDrawPrimitiveVB`와 `DeviceDrawIndexedPrimitiveVB`는 스스로 그리지 않고 이 함수로 전달하므로, 각 진입점에서 세면 VB draw가 두 번 계수됩니다.

색은 원시 포인터에서 오프셋으로 읽지 않고 복호화된 `LegacyDrawCommand`에서 읽습니다. 정점 배치 해석을 두 곳에 두지 않기 위해서입니다.

예산은 `Flip` 진단(8)과 따로 900으로 잡았습니다. `Flip` 예산은 부팅 구간을 담지 못하고, 이 기록은 프레임당 한 줄이므로 자체 상한이 필요합니다.

기록은 `--graphics-draw-diagnostics`와 무관하게 graphics trace 파일(`*.ddraw.log`)로 나갑니다. 진단 로그(`*.jsonl`)가 아닙니다.

### 검증 — 결과

`re2dj ez2dj1stse` 부팅 9초에서 470줄을 얻었습니다. 이 기록이 다음을 확정했습니다.

| 프레임 | 기록 | 해석 |
| --- | --- | --- |
| 150–179 | `draws=1 textured=1 diffuse=0xffffffff` | Warning 유지 |
| 180–200 | `draws=2 textured=1 last=0xff000000 → 0x0f000000` | Warning 위에 전체 화면 검은 사각형, alpha가 `0x0c`씩 감소 |
| 201 | `draws=1 textured=1 diffuse=0xffffffff` | 오버레이 없이 전체 밝기 draw 한 프레임 |
| 202–376 | `draws=2 textured=2 diffuse=0xff000000 → 0xffffffff` | 로고 장면이 검정에서 밝아짐 |

**확인됨.** 게스트는 페이드 구간에도 매 프레임 그립니다. 깜빡임은 표시 경로가 프레임을 빠뜨려 생긴 것이 아닙니다.

`RenderState` 기록은 상태별로 값이 바뀔 때만, 상태당 8회까지 남습니다. `state=27`(`ALPHABLENDENABLE`)이 프레임 201에 한 번만 나타나므로, **게스트는 프레임 200까지 `ALPHABLENDENABLE`을 한 번도 호출하지 않았습니다.** `state=19`(`SRCBLEND`)와 `state=20`(`DESTBLEND`)은 프레임 0에 각각 `0x01`(`ZERO`)과 `0x05`(`SRCALPHA`)로 설정됩니다.

이 두 가지가 [작업 235](20260908-235-fade-blend-factors.md)의 근거입니다.

### 검증 — 시험과 회귀

- Windows x86 Release 전체 build 성공
- `re2dj_unit_tests.exe` → `checks: 1421, failures: 0`
- `re2dj_windows_product_loader_probe.exe` → 4개 항목 ok
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` → exit 0
- 3rd·4th를 실행해 attract 화면 캡처를 확인했습니다. 두 제품 모두 정상입니다.

### 남은 과제

- 프레임 201의 전체 밝기 draw. [작업 235](20260908-235-fade-blend-factors.md)에서 다룹니다.

## English

### Related documents

- Design: [Per-frame Draw Summary Diagnostic Design](../design/20260908-234-frame-draw-summary.md)
- Work order: [Per-frame Draw Summary Diagnostic](../work-orders/20260908-234-frame-draw-summary.md)
- Follow-up: [Preserving the guest's fade blend factors](20260908-235-fade-blend-factors.md)

### Problem solved

This task produced the evidence needed to decide the user's first reported problem, the flicker on the Warning-to-logo transition. It adds diagnostics only and does not change what is displayed.

### Code change

`RootFacade` gained per-frame accumulators — `frame_draw_calls`, `frame_draw_vertices`, `frame_textured_draw_calls`, `frame_draw_summary_count`, `frame_first_diffuse`, `frame_first_diffuse_seen`, and `frame_last_diffuse`. `CountFrameDraw` accumulates the counts, `RecordFrameDiffuse` records the frame's first and last vertex colour, and `SurfaceFlip` writes one `FrameDraws:frame=..:draws=..:vertices=..:textured=..:diffuse=0x..:last=0x..` line and resets.

Counting happens at `DeviceDrawPrimitive` alone. `DeviceDrawPrimitiveVB` and `DeviceDrawIndexedPrimitiveVB` do not draw themselves but forward here, so counting at each entry point would double every vertex-buffer draw.

The colour is read from the decoded `LegacyDrawCommand` rather than from the raw pointer at a byte offset, so the vertex layout is interpreted in one place only.

The budget is 900, separate from the `Flip` diagnostic's 8: that budget cannot cover a boot sequence, and this record is one line per frame, so it needs a limit of its own.

The records go to the graphics trace file (`*.ddraw.log`), not the diagnostic log (`*.jsonl`), and are written regardless of `--graphics-draw-diagnostics`.

### Verification — result

Nine seconds of `re2dj ez2dj1stse` boot produced 470 lines, which settled the following. Frames 150 to 179 show `draws=1 textured=1 diffuse=0xffffffff`, the held Warning. Frames 180 to 200 show `draws=2 textured=1` with the last colour stepping from `0xff000000` down to `0x0f000000` in decrements of `0x0c` — a full-screen black quad over the Warning. Frame 201 shows `draws=1 textured=1 diffuse=0xffffffff`, one full-brightness frame with no overlay. Frames 202 to 376 show `draws=2 textured=2` with the first colour rising from `0xff000000` to `0xffffffff`, the logo scene brightening from black.

**Confirmed.** The guest draws every frame through the fade, so the flicker is not a frame the presentation path dropped.

`RenderState` records are written only when a state's value changes, up to eight times per state. `state=27` (`ALPHABLENDENABLE`) appears exactly once, at frame 201, so **the guest never called `ALPHABLENDENABLE` up to frame 200**. `state=19` (`SRCBLEND`) and `state=20` (`DESTBLEND`) are set at frame 0 to `0x01` (`ZERO`) and `0x05` (`SRCALPHA`).

Those two facts are the basis for [task 235](20260908-235-fade-blend-factors.md).

### Verification — tests and regression

The full Windows x86 Release build succeeded, `re2dj_unit_tests.exe` reported `checks: 1421, failures: 0`, the product-loader probe reported all four items ok, and `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only` exited 0. 3rd and 4th were run and their attract screens captured; both are correct.

### Remaining work

- The full-brightness draw at frame 201, addressed in [task 235](20260908-235-fade-blend-factors.md).
