# 작업 지시서: 장면 전환과 깊이 정렬 보정

## 목적

SDL3/OpenGL backend의 깊이 상태 누락과 장면 전환 alpha quad 경로를 점검하고, 원본 Direct3D 3 호출 계약을 보존하면서 화면 깜빡임·fade·z 정렬 문제를 줄인다.

## 범위

1. 관련 설계·분석 문서와 최신 ddraw trace 검토.
2. 공용 `LegacyFixedFunctionState` 깊이 상태와 compare function 확장.
3. SDL3/OpenGL depth buffer 및 depth state 적용.
4. Windows Direct3D facade의 Z render-state 매핑, 관찰된 inverse-source-color blend 매핑과 좁은 fade 호환성 정책.
5. 진단 로그, unit/runtime probe, Windows x86 build/CTest, canonical 실행 검증.
6. 설계·TODO·작업 로그 갱신.

## 제외

- 원본 실행 파일 또는 원본 HDD 자산 수정·저장.
- 원본 gameplay/render ordering을 C++로 재구현.
- 모든 비텍스처 geometry에 대한 전역 alpha blend 강제.
- 현재 로그로 확인되지 않은 Z-enabled 장면을 확정적으로 기술.

## 완료 조건

- depth-enabled draw가 OpenGL 상태로 전달되고 frame 경계에서 depth가 초기화된다.
- 관찰된 fade 후보는 제한된 조건에서만 source-over로 보정된다.
- 기존 2D draw와 unsupported-state 오류 계약이 회귀하지 않는다.
- Debug/Release build와 CTest가 통과한다.
- canonical run에서 transition/depth 진단 근거를 남긴다.

---

# Work order: scene-transition and depth-ordering correction

## Objective

Inspect the missing depth state in the SDL3/OpenGL backend and the transition alpha-quad path, then reduce flicker, broken fade-out, and z-order issues without changing the original Direct3D 3 call contract.

## Scope and completion criteria

Extend the shared fixed-function state with depth behavior, request and use a depth buffer, map Windows Z render states, support the observed inverse-source-color blend, and add a narrowly detected full-screen black vertex-alpha fade compatibility rule. Preserve unsupported-state failures and the existing 2D path. Verify with unit tests, Windows x86 Debug/Release builds, runtime probe, and a canonical run; document evidence and unresolved questions.
