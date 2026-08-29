# 변환 전 Direct3D 3 정점 draw 작업 로그

관련 설계: [변환 전 Direct3D 3 정점 draw](../design/20260829-089-untransformed-direct3d-draw.md)  
작업 지시: [변환 전 Direct3D 3 정점 draw](../work-orders/20260829-089-untransformed-direct3d-draw.md)

## 결과

- 작업 088 수정본의 사용자 재검증에서 화면 변화가 없음을 확인했다.
- 최신 `20260829-015640-892.ddraw.log`에서 `TextureLoad` 호출은 0회였으므로 작업 088의 장면 직접 원인 추정을 기각했다.
- 같은 로그에서 `DrawPrimitive`가 `FVF 0x112` 14회, `FVF 0x1e2` 50회를 `0x80004001`로 거절한 실제 미구현 경계를 확인했다.
- 플랫폼 중립 `legacy_transform` 모듈을 추가해 Direct3D row-vector world → view → projection과 `D3DVIEWPORT2` mapping을 기존 XYZRHW draw 명령에 연결했다.
- 32바이트 `D3DVERTEX(0x112)`의 XYZ/normal/UV와 `D3DLVERTEX(0x1e2)`의 XYZ/reserved/diffuse/specular/UV layout을 지원했다.
- `D3DFVF_RESERVED1`을 정점 stride 계산에 반영해 `0x1e2`를 정확히 32바이트로 계산한다.
- Windows COM facade는 생성 시 matrix 세 개를 identity로 초기화하고 현재 transform/viewport snapshot만 공용 decoder에 전달한다.
- Windows VFS runtime probe가 실제 COM vtable을 통해 `D3DFVF_VERTEX`와 `D3DFVF_LVERTEX` triangle strip 성공을 검증한다.
- 기존 `D3DFVF_TLVERTEX(0x1c4)` 경로와 renderer backend 계약은 변경하지 않았다.

## 검증

- `cmake --build build\windows-x86 --config Release`: 통과
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 통과
- `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`: Debug 제품 build와 CTest 3/3 통과
- 단위 테스트: identity viewport mapping, 순차 matrix 적용, 두 FVF field layout, non-finite/zero-w/invalid viewport 거절을 확인했다.
- 원본 HDD 자산과 실행 로그는 읽기만 했고 저장소에 추가하지 않았다.

## 남은 확인

사용자가 갱신된 Debug 제품 빌드에서 Music Select 중앙 곡 그림이 표시됨을 확인했다. 따라서 `FVF 0x112`/`0x1e2` 미지원과 해당 결손 화면의 관계는 확인됨으로 승격한다. `0x112` normal의 lighting 필요 여부는 다른 실제 화면 또는 원본 render-state 증거가 확보될 때 결정한다.

---

# Untransformed Direct3D 3 Vertex Draw Work Log

Related design: [Untransformed Direct3D 3 Vertex Draw](../design/20260829-089-untransformed-direct3d-draw.md)  
Work order: [Untransformed Direct3D 3 Vertex Draw](../work-orders/20260829-089-untransformed-direct3d-draw.md)

## Result

- Confirmed no visible change in the user's revalidation of the Task 088 build.
- Latest log `20260829-015640-892.ddraw.log` contains zero `TextureLoad` calls, rejecting Task 088's scene-specific direct-cause hypothesis.
- The same log identifies the actual unsupported boundary: fourteen FVF `0x112` and fifty FVF `0x1e2` `DrawPrimitive` calls returning `0x80004001`.
- Added a platform-neutral `legacy_transform` module that applies Direct3D row-vector world, view, and projection transforms plus `D3DVIEWPORT2` mapping into the existing XYZRHW draw command.
- Supported the 32-byte `D3DVERTEX(0x112)` XYZ/normal/UV and `D3DLVERTEX(0x1e2)` XYZ/reserved/diffuse/specular/UV layouts.
- Included `D3DFVF_RESERVED1` in vertex-stride calculation so `0x1e2` resolves to the correct 32 bytes.
- The Windows COM facade initializes all three matrices to identity and passes only the current transform/viewport snapshot to the common decoder.
- The Windows VFS runtime probe verifies successful `D3DFVF_VERTEX` and `D3DFVF_LVERTEX` triangle strips through the actual COM vtable.
- The existing `D3DFVF_TLVERTEX(0x1c4)` path and renderer-backend contract remain unchanged.

## Verification

- `cmake --build build\windows-x86 --config Release`: passed
- `ctest --test-dir build\windows-x86 -C Release --output-on-failure`: 3/3 passed
- `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`: Debug product build and CTest 3/3 passed
- Unit coverage verifies identity viewport mapping, sequential matrices, both FVF field layouts, and rejection of non-finite, zero-w, and invalid-viewport input.
- Original HDD assets and generated runtime logs were read only and were not added to the repository.

## Remaining validation

The user confirmed that the Music Select center song artwork appears in the refreshed Debug product build. The relationship between unsupported FVF `0x112`/`0x1e2` draws and that missing view is therefore promoted to confirmed. Whether `0x112` normals need lighting remains unresolved until another runtime view or original render-state evidence requires it.
