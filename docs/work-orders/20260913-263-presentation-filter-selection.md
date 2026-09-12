# 정수 배율 화면 필터 선택 작업 지시서

## 목표

최종 OpenGL presentation 단계에서 정수 배율은 nearest, 비정수 배율은 linear로 자동 선택해 창 크기별 화면 품질을 개선한다.

## 범위

- 순수 배율 판정 helper와 단위 테스트 추가
- SDL3/OpenGL backend의 최종 FBO sampler 선택 연결
- CMake 소스 및 단위 테스트 목록 갱신
- 설계·작업 로그 문서 작성

다음은 범위에서 제외한다.

- 개별 Direct3D 텍스처 필터 변경
- 내부 렌더 타깃을 RGB565에서 RGBA8888로 변경
- 고급 업스케일러 또는 셰이더 추가
- 원본 실행 파일 및 게임 자산 수정

## 구현 순서

1. 현재 branch와 working tree를 확인하고 작업 branch를 생성한다.
2. 배율 판정 정책을 문서화한다.
3. `PresentationFilter` helper와 테스트를 추가한다.
4. `Present`에서 aspect-fit viewport를 계산한 뒤 helper 결과를 최종 FBO texture filter에 적용한다.
5. Debug/Release 빌드와 단위·기존 CTest를 실행한다.
6. 작업 로그와 검증 결과를 기록하고 커밋한다.

## 수용 기준

- 640×480, 1280×960, 1920×1440은 `NEAREST`를 선택한다.
- 1280×720, 1920×1080 등 비정수 배율은 `LINEAR`를 선택한다.
- 화면 aspect ratio 유지와 기존 개별 텍스처 필터 동작은 변하지 않는다.
- 관련 단위 테스트와 빌드가 통과한다.

## English

# Work Order: Integer-Scale Presentation Filter Selection

## Goal

Improve window-size image quality by selecting nearest sampling automatically for integer presentation scales and linear sampling for non-integer scales at the final OpenGL presentation stage.

## Scope

- Add and unit-test a pure scale-selection helper.
- Connect the helper to the SDL3/OpenGL final FBO sampler.
- Update the CMake source and unit-test lists.
- Record the design and work log.

The following are out of scope: changing individual Direct3D texture filters, replacing the RGB565 render target with RGBA8888, adding an advanced upscaler or shader, and modifying original executables or game assets.

## Acceptance criteria

- 640×480, 1280×960, and 1920×1440 select `NEAREST`.
- Non-integer scales such as 1280×720 and 1920×1080 select `LINEAR`.
- Aspect-ratio preservation and existing per-texture filtering remain unchanged.
- Relevant unit tests and builds pass.
