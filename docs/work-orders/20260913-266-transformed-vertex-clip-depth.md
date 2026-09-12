# 변환 정점 OpenGL 클립 깊이 보정 작업 지시

관련 설계: [변환 정점의 OpenGL 클립 깊이 보정 설계](../design/20260913-266-transformed-vertex-clip-depth.md)

## 목표

`ez2dj4th` 실행 중 note와 롱노트가 데이터 로딩 성공 및 `DrawPrimitive` 성공 기록에도 화면에 나타나지 않는 증상을 수정한다. 원본 실행 파일의 게임플레이 로직과 VFS를 변경하지 않고, depth test가 꺼진 transformed vertex의 유효하지 않은 `z`가 OpenGL clip volume에서 primitive를 제거하지 않도록 한다.

## 작업 범위

1. 공용 legacy graphics 계층에 transformed vertex clip-depth 선택 정책을 추가한다.
2. SDL3/OpenGL backend가 depth state에 따라 guest `z` 또는 안전한 in-range 깊이를 사용하도록 연결한다.
3. depth-enabled 보존과 depth-disabled 보정을 단위 테스트한다.
4. 관련 architecture/analysis 문서에 최신 로그의 확인 결과와 수정 내용을 반영한다.
5. Windows x86 warnings-as-errors build와 CTest를 실행하고 작업 로그에 결과를 남긴다.

## 제외 범위

- Note, LONG_EFFECT, CHD, FAT32 VFS 읽기 로직 변경
- 원본 EXE 또는 profile 데이터 변경
- texture filter, blend, color key, window mode 변경
- depth-enabled 장면의 guest `z` 의미 변경

## 완료 조건

- `0xCCCCCCCC`와 같은 guest `z`가 depth-disabled transformed draw의 OpenGL clip 계산에 직접 사용되지 않는다.
- depth-enabled transformed draw는 입력 `z`를 계속 전달한다.
- 단위 테스트와 Windows x86 build/CTest가 통과한다.
- 설계, 작업 지시, 작업 로그와 누적 analysis/architecture 문서가 서로 같은 확인 상태를 기록한다.

---

# Transformed-Vertex OpenGL Clip-Depth Correction Work Order

Related design: [Transformed-Vertex OpenGL Clip-Depth Correction Design](../design/20260913-266-transformed-vertex-clip-depth.md)

## Goal

Fix the `ez2dj4th` symptom in which notes and long notes are absent even though their data loads and `DrawPrimitive` reports success. Preserve the original executable's gameplay logic and the VFS, and prevent an invalid `z` from depth-disabled transformed vertices from removing primitives during OpenGL clip-volume testing.

## Scope

1. Add a transformed-vertex clip-depth policy to the shared legacy graphics layer.
2. Connect the SDL3/OpenGL backend so it uses guest `z` when depth testing is enabled and a safe in-range depth otherwise.
3. Unit-test both depth-enabled preservation and depth-disabled correction.
4. Update the architecture and analysis documents with the latest log evidence and the fix.
5. Run the Windows x86 warnings-as-errors build and CTest and record the result.

## Out of scope

- Note, LONG_EFFECT, CHD, FAT32 VFS read logic
- Original EXE or profile data
- Texture filtering, blending, color keying, or window-mode behavior
- Changing guest `z` semantics in depth-enabled scenes

## Completion criteria

- A guest `z` such as `0xCCCCCCCC` is not used directly for OpenGL clip calculation in depth-disabled transformed draws.
- Depth-enabled transformed draws continue to pass the input `z`.
- Unit tests and the Windows x86 build/CTest pass.
- The design, work order, work log, cumulative analysis, and architecture documents report the same evidence status.
