# 작업 로그: Music Select 좌표 변환 진단 및 host viewport 보정

## 결과

작업 097의 좌표 진단과 범용 화면 매핑 보정을 완료했습니다. 사용자 관찰에 따라 z 정렬을 추가로 바꾸기 전에 최신 실행 trace의 논리 좌표를 자산 크기와 대조했습니다. `20260830-120003-655.ddraw.log`의 `frame=3327`에서 297x112 `CLUBMIX_PANEL`은 x=172..469, 175x39 `DEMOPLAY`는 x=232.5..407.5에 그려졌습니다. 두 값은 논리 640x480 화면 중앙 정렬과 일치하므로, 해당 trace만으로 전역 좌표 오프셋이나 draw 순서 오류를 확정할 수 없었습니다.

진단을 위해 Direct3D facade의 bounded `LateDraw` 기록을 Music Select 이후 frame까지 확장하고, FVF와 RHW를 기록했습니다. 변환 전 정점의 x/y 범위와 변환 후 screen-space bounds를 함께 기록하는 `TransformDraw` 진단도 추가했습니다. 진단 로그는 원본 정점 전체나 자산 데이터를 저장하지 않습니다.

실제 수정은 `src/graphics/sdl3_opengl_backend.cpp`에 한정했습니다. backend는 `SDL_GetWindowSizeInPixels` 결과를 이전 `glViewport` 크기와 비교하고, native child 창의 WM_SIZE 또는 DPI 변경으로 pixel 크기가 바뀐 경우 viewport를 다시 적용합니다. guest 논리 좌표 640x480, Direct3D viewport 수식, RHW, draw 순서는 변경하지 않았습니다. 따라서 정적 좌표가 맞는 장면에서 host 크기 변경이 좌표 이동·비율 왜곡처럼 보이는 경로를 제거합니다.

## 검증

1. Windows x86 Debug build: 성공 (`scripts/build.ps1 -Preset windows-x86-debug -Configuration Debug`).
2. Windows x86 Debug CTest: 3/3 통과.
3. Windows x86 Release build: 성공 (`scripts/build.ps1 -Preset windows-x86-debug -Configuration Release`).
4. Windows x86 Release CTest: 3/3 통과.
5. 기존 product trace에서 Music Select 유사 구간의 논리 좌표와 자산 폭을 재확인했습니다.

추가로 `20260830-121711-829` ClubMix 추적을 분리 분석했습니다. 후보 디스크(`texture=21/22`)는 `key=1`, `colorkey=1`과 비키 픽셀 48,147/25,534를 보여 색상 키 discard가 전체 이미지를 지우지 않음을 확인했습니다. 반면 논리 상단 우측(`x>300`, `y<100`)으로 향하는 큰 텍스처 draw는 0회였으므로, 현재는 투명도를 직접 원인으로 확정하지 않고 좌표·상태 전환 및 512x512 합성을 미확정 항목으로 남겼습니다.

## 미확정 및 다음 검증

사용자가 본 특정 중앙 artwork의 최종 위치를 동일한 Music Select 장면에서 캡처하는 검증은 남아 있습니다. 물리 키보드로 Music Select에 진입한 뒤, 창을 이동·크기 변경하거나 DPI가 다른 모니터로 옮기고 artwork 위치·크기·겹침이 유지되는지 확인해야 합니다. 현재 보정은 Music Select 전용 오프셋을 추가하지 않았으며, 추가 오프셋은 재현 가능한 좌표 근거가 생긴 뒤에만 검토합니다.

## 추가 합성 경계 추적

`frame>=3000`에 별도 bounded `Blt`/`BltFast` 진단을 추가하고 product 실행을 다시 관찰했습니다. `20260830-133722-498.ddraw.log`의 `frame=3328`에서 visible display surface는 `id=2`, `640x480`, `caps=0x00002004`였지만, 기록된 source는 `id=174 (28x363)`, `id=242 (72x54)`, `id=253 (21x22)`뿐이었습니다. 모두 `BltFast`, `result=DD_OK`, source color key 활성 상태였으며 `512x512` 또는 `256x256` 디스크 후보 표면을 display surface에 합성한 호출은 없었습니다. 따라서 우측 상단 디스크가 늦은 display-surface blit 누락으로 사라진다는 가설은 현재 trace에서 지지되지 않습니다.

같은 실행의 `frame=603`에서 후보 표면 `texture=21/22`는 각각 `key=1`, `colorkey=1`, `nonkey=48147/25534`, `result=DD_OK`인 직접 draw였습니다. 키 픽셀 discard는 가장자리를 투명하게 만들 수 있지만 유효 픽셀 전체를 제거하지는 않습니다. 키·블렌드 경로는 후속 가장자리 품질 점검 대상으로 남기되, 현재 우선순위는 원본 애니메이션/상태 전환과 `texture=20 (512x512)` 내부 합성의 소유·좌표를 확인하는 것입니다. 키보드 코인·스타트 입력을 넣은 `20260830-135948-958` 실행에서도 관찰된 frame 범위에 새로운 상단 우측 직접 draw가 나타나지 않았습니다.

---

# Work log: Music Select coordinate diagnosis and host viewport correction

## Result

Task 097's coordinate diagnosis and generic screen-mapping correction are complete. Before changing z ordering again, the latest runtime trace was compared with resource dimensions. In `20260830-120003-655.ddraw.log` at `frame=3327`, the 297x112 `CLUBMIX_PANEL` is drawn at x=172..469 and the 175x39 `DEMOPLAY` asset at x=232.5..407.5. Both are centered in the logical 640x480 surface, so that trace alone does not establish a global coordinate offset or draw-order defect.

For diagnosis, the Direct3D facade's bounded `LateDraw` budget was extended past the Music Select frames and now records FVF and RHW. A `TransformDraw` diagnostic also records pre-transform x/y bounds alongside final screen-space bounds. The logs do not store complete original vertex payloads or asset contents.

The implementation change is limited to `src/graphics/sdl3_opengl_backend.cpp`. The backend compares `SDL_GetWindowSizeInPixels` with the last applied `glViewport` size and reapplies the viewport when a native child-window WM_SIZE or DPI change changes the pixel dimensions. Guest logical 640x480 coordinates, the Direct3D viewport formula, RHW, and draw order are unchanged. This removes the path where a correct logical scene appears displaced or distorted after host-size changes.

## Verification

1. Windows x86 Debug build succeeded (`scripts/build.ps1 -Preset windows-x86-debug -Configuration Debug`).
2. Windows x86 Debug CTest passed 3/3.
3. Windows x86 Release build succeeded (`scripts/build.ps1 -Preset windows-x86-debug -Configuration Release`).
4. Windows x86 Release CTest passed 3/3.
5. The existing product trace reconfirms the logical coordinates and resource widths in the Music Select-like interval.

The `20260830-121711-829` ClubMix trace was also isolated for analysis. Candidate discs (`texture=21/22`) show `key=1`, `colorkey=1`, and 48,147/25,534 non-key pixels, confirming that color-key discard does not remove the entire image. There are zero large textured draws targeting the logical upper-right (`x>300`, `y<100`), so transparency is not established as the direct cause; coordinate/state transition and 512x512 composition remain unresolved.

## Unresolved and next validation

The exact final position of the specific center artwork observed by the user still needs a capture of the same Music Select scene. After entering Music Select with the physical keyboard, move or resize the window or move it to a monitor with a different DPI and verify that artwork position, scale, and overlap remain stable. The correction adds no Music Select-specific offset; an offset should be considered only after reproducible coordinate evidence exists.

## Additional composition-boundary trace

A separate bounded `Blt`/`BltFast` diagnostic was enabled for `frame>=3000` and the product was observed again. At `frame=3328` in `20260830-133722-498.ddraw.log`, the visible display surface was `id=2`, `640x480`, `caps=0x00002004`, while the recorded sources were only `id=174 (28x363)`, `id=242 (72x54)`, and `id=253 (21x22)`. All were successful `BltFast` calls with source color keying active; no `512x512` or `256x256` disc-candidate surface was composed into the display surface. The trace therefore does not support a late display-surface blit omission as the reason for the missing upper-right disc.

In the same execution, direct draws at `frame=603` for candidate surfaces `texture=21/22` report `key=1`, `colorkey=1`, `nonkey=48147/25534`, and successful results. Color-key discard can affect the edge pixels, but it cannot remove all valid candidate pixels. The key and blend path remains a follow-up for edge quality; the current priority is identifying the original animation/state transition and the owner/coordinates of the internal composition of `texture=20 (512x512)`. The keyboard coin/start run `20260830-135948-958` likewise produced no new upper-right direct draw in the observed frame range.

## 내부 표면 영역 추적

`20260830-141656-891.ddraw.log`의 `frame=603`에서 `texture=20`의 `512x512` non-key/non-zero bounding box는 모두 `(0,0)-(511,511)`이었습니다. 후보 디스크 표면 `texture=21`은 `(1,9)-(255,247)`, `texture=22`는 `(4,47)-(250,208)`이었고 non-key 픽셀 수는 각각 48,147과 25,534였습니다. 따라서 표면 내부의 유효 영역 자체가 비어 있거나 색상 키로 전부 제거된 상태가 아닙니다. 두 후보는 여전히 논리 좌표 `x=195..451`, `y=3..259`의 중앙 draw 한 경로만 관찰됩니다.

## Internal surface-area trace

At `frame=603` in `20260830-141656-891.ddraw.log`, the `512x512` `texture=20` reports both `non-key` and `non-zero` bounding boxes as `(0,0)-(511,511)`. Candidate disc surface `texture=21` reports `(1,9)-(255,247)` and `texture=22` reports `(4,47)-(250,208)`, with 48,147 and 25,534 non-key pixels respectively. The surface content is therefore not empty, nor entirely removed by color keying. The two candidates still have only the central direct-draw path at logical `x=195..451`, `y=3..259` in the observed run.
