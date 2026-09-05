# 작업 지시 197: Direct3D cull 상태 전달

# Work Order 197: Direct3D Cull-State Forwarding

## 목적 / Purpose

Music Select 원판 draw에서 확인된 `D3DCULL_CCW`를 OpenGL backend에 전달하여, culling 누락이 화면 과노출의 원인인지 직접 비교합니다.

*Forward the `D3DCULL_CCW` observed on the Music Select disc draws into the OpenGL backend so the effect of missing culling can be compared directly.*

## 작업 범위 / Scope

1. 공용 fixed-function state에 cull mode를 추가합니다.
2. Direct3D facade에서 `D3DCULL_NONE/CW/CCW`를 변환합니다.
3. OpenGL Draw에 D3D-to-OpenGL winding mapping과 back-face culling을 적용합니다.
4. line draw와 Present에서 culling 상태를 정리합니다.
5. 설계·KB·분석·작업 로그를 갱신하고 build/test를 실행합니다.

*Add cull mode to the shared fixed-function state, convert `D3DCULL_NONE/CW/CCW` in the Direct3D facade, apply the D3D-to-OpenGL winding mapping and back-face culling in OpenGL Draw, clean up culling for line draws and Present, update documentation, and run build/tests.*

## 제외 범위 / Out of Scope

- blend, alpha/color-key, texture filtering, RGB565 target, Z-buffer 정책 변경
- 원본 게임 draw 순서나 정점 데이터 변경
- cull state 외의 rasterizer state 재구현

*Do not change blending, alpha/color-key handling, texture filtering, the RGB565 target, Z-buffer policy, original draw order, vertex data, or other rasterizer states.*

## 완료 기준 / Acceptance Criteria

- `D3DCULL_NONE`, `D3DCULL_CW`, `D3DCULL_CCW`가 명시적으로 처리됩니다.
- 알 수 없는 cull 값은 draw 실패로 보고됩니다.
- Win32 build, unit tests, CTest가 통과합니다.
- 동일 Music Select 실행으로 시각적 비교가 가능합니다.
