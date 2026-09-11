# 작업 로그: ez2d2m VFS CHD 루트 수정
# Work Log: Fix the ez2d2m VFS CHD Root

## 한국어

### 변경 내용

- `src/platform/windows/injected_runtime.cpp`에 프로필별 CHD 내부 루트를 조립하는
  `ChdPathFromRelative`를 추가했습니다.
- `ChdRelativePath`, `GuestDirectoryExists`, `Re2djVfsFindFirstFileA`가 모두 해당 helper를
  사용하도록 변경했습니다.
- 기존 프로필의 기본값인 `EZ2DJ`는 유지됩니다.
- 설계 문서와 작업 지시서에 범위 및 검증 결과를 기록했습니다.

### 검증

- `cmd /c scripts\\build_win32.bat`: 성공
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: 성공
- `re2dj_ez2dj_keyboard_input_test`: 성공
- `re2dj_windows_product_loader_probe`: 성공
- `re2dj_unit_tests`: 성공
- 전체 CTest는 `re2dj_windows_vfs_runtime_probe`의 창/오디오 자식 단계가 완료되지 않아
  중단했습니다.

새 `ez2d2m` 실행 로그([20260912-033024-415.vfs.log](../../logs/windows_x86_launcher_probe/ez2d2m/20260912-033024-415.vfs.log))에서:

- `system\\opening` 현재 디렉터리 설정: `success=1`
- `system\\title` 현재 디렉터리 설정: `success=1`
- `1P_Press.str`: `chd://ez2dancer/system/title/1P_Press.str`, `success=1`
- 종료 이벤트: 검증 중에는 없음
- Hardlock 보호 코드의 기존 `0x00439f1b` access violation: 1회 기록되지만 실행은 계속됨

### 결론

이번 수정으로 `ez2d2m`의 즉시 종료를 유발하던 프로필별 CHD 디렉터리 경로 오류를
해결했습니다. Hardlock `Function 0x0011` 응답은 여전히 별도 미확정 사항입니다.

## English

### Changes

- Added `ChdPathFromRelative` to `src/platform/windows/injected_runtime.cpp` for the
  profile-selected image-internal CHD root.
- Changed `ChdRelativePath`, `GuestDirectoryExists`, and `Re2djVfsFindFirstFileA` to use the
  same helper.
- Preserved the existing `EZ2DJ` default for existing profiles.
- Recorded the scope and verification results in the design and work-order documents.

### Verification

- `cmd /c scripts\\build_win32.bat`: passed
- `re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: passed
- `re2dj_ez2dj_keyboard_input_test`: passed
- `re2dj_windows_product_loader_probe`: passed
- `re2dj_unit_tests`: passed
- The full CTest run was interrupted because the window/audio child phase of
  `re2dj_windows_vfs_runtime_probe` did not complete.

The new `ez2d2m` run log shows:

- `system\\opening` current-directory change: `success=1`
- `system\\title` current-directory change: `success=1`
- `1P_Press.str`: `chd://ez2dancer/system/title/1P_Press.str`, `success=1`
- No exit event during verification
- The existing Hardlock-protected-code access violation at `0x00439f1b` occurs once, but the
  run continues.

### Conclusion

This fixes the profile-specific CHD directory-path error that caused `ez2d2m` to exit
immediately. The Hardlock `Function 0x0011` response remains a separate unresolved issue.
