# 작업 지시서: Music Select 좌표 변환 진단 및 보정

## 목적

Music Select에서 z 정렬로 보였던 화면 결함을 좌표 변환 문제와 분리한다. 현재 `LateDraw` 상한을 넘어가는 실제 호출을 진단하고, Direct3D 변환·viewport·SDL 화면 매핑 중 확인된 오류를 최소 범위로 수정한다. 진단 결과 정적 논리 좌표에는 오류 근거가 없어, 창/DPI 변경 뒤 남는 stale pixel viewport를 보정 대상으로 선택한다.

## 범위

1. 최신 ddraw trace와 변환/렌더 backend 구현을 검토한다.
2. Music Select 진입 이후까지 bounded draw 진단을 확장한다.
3. 변환 전 FVF `0x112`/`0x1e2`의 좌표·RHW·viewport 근거를 기록한다.
4. 확인된 좌표 오류와 회귀 테스트를 구현한다.
5. 설계·분석·TODO·작업 로그와 필요한 아키텍처 설명을 갱신한다.

추가 진단 범위: `frame>=3000`의 `Blt`/`BltFast`도 별도 bounded trace로 기록하여, 우측 상단 디스크가 직접 draw가 아닌 display-surface 합성으로 생성되는지 확인한다. 진단에는 source/destination surface 식별자·크기·caps·색상 키 상태와 사각형을 남기며 원본 픽셀·자산 내용은 기록하지 않는다.

후속 진단 범위: 후보 texture surface의 `non-key` 및 `non-zero` 픽셀 bounding box를 기록하여 `texture=20 (512x512)` 내부 합성 영역을 확인한다. 영역 요약만 남기고 원본 픽셀·자산 내용은 저장하지 않는다.

## 선택한 보정 / Selected correction

SDL/OpenGL backend가 `SDL_GetWindowSizeInPixels`로 확인한 크기와 이전 `glViewport` 크기가 다를 때만 viewport를 갱신한다. guest의 640x480 논리 좌표, Direct3D viewport 변환식, draw 순서는 변경하지 않는다.

The SDL/OpenGL backend refreshes `glViewport` only when the size returned by `SDL_GetWindowSizeInPixels` differs from the previously applied size. The guest 640x480 logical coordinates, Direct3D viewport transform, and draw order remain unchanged.

## 제외

- 원본 EXE/HDD 자산 저장 또는 수정
- gameplay 로직의 C++ 재구현
- 근거 없는 전역 좌표 오프셋, 강제 depth, draw 호출 재정렬

## 완료 조건

- Music Select 구간의 좌표 진단 근거가 bounded log에 남는다.
- 수정이 필요하면 논리 640x480과 host 배율 경계를 보존한다.
- Windows x86 Debug/Release 빌드와 CTest가 통과한다.
- 실제 화면 검증 결과와 미확정 항목이 작업 로그에 기록된다.

---

# Work order: Music Select coordinate conversion diagnosis and correction

## Objective

Separate the apparent Music Select z-order defect from a coordinate-conversion defect. Extend bounded draw diagnostics through the relevant calls and make the smallest evidence-based correction across the Direct3D transform, viewport, and SDL screen mapping boundaries.

## Scope

Review the latest trace and conversion/backend implementation, extend bounded diagnostics past Music Select entry, record coordinate/RHW/viewport evidence for FVF `0x112` and `0x1e2`, implement a confirmed correction with regression tests, and update design, analysis, TODO, work log, and architecture documentation as needed. Also record a separate bounded trace for `Blt`/`BltFast` at `frame>=3000` to determine whether the upper-right disc is composed into a display surface instead of being directly drawn; include source/destination surface identifiers, dimensions, caps, color-key state, and rectangles, but not original pixels or asset contents.

Follow-up diagnostic scope: record only the `non-key` and `non-zero` pixel bounding boxes of candidate texture surfaces to inspect the internal composition area of `texture=20 (512x512)`. Do not store original pixels or asset contents.

## Exclusions

Do not store or modify original EXE/HDD assets, reimplement gameplay in C++, or apply unsupported global offsets, forced depth, or draw-call reordering.

## Completion criteria

The Music Select coordinate evidence is present in a bounded log; logical 640x480 and host scaling boundaries remain intact; Windows x86 Debug/Release builds and CTest pass; and user-visible results plus unresolved items are documented.

The post-Music-Select display-surface composition boundary is also present in a bounded log, including whether color-keyed `Blt`/`BltFast` calls target the visible surface.

The candidate texture content summary is also present, including non-key and non-zero bounding boxes without storing original pixels.
