# 변환 전 Direct3D 3 정점 draw 작업 지시

관련 설계: [변환 전 Direct3D 3 정점 draw](../design/20260829-089-untransformed-direct3d-draw.md)

## 상태

**구현·자동 검증 완료, 사용자 화면 재검증 대기.** 수정본 실제 로그에서 `TextureLoad`는 호출되지 않았고 `FVF 0x112`와 `0x1e2` draw가 반복적으로 거절된다.

## 작업

1. 플랫폼 중립 matrix/viewport transform과 untransformed FVF decoder를 추가한다.
2. `D3DVERTEX(0x112)`와 `D3DLVERTEX(0x1e2)`의 32바이트 layout을 지원한다.
3. Windows COM facade가 저장한 world/view/projection/viewport를 decoder에 전달한다.
4. 단위 테스트와 Windows facade probe를 보강한다.
5. Windows x86 build와 CTest를 실행한다.
6. 작업 088의 기각된 직접 원인 추정과 작업 089 결과를 architecture, analysis, TODO/IMPLEMENTED에 반영한다.
7. 작업 로그와 커밋을 남기고 사용자 화면 재검증을 요청한다.

---

# Untransformed Direct3D 3 Vertex Draw Work Order

Related design: [Untransformed Direct3D 3 Vertex Draw](../design/20260829-089-untransformed-direct3d-draw.md)

## Status

**Implementation and automated verification complete; user-visible revalidation pending.** The corrected runtime makes no `TextureLoad` calls while repeatedly rejecting FVF `0x112` and `0x1e2` draws.

## Work

1. Add platform-neutral matrix/viewport transformation and untransformed FVF decoding.
2. Support the 32-byte `D3DVERTEX(0x112)` and `D3DLVERTEX(0x1e2)` layouts.
3. Pass retained world/view/projection/viewport state from the Windows COM facade.
4. Extend unit and Windows facade probes.
5. Run the Windows x86 build and CTest.
6. Update architecture, analysis, TODO/IMPLEMENTED with the rejected Task 088 hypothesis and Task 089 result.
7. Leave a work log and commit, then request user-visible revalidation.
