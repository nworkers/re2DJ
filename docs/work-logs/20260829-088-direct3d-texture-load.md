# Direct3D 3 texture Load 복사 작업 로그

관련 설계: [Direct3D 3 texture Load 복사](../design/20260829-088-direct3d-texture-load.md)  
작업 지시: [Direct3D 3 texture Load 복사](../work-orders/20260829-088-direct3d-texture-load.md)

## 결과

- 사용자 화면에서 Music Select 원형 프레임 내부가 검은색인 현상을 확인했다.
- 최신 VFS 로그 `20260829-013719-626.vfs.log`에서 `_3week.bmp`를 포함한 곡 그림은 정상 로드됐다.
- Windows COM facade의 `IDirect3DTexture2::Load`가 vtable에는 연결됐지만 무조건 `DDERR_UNSUPPORTED`를 반환하는 구현 결손을 확인했다.
- 같은 DirectDraw root와 크기를 가진 RGB565 texture 사이에서 pixel row, `DDCKEY_SRCBLT` 상태·범위를 복사하고 destination revision을 증가시키도록 구현했다.
- null source는 `DDERR_INVALIDPARAMS`, 자기 복사는 `DD_OK`, 크기 불일치는 `D3DERR_TEXTURE_LOAD_FAILED`, busy surface는 `DDERR_SURFACEBUSY`로 처리한다.
- bounded `TextureLoad` 진단에 destination/source ID, revision과 HRESULT를 남긴다.
- Windows facade probe가 RGB565 복사, color-key 전달과 실패 계약을 검증한다.

## 검증

- 표준 `build/windows-x86` Debug warnings-as-errors 빌드는 컴파일까지 통과했으나, 사용자 실행의 `ez2dj.exe`/`re2dj.exe`가 Debug injected-runtime DLL을 잡고 있어 `LNK1168`로 링크하지 못했다. 뒤의 Debug CTest는 기존 바이너리를 사용했으므로 검증 근거에서 제외했다.
- 같은 표준 빌드 디렉터리의 Release 구성을 새로 링크했고 warnings-as-errors build가 성공했다.
- Release CTest 3/3 통과: `re2dj_windows_vfs_runtime_probe`, `re2dj_windows_product_loader_probe`, `re2dj_unit_tests`.
- 원본 HDD, 실행 파일과 로그는 수정하거나 커밋하지 않았다.

## 남은 확인

후속 사용자 재검증에서 화면 변화가 없었고 `20260829-015640-892.ddraw.log`의 `TextureLoad` 호출은 0회였다. 이 구현 결손은 일반 호환 경계로 남지만 해당 장면의 직접 원인 추정은 기각됐다. 같은 로그에서 반복 실패한 변환 전 FVF `0x112`와 `0x1e2`는 작업 089로 분리했다.

---

# Direct3D 3 Texture Load Copy Work Log

## Result

- Confirmed the user's black center inside the Music Select circular frame.
- The latest VFS log, `20260829-013719-626.vfs.log`, successfully loads song artwork including `_3week.bmp`.
- Confirmed that the Windows COM facade connects `IDirect3DTexture2::Load` in its vtable but unconditionally returns `DDERR_UNSUPPORTED`.
- Implemented pixel-row and `DDCKEY_SRCBLT` state/range copying between equal-sized RGB565 textures owned by the same DirectDraw root, followed by a destination revision increment.
- Null source returns `DDERR_INVALIDPARAMS`, self-copy returns `DD_OK`, size mismatch returns `D3DERR_TEXTURE_LOAD_FAILED`, and busy surfaces return `DDERR_SURFACEBUSY`.
- A bounded `TextureLoad` diagnostic records destination/source IDs, revision, and HRESULT.
- The Windows facade probe verifies RGB565 copying, color-key propagation, and failure contracts.

## Verification

- The standard `build/windows-x86` Debug warnings-as-errors build compiled the changed source but could not link because the user's running `ez2dj.exe`/`re2dj.exe` held the Debug injected-runtime DLL, producing `LNK1168`. The following Debug CTest used older binaries and is excluded from evidence.
- A fresh Release configuration linked successfully in the same standard build directory with warnings treated as errors.
- Release CTest passes all three tests: `re2dj_windows_vfs_runtime_probe`, `re2dj_windows_product_loader_probe`, and `re2dj_unit_tests`.
- No original HDD content, executable, or runtime log was modified or committed.

## Remaining validation

Later user revalidation showed no visual change, and `20260829-015640-892.ddraw.log` contains zero `TextureLoad` calls. This implementation remains a general compatibility boundary, but its inferred direct relationship to the scene defect is rejected. The repeatedly failing untransformed FVFs `0x112` and `0x1e2` moved to Task 089.
