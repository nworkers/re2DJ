# 작업 지시서: Windows x86 릴리즈 빌드 스크립트

## 한국어

설계: [릴리즈 빌드 스크립트 설계](../design/20260906-215-release-build-script.md)

### 목표

Windows x86 Release 빌드와 Release 테스트를 반복 실행할 수 있는 스크립트를 추가합니다.

### 작업 항목

1. `scripts/build_release.ps1`을 추가합니다.
2. `scripts/build_release.bat` wrapper를 추가합니다.
3. `scripts/README.md`에 두 스크립트의 사용법과 Release 출력 경로를 기록합니다.
4. Release build와 CTest, wrapper의 `-SkipTests` 경로를 검증합니다.
5. 작업 로그를 남기고 현재 작업 브랜치의 변경을 커밋합니다.
6. patch 버전을 증가시킨 뒤 현재 브랜치의 커밋을 하나로 squash하여 `main`에 머지하고 annotated tag를 만듭니다.

### 제외 범위

- CMake preset의 generator나 binary directory 재구성
- Linux/Web 릴리즈 파이프라인 추가
- 원본 HDD/CHD 자산의 복사 또는 패키징
- 사용자 변경 파일 `config/ez2dj-io.example.ini`의 수정·커밋

### 완료 조건

- Release 스크립트가 configure, build, test를 실패 시 즉시 중단하고 원래 exit code를 반환합니다.
- `-SkipTests`가 build를 수행한 뒤 테스트만 생략합니다.
- Release CTest가 통과합니다.
- 관련 문서와 작업 로그가 추가됩니다.

## English

Design: [Release build script design](../design/20260906-215-release-build-script.md)

### Objective

Add scripts that repeat the Windows x86 Release build and Release test flow.

### Work items

1. Add `scripts/build_release.ps1`.
2. Add the `scripts/build_release.bat` wrapper.
3. Document both scripts and the Release output path in `scripts/README.md`.
4. Verify the Release build, CTest, and the wrapper's `-SkipTests` path.
5. Leave a work log and commit the current task branch changes.
6. Increment the patch version, squash the current branch commits into `main`, and create the annotated tag.

### Out of scope

- Reworking the CMake generator or binary-directory layout
- Adding Linux/Web release pipelines
- Copying or packaging original HDD/CHD assets
- Modifying or committing `config/ez2dj-io.example.ini`

### Completion criteria

- The Release script stops on configure, build, or test failure and returns the native exit code.
- `-SkipTests` still builds and skips only tests.
- Release CTest passes.
- Related design, work-order, and work-log documents exist.
