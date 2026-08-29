# Direct3D 3 texture Load 복사 작업 지시

관련 설계: [Direct3D 3 texture Load 복사](../design/20260829-088-direct3d-texture-load.md)

## 상태

**구현·자동 검증 완료, 사용자 화면 재검증 대기.** Music Select 곡 BMP는 VFS에서 정상적으로 열렸고 `IDirect3DTexture2::Load` 복사 경계를 구현했다.

## 작업

1. RGB565 texture surface 전체 복사와 source color key 전달을 구현한다.
2. 성공한 destination의 content revision을 증가시킨다.
3. 유효하지 않은 facade, null source, 자기 복사와 크기 불일치의 HRESULT 계약을 고정한다.
4. bounded `TextureLoad` 진단을 추가한다.
5. Windows facade probe에 성공·실패 계약 검증을 추가한다.
6. Windows x86 build와 CTest를 실행한다.
7. architecture, analysis, TODO/IMPLEMENTED와 작업 로그를 실제 결과에 맞게 갱신하고 커밋한다.
8. 사용자가 Music Select 중앙 그림을 다시 확인한다.

---

# Direct3D 3 Texture Load Copy Work Order

Related design: [Direct3D 3 Texture Load Copy](../design/20260829-088-direct3d-texture-load.md)

## Status

**Implementation and automated verification complete; user-visible revalidation pending.** Music Select song BMPs open successfully through the VFS, and the `IDirect3DTexture2::Load` copy boundary is now implemented.

## Work

1. Implement a full RGB565 texture-surface copy and source color-key propagation.
2. Increment the destination content revision after success.
3. Pin HRESULT behavior for invalid facades, null source, self-copy, and mismatched sizes.
4. Add a bounded `TextureLoad` diagnostic.
5. Extend the Windows facade probe with success and failure contract checks.
6. Run the Windows x86 build and CTest.
7. Update architecture, analysis, TODO/IMPLEMENTED, and the work log to match results, then commit.
8. Ask the user to revalidate the Music Select center artwork.
