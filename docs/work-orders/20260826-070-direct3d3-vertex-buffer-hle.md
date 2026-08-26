# Direct3D 3 정점 버퍼 HLE 작업 지시

관련 설계: [Direct3D 3 정점 버퍼 HLE](../design/20260826-070-direct3d3-vertex-buffer-hle.md)

## 상태

**완료.** 중립 코어, COM facade, nullable-size Lock 회귀 probe와 단위 테스트를 구현했다. warnings-as-errors x86/x64 빌드와 CTest, canonical 두 실행이 모두 통과했으며 다음 경계는 `KSnd(ksndDuplicate) : Error on duplicate`다([작업 로그](../work-logs/20260826-070-direct3d3-vertex-buffer-hle.md) 참조).

## 작업

1. 플랫폼 공용 `LegacyVertexBuffer`(descriptor, FVF stride, 전체 buffer lock)와 단위 테스트를 추가한다.
2. `direct3d3_com_facade.cpp`에 `D3dCreateVertexBuffer`와 완전한 `IDirect3DVertexBuffer` vtable facade를 구현하고 관찰 marker를 연결한다.
3. `windows_vfs_runtime_probe`에 Direct3D3 → CreateVertexBuffer → Lock/Unlock/GetVertexBufferDesc 흐름 검증을 추가한다.
4. `-DRE2DJ_WARNINGS_AS_ERRORS=ON` windows-x86/windows-x64 build와 CTest를 통과시킨다.
5. `VbLock` 진입 marker에 self/vtable/data/size 포인터를 기록하고, `lpdwSize == nullptr`을 허용한다. probe에 동일 호출 형태의 회귀 검증을 추가한다.
6. canonical 실행 두 번으로 `0x00420353` AV 소멸, Lock 출력, 다음 경계를 확인한다.
7. ARCHITECTURE, TODO, analysis, IMPLEMENTED, 작업 로그를 결과에 맞춰 갱신한다.

---

# Direct3D 3 Vertex Buffer HLE Work Order

Related design: [Direct3D 3 Vertex Buffer HLE](../design/20260826-070-direct3d3-vertex-buffer-hle.md)

**Complete.** The neutral core, COM facade, nullable-size Lock regression probe, and unit tests are implemented. Warnings-as-errors x86/x64 builds, CTest, and two canonical runs pass; the next boundary is `KSnd(ksndDuplicate) : Error on duplicate` (see the work log).

## Tasks

1. Add the platform-neutral LegacyVertexBuffer (descriptor, FVF stride, whole-buffer lock) with unit tests.
2. Implement D3dCreateVertexBuffer and a complete IDirect3DVertexBuffer vtable facade in direct3d3_com_facade.cpp with observation markers.
3. Extend windows_vfs_runtime_probe with a Direct3D3 → CreateVertexBuffer → Lock/Unlock/GetVertexBufferDesc flow.
4. Pass warnings-as-errors Windows x86/x64 builds and CTest.
5. Record self/vtable/data/size pointers at VbLock entry, accept `lpdwSize == nullptr`, and add a matching probe regression.
6. Confirm through two canonical runs that the 0x00420353 AV disappears; record Lock output and the next boundary.
7. Update ARCHITECTURE, TODO, analysis, IMPLEMENTED, and the work log to match the results.
