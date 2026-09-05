# 작업 지시 195: RGB565 논리 렌더 대상

# Work Order 195: RGB565 Logical Render Target

## 목적 / Purpose

Music Select의 source-color-keyed additive layer를 원본이 요청한 640×480 RGB565 render target에서 합성하도록 SDL3/OpenGL backend를 변경합니다. host window pixel framebuffer에 직접 그리는 현재 경로를 제거합니다.

*Change the SDL3/OpenGL backend so Music Select source-color-keyed additive layers compose in the requested 640×480 RGB565 render target, replacing the current direct draw into the host-window pixel framebuffer.*

## 선행 근거 / Evidence

- 사용자 확인 Music Select 실행 `20260905-112440-703`의 중앙 artwork(`texture=387`)와 selection ring(`texture=279`)은 RGB565 texture, linear filter, color key, additive blend를 요청했습니다.
- 같은 실행은 `DITHERENABLE=1`을 기록했습니다.
- depth test와 depth write는 해당 화면의 모든 기록 draw에서 꺼져 있어 Z 처리는 주원인에서 제외했습니다.
- RGB texture stage는 facade가 `MODULATE(TEXTURE, DIFFUSE)` 이외에 명시적 오류를 반환하므로, 이 draw들이 성공한 사실로 해당 RGB stage 상태도 확인됩니다.

*The same points are evidence, not a claim that RGB565 target precision is already proven as the root cause.*

## 작업 범위 / Scope

1. `Sdl3OpenGlBackend`에 RGB565 framebuffer/depth16 attachment lifecycle을 추가합니다.
2. guest draw/clear는 logical framebuffer를 대상으로 고정합니다.
3. present 시 default framebuffer로 nearest full-screen copy합니다.
4. 초기화 실패를 명시적으로 전파하고 resource cleanup을 추가합니다.
5. `ARCHITECTURE.md`, 분석 문서, 작업 로그를 갱신합니다.

## 제외 범위 / Out Of Scope

- 원본 자산의 저장·추출·변경
- gamma-correct blending, fog, multisample, 새로운 Direct3D state 지원
- 게임 로직 또는 draw 순서의 재구현

## 완료 기준 / Acceptance Criteria

- Win32 build, unit test, CTest가 통과합니다.
- 640×480 RGB565 + depth16 framebuffer가 완전하지 않으면 backend initialization이 실패합니다.
- host resize가 guest draw target resolution을 바꾸지 않습니다.
- 사용자 Music Select 재검증 절차와 결과를 작업 로그에 남깁니다.
