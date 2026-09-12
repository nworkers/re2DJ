# 작업 로그: GitHub Actions 릴리즈 CI 구성

## 결과

공개된 `nworkers/rePIU`의 `ci.yml`/`release.yml`을 기준으로 `re2DJ`에 Windows x86 Release package와 GitHub Release 자동 생성을 추가했다. `v*` tag push는 version gate, Release build, 검증, zip/checksum 생성, artifact 업로드, Release 생성까지 수행한다. 수동 workflow 실행은 Release를 만들지 않고 artifact만 남긴다.

## 구현

- `.github/workflows/release.yml` 추가
  - `v*` tag push 및 `workflow_dispatch` 지원
  - `VERSION`과 tag의 `v` 제거 값 비교
  - Windows-2022 Release build와 dependency cache
  - 장치/오디오 lifecycle 대기 probe를 제외한 Release CTest와 VFS enumeration probe 실행
  - `contents: write`와 `gh release create/edit/upload` 사용
  - 기존 Release asset `--clobber` 갱신
  - `docs/release-notes/<tag>.md` 우선, 없으면 `--generate-notes` fallback
- `scripts/package_release.ps1`/`.bat` 추가
  - `re2dj.exe`, `re2dj_windows_injected_runtime.dll`, `config/`, README/LICENSE/VERSION/RELEASE_NOTES만 package
  - `re2dj-v<version>-windows-x86.zip`와 SHA256 sidecar 생성
  - 원본 HDD/CHD와 게임 실행 자산은 포함하지 않음
- `docs/release-notes/README.md`에 tag별 노트 입력 규칙 추가
- `scripts/README.md`에 package 명령과 산출물 정책 추가

## 검증

- `scripts/build_release.ps1 -SkipTests`: 성공
- `ctest --test-dir build/windows-x86 -C Release --output-on-failure -E re2dj_windows_vfs_runtime_probe`: 4/4 통과
- `build/windows-x86/bin/Release/re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only`: exit code 0
- `scripts/package_release.ps1 -Configuration Release -Version 0.0.39`: 성공
- `scripts/package_release.bat -Configuration Release -Version 0.0.39`: 성공
- package zip contents와 SHA256 sidecar 일치 확인
- `package_release.ps1` PowerShell parser syntax check 통과
- `git diff --check`: 통과

GitHub Actions의 실제 tag push와 원격 Release publish는 로컬에서 수행하지 않았다. 다음 릴리즈에서는 `VERSION`을 올리고 `docs/release-notes/v<version>.md`를 추가한 뒤 `v<version>` tag를 push하면 된다.

## English

### Result

Based on the public `nworkers/rePIU` `ci.yml`/`release.yml`, `re2DJ` now has a Windows x86 Release package and automatic GitHub Release flow. A `v*` tag push performs the version gate, Release build, verification, zip/checksum creation, artifact upload, and Release creation. A manual workflow run retains an artifact without creating a Release.

### Implementation

- Added `.github/workflows/release.yml` with tag push and `workflow_dispatch`, VERSION/tag comparison, Windows-2022 Release build and dependency cache, deterministic tests, `contents: write`, and `gh` Release create/edit/upload.
- Added `scripts/package_release.ps1`/`.bat`.
  - The package contains `re2dj.exe`, `re2dj_windows_injected_runtime.dll`, `config/`, and README/LICENSE/VERSION/RELEASE_NOTES.
  - It creates `re2dj-v<version>-windows-x86.zip` and a SHA256 sidecar.
  - Original HDD/CHD contents and game executable assets are excluded.
- Added the per-tag notes convention to `docs/release-notes/README.md` and package usage to `scripts/README.md`.
- Handwritten `docs/release-notes/<tag>.md` takes precedence over `--generate-notes`; an existing Release is updated with `--clobber`.

### Verification

- `scripts/build_release.ps1 -SkipTests`: passed
- Release CTest excluding the known product-lifecycle wait probe: 4/4 passed
- Release VFS enumeration probe: exit code 0
- Both PowerShell and batch package entry points: passed
- Package contents and SHA256 sidecar: matched
- PowerShell parser syntax check for `package_release.ps1`: passed
- `git diff --check`: passed

An actual remote tag push and Release publish were not performed locally. For the next release, bump `VERSION`, add `docs/release-notes/v<version>.md`, and push the matching `v<version>` tag.
