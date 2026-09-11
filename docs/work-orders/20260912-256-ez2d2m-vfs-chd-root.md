# 작업 지시서: ez2d2m VFS CHD 루트 수정
# Work Order: Fix the ez2d2m VFS CHD Root

## 한국어

### 범위

설계 문서 [20260912-256-ez2d2m VFS CHD 루트 일반화](../design/20260912-256-ez2d2m-vfs-chd-root.md)에
따라, CHD-backed VFS의 디렉터리 확인과 파일 열거가 프로필별 CHD 내부 루트를 사용하도록
수정합니다.

### 작업 항목

- [x] `src/platform/windows/injected_runtime.cpp`에서 CHD 내부 루트 조립을 공통화합니다.
- [x] `GuestDirectoryExists`의 `EZ2DJ/` 하드코딩을 제거합니다.
- [x] `Re2djVfsFindFirstFileA`의 `EZ2DJ/` 하드코딩을 제거합니다.
- [x] 기존 Windows VFS 런타임 프로브의 enumeration-only 경로를 실행합니다.
- [x] Windows x86 Debug 빌드를 실행하고 독립 CTest 3개를 통과시킵니다.
- [x] `ez2d2m` CHD를 다시 실행하고 경로 관련 VFS 로그를 확인합니다.
- [x] 작업 로그와 `docs/analysis/ez2d2m-chd-filesystem.md`를 갱신합니다.

전체 CTest의 `re2dj_windows_vfs_runtime_probe`는 창/오디오 자식 단계에서 완료되지 않아
중단했습니다. 동일 바이너리의 `--vfs-enumeration-only` 경로와 나머지 세 CTest는 성공했습니다.

### 비범위

- Hardlock `Function 0x0011`의 정답 응답 생성
- EZ2Dancer 전용 입력 설정 및 F1 동작
- 원본 CHD 또는 실행 파일의 저장소 추가

## English

### Scope

Following the [20260912-256 VFS CHD-root design](../design/20260912-256-ez2d2m-vfs-chd-root.md),
change the CHD-backed VFS directory checks and file enumeration to use the profile-selected
image-internal root.

### Tasks

- [x] Commonize CHD-internal root construction in `src/platform/windows/injected_runtime.cpp`.
- [x] Remove the `EZ2DJ/` hard-code from `GuestDirectoryExists`.
- [x] Remove the `EZ2DJ/` hard-code from `Re2djVfsFindFirstFileA`.
- [x] Run the enumeration-only path of the existing Windows VFS runtime probe.
- [x] Build the Windows x86 Debug configuration and pass three independent CTest tests.
- [x] Re-run the `ez2d2m` CHD and inspect the path-related VFS events.
- [x] Update the work log and `docs/analysis/ez2d2m-chd-filesystem.md`.

The full CTest run was interrupted because `re2dj_windows_vfs_runtime_probe` did not complete
in its window/audio child-process phase. Its `--vfs-enumeration-only` path and the other three
CTest tests passed.

### Out of scope

- Producing a valid Hardlock `Function 0x0011` response
- EZ2Dancer-specific input configuration and F1 behavior
- Adding the original CHD or executable to the repository
