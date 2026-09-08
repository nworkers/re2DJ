# 작업 지시서: VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리

## 한국어

### 관련 설계

[VFS 게스트 루트 접두사와 CHD 열거 현재 디렉터리 설계](../design/20260908-225-vfs-guest-root-and-chd-enumeration.md)

### 작업 목표

복호화된 1st SE 게스트가 실패한 작업 디렉터리 호출 두 건의 원인을 고칩니다. 게스트 루트 접두사를 프로파일에서 받고, CHD 열거가 게스트 현재 디렉터리를 유지하게 합니다.

### 작업 항목

1. 주입 런타임에 `g_re2dj_vfs_guest_root` export를 추가하고 `StripGuestRoot`가 그 값을 보게 합니다.
2. launcher 주 프로세스 주입 경로에서 프로파일의 `guest_drive_letter`·`guest_directory`로 그 값을 채웁니다. 프로파일에 없으면 `D:\ez2dj`를 씁니다.
3. bootstrap child 주입 경로에도 같은 값을 씁니다.
4. `Re2djVfsFindFirstFileA`의 CHD 분기가 패턴을 먼저 분리하고 디렉터리만 해석하게 합니다.
5. `re2dj_windows_vfs_runtime_probe`에 게스트 루트 접두사 시험을 더합니다.
6. Windows x86 build, unit test, VFS runtime probe, product loader probe를 검증합니다.
7. `re2dj ez2dj1stse` 실제 실행으로 두 결함이 사라졌는지 확인하고 새 도달 경계를 기록합니다.
8. 3rd·4th 실행으로 회귀가 없는지 확인합니다.
9. analysis, ARCHITECTURE, work log를 갱신합니다.

### 제외 범위

- 상대 경로 의미 변경
- `ChdRelativePath`·`GuestDirectoryExists`의 `"EZ2DJ"` 고정 이름 프로파일화
- graphics HLE 경로 신설
- Hardlock 정책 변경

### 완료 조건

- `set:request=c:\ez2dj`가 `success=1`이 됩니다.
- `find-first`의 `chd_dir`이 현재 디렉터리를 반영합니다.
- VFS runtime probe가 기본값과 설정값 양쪽에서 통과합니다.
- Windows x86 build와 기존 시험이 통과하고 3rd·4th 실행에 회귀가 없습니다.

## English

### Related design

[VFS Guest Root Prefix and CHD Enumeration Working Directory Design](../design/20260908-225-vfs-guest-root-and-chd-enumeration.md)

### Objective

Fix the cause of the two working-directory calls the decrypted 1st SE guest failed: take the guest root prefix from the profile, and keep the guest current directory through CHD enumeration.

### Work items

1. Add the `g_re2dj_vfs_guest_root` export to the injected runtime and make `StripGuestRoot` consult it.
2. Fill it from the profile's `guest_drive_letter` and `guest_directory` on the launcher's main-process injection path, falling back to `D:\ez2dj` when the profile carries none.
3. Write the same value on the bootstrap-child injection path.
4. Make the CHD branch of `Re2djVfsFindFirstFileA` split the pattern first and resolve only the directory.
5. Add a guest-root-prefix test to `re2dj_windows_vfs_runtime_probe`.
6. Verify the Windows x86 build, unit tests, the VFS runtime probe, and the product-loader probe.
7. Confirm with a real `re2dj ez2dj1stse` run that both defects are gone and record the new reached boundary.
8. Confirm no regression with 3rd and 4th runs.
9. Update the analysis document, `ARCHITECTURE.md`, and the work log.

### Out of scope

- Changing relative-path semantics
- Deriving the `"EZ2DJ"` literal in `ChdRelativePath` and `GuestDirectoryExists` from the profile
- Adding a graphics HLE path
- Changing Hardlock policy

### Completion criteria

- `set:request=c:\ez2dj` reports `success=1`.
- The `find-first` line's `chd_dir` reflects the current directory.
- The VFS runtime probe passes for both the default and a configured guest root.
- The Windows x86 build and existing tests pass with no 3rd or 4th regression.
